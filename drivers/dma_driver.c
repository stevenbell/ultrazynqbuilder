/**
 * @file driver.c
 * @authors:	Gedeon Nyengele <nyengele@stanford.edu>
 * 				
 * version 2.0
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

#include "kbuffer.h"
#include "ioctl_cmds.h"
#include "common.h"
#include "dma.h"
#include "dma_help.h"
#include "dma_queue.h"

#define CLASS_NAME "xilcam"
#define DEVICE_NAME "xilcam0"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gedeon Nyengele");
MODULE_DESCRIPTION("DMA S2MM Driver");
MODULE_VERSION("2.0");

const int debug_level = 4;

static int majorNumber;
static struct class *drvClass = NULL;
static struct device *drvDevice = NULL;

static unsigned nOpens = 0;

// driver internal state variables
struct resource* irq_res = NULL; // interrupt
struct resource* reg_res = NULL; // register space
static char *dev_base_addr = NULL; // base address for register space
static char *sg_base_addr = NULL; // base address for SG descritor table

// wait queue
DECLARE_WAIT_QUEUE_HEAD(wq_frame);

// the queues
static struct dma_queue_t processing_queue;
static struct dma_queue_t completed_queue;

// CMA driver utils
extern struct KBuffer *get_buffer_by_id(u32 id);

static void schedule_dma(void)
{
	struct dma_queue_item_t *hol;
	struct KBuffer *kbuf;

	if(!dma_help_is_idle((void *) dev_base_addr)) return;
	
	hol = dma_queue_dequeue(&processing_queue);
	if(hol == NULL) return;

	kbuf = get_buffer_by_id(hol->id);
	if(kbuf == NULL) return;

	dma_help_run_once((void *) dev_base_addr, (void *) sg_base_addr, kbuf);
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	// no support for multiple opens
	if(nOpens++) return -EBUSY;

	// initialize DMA
	if(dma_help_init((void *) dev_base_addr) != 0) return -EINVAL;

	// initialze the queues
	dma_queue_init(&processing_queue);
	dma_queue_init(&completed_queue);

	// allocate space for SG descriptor
	sg_base_addr = kmalloc(DESC_SIZE, GFP_KERNEL);
	if(sg_base_addr == NULL) return -ENOMEM;

	return 0;
}

static int dev_close(struct inode *inodep, struct file *filep)
{
	dma_help_stop((void *) dev_base_addr);
	nOpens = 0;
	if(sg_base_addr) kfree(sg_base_addr);
	return 0;
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long user_arg)
{
	struct UBuffer ubuf;
	struct KBuffer *kbuf_ptr;
	struct dma_queue_item_t *qitem;
	long status = 0;

	DEBUG("[dma-mod] dev_ioctl entry\n");

	switch(cmd) {
		case ENROLL_BUFFER:
			DEBUG("[dma-mod] ioctl [ENROLL_BUFFER]\n");
			if(access_ok(VERIFY_READ, (void *)user_arg, sizeof(struct UBuffer))) {
				copy_from_user(&ubuf, (void *)user_arg, sizeof(struct UBuffer));
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
					dma_queue_enqueue(qitem, &processing_queue);
					schedule_dma();
					status = 0;
				}
			} else {
				status = -EIO;
			}			
		break;

		case WAIT_COMPLETE:
			DEBUG("[dma-mod] ioctl [WAIT_COMPLETE]\n");
			if(access_ok(VERIFY_READ, (void *)user_arg, sizeof(struct UBuffer))) {
				copy_from_user(&ubuf, (void *)user_arg, sizeof(struct UBuffer));				
				if((qitem = dma_queue_get(ubuf.id, &completed_queue)) != NULL) {
					// remove and free item from completed queue
					dma_queue_delete(qitem->id, &completed_queue);
					kfree(qitem);
					status = 0;
				}
				else if((qitem = dma_queue_get(ubuf.id, &processing_queue)) != NULL) {
					// wait until item is moved to completed queue
					wait_event_interruptible(wq_frame, (qitem = dma_queue_get(ubuf.id, &completed_queue)) != NULL);
					dma_queue_delete(qitem->id, &completed_queue);
					kfree(qitem);
					status = 0;
				}
				else { status = -EINVAL; }
			} else {
				status = -EIO;
			}
			break;

		case STATUS_CHECK:
			DEBUG("[dma-mod] ioctl [STATUS_CHECK]\n");
			if(access_ok(VERIFY_READ, (void *)user_arg, sizeof(struct UBuffer))) {
				copy_from_user(&ubuf, (void *)user_arg, sizeof(struct UBuffer));
				if(dma_queue_get(ubuf.id, &completed_queue) != NULL) { status = DMA_COMPLETED; }
				else if(dma_queue_get(ubuf.id, &processing_queue) != NULL) { status = DMA_IN_PROGRESS; } 
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
	// ackowledge interrupt
	dma_help_ack_all((void *) dev_base_addr);

	// stop DMA
	dma_help_stop((void *) dev_base_addr);

	// dispath "new_frame" event
	//atomic_set(&new_frame, 1);
	//wake_up_interruptible(&wq_frame);
	return IRQ_WAKE_THREAD;
}

irqreturn_t frame_finished_threaded(int irq, void *dev_id)
{
	struct dma_queue_item_t *qitem;
	qitem = dma_queue_dequeue(&processing_queue);

	if(qitem) {
		dma_queue_enqueue(qitem, &completed_queue);
	}

	wake_up_interruptible(&wq_frame);
	schedule_dma();
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
	DEBUG("[dma-mod] dev_probe entry\n");

	// get device interrupt and register interrupt callback
	DEBUG("[dma-mod] setting up irq handler\n");
	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(irq_res == NULL) {
		ERROR("[dma-mod] IRQ lookup failed. Check device tree\n");
		return -ENODEV;
	}
	status = request_threaded_irq(irq_res->start, frame_finished_handler, 
		frame_finished_threaded, 0, "xilcam", NULL);
	DEBUG("[dma-mod] IRQ handler registered, device IRQ number is %llu\n", irq_res->start);

	// dynamically get device's register space info
	DEBUG("[dma-mod] getting device register space\n");
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(reg_res == NULL) {
		if(irq_res) {
			free_irq(irq_res->start, NULL);
			irq_res = NULL;
		}
		ERROR("[dma-mod] Register space lookup failed. Check device tree\n");
		return -ENODEV;
	}

	// we only need to ioremap a small chunk
	// so we'll remap a page
	dev_base_addr = ioremap(reg_res->start, PAGE_SIZE);
	if(dev_base_addr == NULL) {
		if(irq_res) {
			free_irq(irq_res->start, NULL);
			irq_res = NULL;
		}
		reg_res = NULL;
		ERROR("[dma-mod] failed to ioremap device register space\n");
		return -ENODEV;
	}
	DEBUG("[dma-mod] device register space remapped successfully\n");

	// dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNumber < 0) {
		WARNING("[dma-mod]: failed to register a major number for device [%s]\n", DEVICE_NAME);
		return majorNumber;
	}
	DEBUG("[dma-mod]: registered device [%s] with major number [%d]\n", DEVICE_NAME, majorNumber);

	// register the device class
	drvClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(drvClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[dma-mod]: failed to register device class [%s]\n", CLASS_NAME);
		return PTR_ERR(drvClass);
	}
	DEBUG("[dma-mod]: device class [%s] registered successfully\n", CLASS_NAME);

	// register the device driver
	drvDevice = device_create(drvClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(drvDevice)) {
		class_destroy(drvClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[dma-mod]: failed to register device [%s]\n", DEVICE_NAME);
		return PTR_ERR(drvDevice);
	}
	
	DEBUG("[dma-mod]: device driver registered successfully\n");
	DEBUG("[dma-mod] dev_probe exit\n");
	return 0;
}

static int dev_remove(struct platform_device *pdev)
{
	DEBUG("[dma-mod] dev_remove entry\n");
	if(irq_res) {
		free_irq(irq_res->start, NULL);
		irq_res = NULL;
	}
	if(dev_base_addr) {
		iounmap(dev_base_addr);
		dev_base_addr = NULL;
		reg_res = NULL;
	}

	device_destroy(drvClass, MKDEV(majorNumber, 0)); 	// remove the device driver
	class_unregister(drvClass); 							// unregister the device class
	class_destroy(drvClass); 							// remove the device class
	unregister_chrdev(majorNumber, DEVICE_NAME); 			// unregister the major number
	DEBUG("[dma-mod]: device driver exited successfully\n");
	DEBUG("[dma-mod] dev_remove exit \n");
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

// register platform DMA driver
module_platform_driver(dma_driver);
