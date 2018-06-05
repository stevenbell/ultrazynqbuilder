#ifndef _XILCAM_DMA_H_
#define _XILCAM_DMA_H_

#define DESC_SIZE		0x40

/* VSIZE */
#define VSIZE_OFFSET	19
#define VSIZE_LEN		13
#define VSIZE_MAX		((1 << VSIZE_LEN) - 1)

/* STRIDE */
#define STRIDE_OFFSET	0
#define STRIDE_LEN		16
#define STRIDE_MAX		((1 << STRIDE_LEN) - 1)

/* HSIZE */
#define HSIZE_OFFSET	0
#define HSIZE_LEN		16
#define HSIZE_MAX		((1 << HSIZE_LEN) - 1)

/* MISC */
#define AWUSER_OFFSET	28
#define AWCACHE_OFFSET	24
#define RX_SOP_OFFSET	27
#define RX_EOP_OFFSET	26
#define TUSER_OFFSET	16
#define TID_OFFSET		8
#define TDEST_OFFSET	0

/* S2MM Register Space */
#define S2MM_SG_CTRL		0x2C
#define S2MM_OFFSET			0x30
#define S2MM_ACR			(S2MM_OFFSET + 0x00)
#define S2MM_ASR			(S2MM_OFFSET + 0x04)
#define S2MM_CURDESC_LSB	(S2MM_OFFSET + 0x08)
#define S2MM_CURDESC_MSB	(S2MM_OFFSET + 0x0C) 
#define S2MM_TAILDESC_LSB	(S2MM_OFFSET + 0x10)
#define S2MM_TAILDESC_MSB	(S2MM_OFFSET + 0x14)

#endif /* _XILCAM_DMA_H_ */
