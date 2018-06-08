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

#include "dma_help.h"
#include "dma.h"

#define MAKE_RD_IOPTR(ptr) ((void *)(ptr))
#define MAKE_WR_IOPTR(ptr) ((void *)(ptr))

int dma_help_init(void *reg_space)
{
	unsigned long status;
	char *dma_addr;
	dma_addr = (char *) reg_space;

	// check if SG is included
	status = ioread32(MAKE_RD_IOPTR(dma_addr + S2MM_ASR));
	if((status & (1 << 3)) == 0) {
		return -1;
	}

	// reset DMA engine
	iowrite32(1 << 2, MAKE_WR_IOPTR(dma_addr + S2MM_ACR));
	while(ioread32(MAKE_RD_IOPTR(dma_addr + S2MM_ACR)) & (1 << 2));

	// SG control register setup
	iowrite32(0x00000F0F, MAKE_WR_IOPTR(dma_addr + S2MM_SG_CTRL));

	// reset pending flags
	iowrite32(1 << 12, MAKE_WR_IOPTR(dma_addr + S2MM_ASR));

	return 0;
}

void dma_help_run_once(void *reg_space, void *sg_table, struct KBuffer *buf)
{
	char *dma_addr;
	u32 *sg_addr;

	dma_addr = (char *) reg_space;	
	sg_addr = (u32 *) sg_table;

	// setup SG descriptor
	sg_addr[0] = (u32) virt_to_phys(sg_table) + 0x40; // Next descriptor
	sg_addr[1] = 0; // Upper 32 bits, not used
	sg_addr[2] = buf->phys_addr & 0xFFFFFFFF; // Data base address
	sg_addr[3] = 0; // Upper 32 bits, not used
	sg_addr[4] = 3 << AWCACHE_OFFSET;
	sg_addr[5] = (buf->xdomain.height << VSIZE_OFFSET) | (buf->xdomain.stride * buf->xdomain.depth);
	sg_addr[6] = buf->xdomain.width * buf->xdomain.depth;
	sg_addr[7] = 0;

	// Starting descriptor
	iowrite32(virt_to_phys(sg_table), MAKE_WR_IOPTR(dma_addr + S2MM_CURDESC_LSB));

	// set IRQThreshold, enable IOC interrupt, and run
	iowrite32((1 << 16) | (1 << 12) | 1, MAKE_WR_IOPTR(dma_addr + S2MM_ACR));

	// Tail descriptor, kicks off transaction
	iowrite32(virt_to_phys(sg_table), MAKE_WR_IOPTR(dma_addr + S2MM_TAILDESC_LSB));
} 

void dma_help_stop(void *reg_space)
{
	char *dma_addr;
	dma_addr = (char *) reg_space;
	iowrite32(0, MAKE_WR_IOPTR(dma_addr + S2MM_ACR));
}

void dma_help_ack_all(void *reg_space)
{
	char *dma_addr;
	dma_addr = (char *) reg_space;
	iowrite32(1 << 12, MAKE_WR_IOPTR(dma_addr + S2MM_ASR));
}

int dma_help_is_idle(void *reg_space)
{
	char *dma_addr;
	dma_addr = (char *) reg_space;
	
	return ioread32(MAKE_RD_IOPTR(dma_addr + S2MM_ASR)) & 0x3; // Either IDLE or HALTED bits are 1 means stopped
}
