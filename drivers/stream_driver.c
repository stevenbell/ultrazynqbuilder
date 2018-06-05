/**
 * @file driver.c
 * @authors:	Gedeon Nyengele <nyengele@stanford.edu>
 * 				
 * @date April 30 2018
 * version 1.0
 * @brief Driver for AXI Stream Traffic Generator
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

#include "ioctl_cmds.h"
#include "common.h"

#define CLASS_NAME "streamgen"
#define DEVICE_NAME "streamgen0"

#define DEV_BASE_ADDR	0x80010000
#define DEV_ADDR_RANGE	0x10000

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gedeon Nyengele");
MODULE_DESCRIPTION("Driver for AXI Stream Traffic Generator");
MODULE_VERSION("1.0");

const int debug_level = 4;

static int majorNumber;
static struct class *drvClass = NULL;
static struct device *drvDevice = NULL;

static unsigned char *dev_base_addr;
static unsigned nOpens = 0;

/** @brief callbacks for defined driver operations
 * implementation defined for open, release, and ioctl
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static long dev_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
	.open 			= dev_open,
	.release 		= dev_release,
	.unlocked_ioctl		= dev_ioctl,
};

static int __init dev_init(void)
{
	DEBUG("[streamgen]: dev_init entry\n");
	
	// map the register space
	DEBUG("[streamgen] attempting to ioremap device register space\n");
	dev_base_addr = ioremap(DEV_BASE_ADDR, DEV_ADDR_RANGE);
	if(dev_base_addr == NULL) {
		WARNING("[streamgen] could not ioremap device register space\n");
		return -ENODEV;
	}
	DEBUG("[stremgen] ioremap successfully completed\n");

	// dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNumber < 0) {
		WARNING("[streamgen]: failed to register a major number for device [%s]\n", DEVICE_NAME);
		return majorNumber;
	}
	DEBUG("[streamgen]: registered device [%s] with major number [%d]\n", DEVICE_NAME, majorNumber);

	// register the device class
	drvClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(drvClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[streamgen]: failed to register device class [%s]\n", CLASS_NAME);
		return PTR_ERR(drvClass);
	}
	DEBUG("[streamgen]: device class [%s] registered successfully\n", CLASS_NAME);

	// register the device driver
	drvDevice = device_create(drvClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(drvDevice)) {
		class_destroy(drvClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[streamgen]: failed to register device [%s]\n", DEVICE_NAME);
		return PTR_ERR(drvDevice);
	}
	DEBUG("[streamgen]: device driver registered successfully\n");

	return 0;
}

static void __exit dev_exit(void)
{
	device_destroy(drvClass, MKDEV(majorNumber, 0)); 	// remove the device driver
	class_unregister(drvClass); 							// unregister the device class
	class_destroy(drvClass); 							// remove the device class
	unregister_chrdev(majorNumber, DEVICE_NAME); 			// unregister the major number
	DEBUG("[streamgen]: device driver exited successfully\n");
}

// device open callback
static int dev_open(struct inode *inodep, struct file *filep)
{
	DEBUG("[streamgen]: dev_open entry\n");

	// currently not supporting multiple concurrent openings yet
	if(nOpens > 0) {
		WARNING("[streamgen]: dev_open : no support for multiple concurrent opens\n");
		return -EBUSY;
	}
	nOpens++;
	
	DEBUG("[streamgen]: dev_open exit\n");
	return 0; 
}

// device release (close)  callback
static int dev_release(struct inode *inodep, struct file *filep)
{
	DEBUG("[streamgen]: dev_close entry\n");

	// no support for multiple device closings
	// assume only one device opened
	nOpens = 0;

	DEBUG("[streamgen] dev_close exit\n");
	return 0;
}

// device ioctl callback
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	unsigned int reg_val;	

	DEBUG("[streamgen] dev_ioctl entry \n");
	DEBUG("[streamgen]: ioctl cmd 0x%08x | %lu (0x%0lx)\n", cmd, arg, arg);

	switch(cmd) {

		case RD_CONFIG:
			DEBUG("[streamgen]: ioctl [RD_CONFIG]\n");
			reg_val = ioread32((void *)(dev_base_addr + 0));
			if(access_ok(VERIFY_WRITE, (void *)arg, sizeof(unsigned int))) {
				copy_to_user((void *)arg, &reg_val, sizeof(unsigned int));				
			} else { return(-EIO); }
		break;
		case RD_SIZE:
			DEBUG("[streamgen]: ioctl [RD_SIZE]\n");
			reg_val = ioread32((void *)(dev_base_addr + 12));
			if(access_ok(VERIFY_WRITE, (void *)arg, sizeof(unsigned int))) {
				copy_to_user((void *)arg, &reg_val, sizeof(unsigned int));				
			} else { return(-EIO); }
		break;
		case RD_STATUS:
			DEBUG("[streamgen]: ioctl [RD_STATUS]\n");
			reg_val = ioread32((void *)(dev_base_addr + 16));
			if(access_ok(VERIFY_WRITE, (void *)arg, sizeof(unsigned int))) {
				copy_to_user((void *)arg, &reg_val, sizeof(unsigned int));				
			} else { return(-EIO); }
		break;
		
		case WR_CONFIG:
			DEBUG("[streamgen]: ioctl [WR_CONFIG]\n");
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(unsigned int))) {
				copy_from_user(&reg_val, (void *)arg, sizeof(unsigned int));
				iowrite32(reg_val, (void *)(dev_base_addr + 0));
			} else { return(-EIO); }
		break;
		case WR_CTRL_SET:
			DEBUG("[streamgen]: ioctl [WR_CTRL_SET]\n");
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(unsigned int))) {
				copy_from_user(&reg_val, (void *)arg, sizeof(unsigned int));
				iowrite32(reg_val, (void *)(dev_base_addr + 4));
			} else { return(-EIO); }
		break;
		case WR_CTRL_CLR:
			DEBUG("[streamgen]: ioctl [WR_CONFIG]\n");
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(unsigned int))) {
				copy_from_user(&reg_val, (void *)arg, sizeof(unsigned int));
				iowrite32(reg_val, (void *)(dev_base_addr + 8));
			} else { return(-EIO); }
		break;
		case WR_SIZE:
			DEBUG("[streamgen]: ioctl [WR_SIZE]\n");
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(unsigned int))) {
				copy_from_user(&reg_val, (void *)arg, sizeof(unsigned int));
				iowrite32(reg_val, (void *)(dev_base_addr + 12));
			} else { return(-EIO); }
		break;

		default:
			//unknown command
			return(-EINVAL);
		break; // default
	}
	DEBUG("[streamgen] dev_ioctl exit\n");
	return 0;
}

module_init(dev_init);
module_exit(dev_exit);
