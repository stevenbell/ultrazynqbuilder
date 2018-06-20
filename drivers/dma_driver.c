/**
 * @file driver.c
 * @authors:	Gedeon Nyengele <nyengele@stanford.edu>
 * 				
 * version 3.0
 * @brief DMA S2MM Driver 
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <asm/page.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>

#include "kbuffer.h"
#include "ioctl_cmds.h"
#include "common.h"
#include "dma.h"
#include "dma_help.h"
#include "dma_queue.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gedeon Nyengele");
MODULE_DESCRIPTION("DMA S2MM Driver");
MODULE_VERSION("3.0");

/* debug level */
const int debug_level = 4;

/* local driver data */
struct my_drvdata {
	int nOpens;
	dev_t dev_num;
	void __iomem *dev_base_addr;
	void *sg_base_addr;
	struct resource *irq_res;
	struct resource *reg_res;
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	wait_queue_head_t wq_frame;
	struct dma_queue_t processing_queue;
	struct dma_queue_t completed_queue;
};

/* global driver data */
#define CLASSNAME	"xilcam"
#define MAX_DEVS	32
static struct class *drvClass = NULL;
static atomic_t ndevs;
static dev_t global_dev_num;

/* CMA driver utils */
extern struct KBuffer *get_buffer_by_id(u32 id);

static void schedule_dma(struct my_drvdata *drvdata)
{
	struct dma_queue_item_t *hol;
	struct KBuffer *kbuf;

	if(!dma_help_is_idle(drvdata->dev_base_addr)){
		DEBUG("DMA is not idle; skipping scheduling for now");
		return;
	}
	
	hol = dma_queue_peek(&drvdata->processing_queue);
	if(hol == NULL) return;

	kbuf = get_buffer_by_id(hol->id);
	if(kbuf == NULL) return;

	dma_help_run_once(drvdata->dev_base_addr, drvdata->sg_base_addr, kbuf);
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	struct my_drvdata *drvdata;

	DEBUG("dev_open entry\n");
	
	drvdata = container_of(inodep->i_cdev, struct my_drvdata, cdev);
	
	// no support for multiple opens
	if(drvdata->nOpens++) return -EBUSY;

	// initialize DMA
	if(dma_help_init(drvdata->dev_base_addr)) return -EINVAL;

	// initialze the queues
	dma_queue_init(&drvdata->processing_queue);
	dma_queue_init(&drvdata->completed_queue);

	// initialize waitqueue
	init_waitqueue_head(&drvdata->wq_frame);

	// allocate space for SG descriptor
	drvdata->sg_base_addr = kmalloc(DESC_SIZE, GFP_KERNEL);
	if(drvdata->sg_base_addr == NULL) return -ENOMEM;
	
	// copy drvdata to filep
	filep->private_data = drvdata;

	DEBUG("dev_open exit\n");
	return 0;
}

static int dev_close(struct inode *inodep, struct file *filep)
{
	struct my_drvdata *drvdata;

	DEBUG("dev_close entry\n");

	drvdata = container_of(inodep->i_cdev, struct my_drvdata, cdev);
	dma_help_stop(drvdata->dev_base_addr);
	drvdata->nOpens = 0;
	if(drvdata->sg_base_addr) kfree(drvdata->sg_base_addr);
	filep->private_data = NULL;

	DEBUG("dev_close exit\n");
	return 0;
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long user_arg)
{
	struct UBuffer ubuf;
	struct KBuffer *kbuf_ptr;
	struct dma_queue_item_t *qitem;
	long status = 0;
	struct my_drvdata *drvdata;

	DEBUG("[dma-mod] dev_ioctl entry\n");

	drvdata = filep->private_data;
	if(!drvdata) {
		DEBUG("[dma-mod] ioctl: could not get driver data\n");
		return -ENOMEM;
	}

	switch(cmd) {
		case ENROLL_BUFFER:
			DEBUG("[dma-mod] ioctl [ENROLL_BUFFER]\n");
			if(access_ok(VERIFY_READ, (void *)user_arg, sizeof(struct UBuffer))) {
				status = copy_from_user(&ubuf, (void *)user_arg, sizeof(struct UBuffer));
				kbuf_ptr = get_buffer_by_id(ubuf.id);
				if(kbuf_ptr == NULL) {
					ERROR("[dma-mod] could not find kernel buffer with ID = %u\n", ubuf.id);
					status = -EINVAL;
				}
				else {
					qitem = kmalloc(sizeof(struct dma_queue_item_t), GFP_KERNEL);
					if(!qitem) {
						DEBUG("[dma-mod] failed to allocate memory. Line %d\n", __LINE__ - 2);
					}
					qitem->id = ubuf.id;
					dma_queue_enqueue(qitem, &drvdata->processing_queue);
					schedule_dma(drvdata);
					status = 0;
				}
			} else {
				status = -EIO;
			}			
		break;

		case WAIT_COMPLETE:
			DEBUG("[dma-mod] ioctl [WAIT_COMPLETE]\n");
			if(access_ok(VERIFY_READ, (void *)user_arg, sizeof(struct UBuffer))) {
				status = copy_from_user(&ubuf, (void *)user_arg, sizeof(struct UBuffer));				

				if((qitem = dma_queue_get(ubuf.id, &drvdata->completed_queue)) != NULL) {
					// remove and free item from completed queue
					dma_queue_delete(qitem->id, &drvdata->completed_queue);
					kfree(qitem);
					status = 0;
				}
				else if((qitem = dma_queue_get(ubuf.id, &drvdata->processing_queue)) != NULL) {
					// wait until item is moved to completed queue
					wait_event_interruptible(drvdata->wq_frame, (qitem = dma_queue_get(ubuf.id, &drvdata->completed_queue)) != NULL);
					dma_queue_delete(qitem->id, &drvdata->completed_queue);
					kfree(qitem);
					status = 0;
				}
				else {
					DEBUG("[dma-mod]: Buffer neither completed nor processing!");
					status = -EINVAL;
				}
			} else {
				DEBUG("[dma-mod]: Failed to access user pointer");
				status = -EIO;
			}
			break;

		case STATUS_CHECK:
			DEBUG("[dma-mod] ioctl [STATUS_CHECK]\n");
			if(access_ok(VERIFY_READ, (void *)user_arg, sizeof(struct UBuffer))) {
				status = copy_from_user(&ubuf, (void *)user_arg, sizeof(struct UBuffer));
				if(dma_queue_get(ubuf.id, &drvdata->completed_queue) != NULL) { status = DMA_COMPLETED; }
				else if(dma_queue_get(ubuf.id, &drvdata->processing_queue) != NULL) { status = DMA_IN_PROGRESS; } 
				else { status = -EINVAL; }
			} else {
				status = -EIO;
			}
			break;

		default:
			DEBUG("[dma-mod] un-supported ioctl [0x%08x]\n", cmd);
			status = -EINVAL;
		break;
	}

	DEBUG("[dma-mod] dev_ioctl exit\n");
	return status;
}

// interrupt handler for when a frame finishes
irqreturn_t frame_finished_handler(int irq, void *dev_id)
{
	struct my_drvdata *drvdata;
	drvdata = dev_id;

	// ackowledge interrupt
	dma_help_ack_all(drvdata->dev_base_addr);

	// stop DMA
	dma_help_stop(drvdata->dev_base_addr);

	return IRQ_WAKE_THREAD;
}

irqreturn_t frame_finished_threaded(int irq, void *dev_id)
{
	struct my_drvdata *drvdata;
	struct dma_queue_item_t *qitem;

	drvdata = dev_id;
	qitem = dma_queue_dequeue(&drvdata->processing_queue);

	if(qitem) {
		dma_queue_enqueue(qitem, &drvdata->completed_queue);
	}

	wake_up_interruptible(&drvdata->wq_frame);
	schedule_dma(drvdata);
	return IRQ_HANDLED;
}

// device operations callbacks
static struct file_operations fops = {
	.open			= dev_open,
	.release		= dev_close,
	.unlocked_ioctl	= dev_ioctl,
};


// device probe callback
static int dev_probe(struct platform_device *pdev)
{
	int status;
	struct my_drvdata *drvdata;
	DEBUG("[dma-mod] dev_probe entry\n");

	if(atomic_read(&ndevs) >= MAX_DEVS) {
		DEBUG("[dma-mod] driver can support only up to %d devices\n", MAX_DEVS);
		return -ENOMEM;
	}

	drvdata = kmalloc(sizeof(struct my_drvdata), GFP_KERNEL);
	if(!drvdata) {
		DEBUG("[dma-mod] failed to allocate memory for driver data\n");
		return -ENOMEM;
	}

	// get device interrupt and register interrupt callback
	DEBUG("[dma-mod] setting up irq handler\n");
	drvdata->irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(drvdata->irq_res == NULL) {
		ERROR("[dma-mod] IRQ lookup failed. Check device tree\n");
		return -ENODEV;
	}
	status = request_threaded_irq(drvdata->irq_res->start, frame_finished_handler, 
		frame_finished_threaded, 0, "xilcam", drvdata);
	DEBUG("[dma-mod] IRQ handler registered, device IRQ number is %llu\n", drvdata->irq_res->start);

	// dynamically get device's register space info
	DEBUG("[dma-mod] getting device register space\n");
	drvdata->reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(drvdata->reg_res == NULL) {
		if(drvdata->irq_res) {
			free_irq(drvdata->irq_res->start, NULL);
			drvdata->irq_res = NULL;
		}
		ERROR("[dma-mod] Register space lookup failed. Check device tree\n");
		return -ENODEV;
	}

	// we only need to ioremap a small chunk
	// so we'll remap a page
	drvdata->dev_base_addr = ioremap(drvdata->reg_res->start, PAGE_SIZE);
	if(drvdata->dev_base_addr == NULL) {
		if(drvdata->irq_res) {
			free_irq(drvdata->irq_res->start, NULL);
			drvdata->irq_res = NULL;
		}
		drvdata->reg_res = NULL;
		ERROR("[dma-mod] failed to ioremap device register space\n");
		return -ENODEV;
	}
	DEBUG("[dma-mod] device register space remapped successfully\n");
	
	// keep a record the platform_device pointer
	drvdata->pdev = pdev;

	// record device's MAJOR and MINOR
	drvdata->dev_num = MKDEV(MAJOR(global_dev_num), atomic_read(&ndevs));
	
	// create device node with udev
	drvdata->dev = device_create(drvClass, &pdev->dev, drvdata->dev_num,
						NULL, "%s%d", CLASSNAME, MINOR(drvdata->dev_num));
	
	// reset nOpens count
	drvdata->nOpens = 0;

	// save device data
	platform_set_drvdata(pdev, drvdata);
	dev_set_drvdata(drvdata->dev, drvdata);

	// initialize character device
	cdev_init(&drvdata->cdev, &fops);
	cdev_add(&drvdata->cdev, drvdata->dev_num, 1);

	// increment device count
	atomic_inc(&ndevs);

	DEBUG("[dma-mod] dev_probe exit\n");
	return 0;
}

static int dev_remove(struct platform_device *pdev)
{
	struct my_drvdata *drvdata;

	DEBUG("[dma-mod] dev_remove entry\n");
	drvdata = platform_get_drvdata(pdev);
	if(!drvdata) {
		DEBUG("[dma-mod] no device data found\n");
	} else {
		free_irq(drvdata->irq_res->start, drvdata);
		iounmap(drvdata->dev_base_addr);
		device_destroy(drvClass, drvdata->dev_num);
		cdev_del(&drvdata->cdev);
		kfree(drvdata);
	}
	atomic_dec(&ndevs);

	DEBUG("[dma-mod] dev_remove exit\n");
	return 0;
}

// driver match strings
static struct of_device_id dma_of_match[] = {
	{.compatible = "xilcam", },
	{},
};

// dma driver
static struct platform_driver dma_driver =  {
	.driver = {
		.name = "xilcam",
		.of_match_table = dma_of_match,
	},
	.probe = dev_probe,
	.remove = dev_remove,
};

// driver initialization and tear-down
static int __init dev_init(void)
{
	int status;

	DEBUG("[dma-mod] dev_init entry\n");

	atomic_set(&ndevs, 0);
	status = alloc_chrdev_region(&global_dev_num, 0, MAX_DEVS, CLASSNAME);
	if(status < 0) {
		DEBUG("[dma-mod] failed to alloc_chrdev_region\n");
		return status;
	}
	drvClass = class_create(THIS_MODULE, CLASSNAME);
	platform_driver_register(&dma_driver);

	DEBUG("[dma-mod] dev_init exit\n");
	return 0;
}

static void __exit dev_exit(void)
{
	DEBUG("[dma-mod] dev_exit entry\n");

	unregister_chrdev_region(MKDEV(MAJOR(global_dev_num), 0), MAX_DEVS);
	platform_driver_unregister(&dma_driver);
	class_destroy(drvClass);
	atomic_set(&ndevs, 0);

	DEBUG("[dma-mod] dev_exit exit\n");
}

module_init(dev_init);
module_exit(dev_exit);
