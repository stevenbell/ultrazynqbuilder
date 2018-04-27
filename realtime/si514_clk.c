/* si514_clk.c
 * Control routines for Silicon Labs Si514 programmable oscillator, which
 * is used as a reconfigurable clock for the camera.
 */

#include "si514_clk.h"
#include "i2c.h"

void si514_init()
{
	// Set the clock to 24 MHz
	// This is a large frequency change (not just fine-tuning), so follow page
	// 14 on the Si514 datasheet
	u8 clkreg[2];

	// First disable the output
	clkreg[0] = 132;
	clkreg[1] = 0;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	// 0x0 [ LP1[3:0] | LP2[3:0] ]
	u8 LP1 = 2;
	u8 LP2 = 3;

	clkreg[0] = 0x00;
	clkreg[1] = LP1 << 4 | (LP2 & 0xF);
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	// M_Frac and M_Int
	u32 M_Frac = 22159775;
	u16 M_Int = 66;

	clkreg[0] = 0x05; // 0x05 [ M_Frac [7:0] ]
	clkreg[1] = M_Frac & 0xFF;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	clkreg[0] = 0x06; // 0x06 [ M_Frac [15:8] ]
	clkreg[1] = (M_Frac >> 8) & 0xFF;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	clkreg[0] = 0x07; // 0x07 [ M_Frac [23:16] ]
	clkreg[1] = (M_Frac >> 16) & 0xFF;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	clkreg[0] = 0x08; // 0x08 [ M_Int [2:0] | M_Frac [28:24] ]
	clkreg[1] = ((M_Int << 5) & 0xFF) | (M_Frac >> 24);
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	clkreg[0] = 0x09; // 0x09 [ M_Int [8:3] ]
	clkreg[1] = (M_Int >> 3);
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	// HS_DIV and LS_DIV
	u16 HS_DIV = 88;
	u8 LS_DIV = 0;

	clkreg[0] = 0x0A; // 0x0A [ HS_DIV [7:0] ]
	clkreg[1] = HS_DIV & 0xFF;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	clkreg[0] = 0x0B; // 0x0B [ 0 | LS_DIV [2:0] | 00 | HS_DIV [9:8] ]
	clkreg[1] = (LS_DIV << 4) | (HS_DIV >> 8);
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	// Run frequency calibration to set new frequency
	clkreg[0] = 132;
	clkreg[1] = 0x01;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);

	// Enable the output again
	clkreg[0] = 132;
	clkreg[1] = 0x04;
	XIic_Send(IIC_BASEADDR, SI514_IIC_ADDR, clkreg, 2, XIIC_STOP);
}


