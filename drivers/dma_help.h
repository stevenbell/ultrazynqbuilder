#ifndef __MY_DMA_HELP_H__
#define __MY_DMA_HELP_H__

#include "kbuffer.h"

/* initialze the DMA
 * @param reg_space The iomapped pointer
 *        to DMA's register address space
 * @return 0 if succeeded to init DMA, non-zero otherwise
 */
int dma_help_init(void *reg_space);

/* set DMA up for one 2D transfer
 * @param reg_space The iomapped pointer to DMA register address space
 * @param sg_table Kernel virtual address of SG descriptor (allocated with kmalloc)
 * @param buf Kernel virutal address of buffer to fill
 */
void dma_help_run_once(void *reg_space, void *sg_table, struct KBuffer *buf);

/* stop the DMA
 * @param reg_space The iomapped pointer
 *        to DMA's register address space
 */
void dma_help_stop(void *reg_space);

/* acknowledge all pending interrupts
 * @param reg_space The iomapped pointer
 *        to DMA's register address space
 */
void dma_help_ack_all(void *reg_space);

/* check whether DMA is halted or not
 * @param reg_space The iomapped pointer
 *        to DMA's register address space
 * @return 0 if DMA is running, non-zero value otherwise
 */
int dma_help_is_idle(void *reg_space);

#endif /* __MY_DMA_HELP_H__ */
