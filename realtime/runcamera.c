/* readcamera.c
 * Test code for controlling the dual-camera system
 * This is largely copy-paste from camcontrol and "blinky"
 * If this is built to run along with Linux, be sure to set the USING_AMP
 * define, or else it'll try to initialize and shutdown the platform (both of
 * which will make you sad).
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xiic.h"
#include "xscugic.h"

#include "initcommands.h" // byte strings for initializing the camera

#define IIC_DEVICE_ID	XPAR_CAMERA_IIC_DEVICE_ID
#define IIC_BASEADDR	XPAR_CAMERA_IIC_BASEADDR

#define IIC_MUX_ADDR	0x70 // 0b1110000 [R/W]
#define IIC_CAM_ADDR	0x10 // Same for all the cameras

#define GPIO_LEDS   XPAR_AXI_GPIO_0_BASEADDR
#define GPIO_CAMERA (XPAR_AXI_GPIO_0_BASEADDR + 0x8)

#define DMA0_BASE XPAR_AXIDMA_0_BASEADDR
#define DMA1_BASE XPAR_AXIDMA_1_BASEADDR

#define DMA_MEM		0x60000000
#define DMA0_SG 	(DMA_MEM + 0x1000)
#define DMA1_SG 	(DMA_MEM + 0x2000)
#define DMA0_DATA	(DMA_MEM + 0x100000)
#define DMA1_DATA	(DMA_MEM + 0x200000)

#define CSIRX0	XPAR_CAM0_CSIRX_REGSPACE_S_AXI_BASEADDR
//#define CSIRX1	XPAR_AXI_CSI_1_S00_AXI_BASEADDR

u32 irqs_received = 0;

void csi_irq_handler(void* val)
{
	irqs_received++;
}

// Set the value of the mux to a particular channel
void set_mux(u8 channel)
{
	u8 muxval = 0b00000100 | channel;
	XIic_Send(IIC_BASEADDR, IIC_MUX_ADDR, &muxval, 1, XIIC_STOP);
}

s32 query_registers(XIic* i2c, uint16_t address, uint8_t nregs, uint8_t registers[])
{
	// Force the 16-bit address into an MSB-first array of two bytes
	uint8_t addrbytes[2];
	addrbytes[0] = address >> 8;
	addrbytes[1] = address & 0xff;

	// Send the (two-byte) address
	int bytes_sent = XIic_Send(IIC_BASEADDR, IIC_CAM_ADDR, addrbytes, 2, XIIC_STOP);
	if (bytes_sent == 0) {
		// TODO: we could probably retry instead
		return XST_FAILURE;
	}
	usleep(50);

	// Receive the number of requested bytes
	int bytes_rxed = XIic_Recv(IIC_BASEADDR, IIC_CAM_ADDR, registers, 2, XIIC_STOP);
	if (bytes_rxed != nregs) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}


void configure_dmarx(uint32_t dma_base, uint32_t sg_base, uint32_t data_base)
{
	// Create 1-byte pointers just to make the addressing cleaner below
	uint8_t* dma = (uint8_t*)dma_base;
	uint8_t* sg = (uint8_t*)sg_base;

    /* For 2D DMA transfer (uses scatter-gather) */
    uint32_t hsize = 1600;//38*3; // Each line is 1640x1232, minus 2px through demosaic
    uint32_t vsize = 1200;

    *(uint32_t*)(sg + 0x00) = (uint32_t)sg + 0x40; // Next descriptor (must be 16-word aligned)
    *(uint32_t*)(sg + 0x04) = 0; // Upper 32 bits of next descriptor addr are zero (for < 4GB RAM!)
    *(uint32_t*)(sg + 0x08) = data_base; // Data address
    *(uint32_t*)(sg + 0x0C) = 0; // Upper 32 bits
    *(uint32_t*)(sg + 0x10) = 0xff000000; // AWUSER = 0b1111, AWCACHE = 0b1111 to enable coherency
    *(uint32_t*)(sg + 0x14) = vsize << 19 | hsize; // [32 -- VSIZE -- 19 | 18 - reserved - 16 | 15 -- STRIDE -- 0]
    *(uint32_t*)(sg + 0x18) = hsize; // HSIZE
    *(uint32_t*)(sg + 0x1c) = 0; // SOP/EOP flags are set by DMA

    Xil_DCacheFlushRange((uint32_t)sg, 0x40 * 10); // Make sure the descriptor is in DRAM

    *(uint32_t*)(dma + 0x2C) = 0x00000f0f; // Enable coherency for SG transactions (ARUSER is 0b1111, dunno if it matters)
    *(uint32_t*)(dma + 0x30) = 0x00010001; // Enable DMA engine
    *(uint32_t*)(dma + 0x38) = (uint32_t)sg; // Target address
    *(uint32_t*)(dma + 0x3c) = 0; // Upper 32 bits
    *(uint32_t*)(dma + 0x40) = (uint32_t)sg; // Last descriptor (also the first, for now)
}

int main()
{
	uint32_t* leds = (uint32_t*)GPIO_LEDS;
	uint32_t* gpio_cam = (uint32_t*)GPIO_CAMERA;

#ifndef USING_AMP
	// Try enabling inner broadcasting so CCI sees CPU memory ops
	// This only works in a bare-metal context on R5 (if at all?)
	//Xil_Out32(0xFF41A040,0x3);

	// Enable snooping of APU caches from CCI
    //Xil_Out32(0xFD6E4000,0x1);
    //dmb();

	init_platform();
#endif

    // Turn off all the LEDs to start
    *leds = 0x00;

//    print("Enabling camera\n\r");
    *leds = 0x01;
    *gpio_cam = 0xff;

//    print("Blinking LEDS...\n\r");
    // Blink for 5 seconds
    for(int i = 0; i < 20; i++){
    	*leds = 0xaa;
    	usleep(100000);
    	*leds = 0x55;
    	usleep(100000);
    }

    // TODO: not sure if we need this if we're just playing fast and loose
    // with the low-level driver.
	XIic i2c;
	XIic_Config *config;
	int status;
	config = XIic_LookupConfig(IIC_DEVICE_ID);
	if (NULL == config) {
//		print("Failed to look up I2C config!\n\r");
		return XST_FAILURE;
	}

	status = XIic_CfgInitialize(&i2c, config, config->BaseAddress);
	if (status != XST_SUCCESS) {
//		print("Failed to initialize I2C!\n\r");
		return XST_FAILURE;
	}

	// Do a self-test, just because
	status = XIic_SelfTest(&i2c);
	if (status != XST_SUCCESS) {
//		print("I2C self-test failed\n\r");
		return XST_FAILURE;
	}

	// Try turning on all the cameras
	for(u8 cam = 0; cam < 4; cam++){
		*leds = 1 << cam;
		set_mux(cam);

		// Now talk to the camera
	    // We expect a response from the first couple queries
		uint8_t result[2];
		query_registers(&i2c, 0x0000, 2, result);
		usleep(1000);

		query_registers(&i2c, 0x0002, 1, result);
		usleep(1000);

		// Set a gazillion registers
		for(int i = 0; i < COMMANDS_LEN; i++){
			uint8_t* tx_bytes = byte_strings[i];
			int tx_len = tx_bytes[0]; // First byte is the length
			XIic_Send(IIC_BASEADDR, IIC_CAM_ADDR, tx_bytes+1, tx_len, XIIC_STOP);
		}
	}


	// Enable TTC0 counter 1 as our clock
	// Runs at 100MHz by default (rolls over after 42 seconds)
	uint32_t* clock = (uint32_t*)0xFF110018;
	uint32_t* clkctl = (uint32_t*)0xFF11000C;
	*clkctl = 0x00; // Start and disable interrupt-on-match

	// Try to schedule camera events to happen at a particular time
	// Pick a time in the future
	// Schedule dummy frames so we hit the time point
	// See how close we ended up
	uint32_t* csirx0 = (uint32_t*)CSIRX0;
	set_mux(3);

	// Set the COARSE_INTEGRATION_TIME register 0x015A
	uint8_t regvals[4] = {0x01,0x5A,0x00,0x00};

	uint16_t exposure_lines = 1;
	uint32_t last = *clock;

	uint32_t framecount = 0;

	//----------- IRQ testing
	const int IRQ_NUM = XPAR_FABRIC_CAM0_CSIRX_CSI_INTR_INTR; //125U;

	printf("Testing IRQ\r\n");
	XScuGic_Config* gic_cfg = XScuGic_LookupConfig(XPAR_PSU_RCPU_GIC_DEVICE_ID);
	XScuGic gic;
	int32_t ok;
	ok = XScuGic_CfgInitialize(&gic, gic_cfg, gic_cfg->CpuBaseAddress);
	if(ok != XST_SUCCESS){
		printf("Failed to initialize gic\r\n");
	}

	// Connect up the GIC
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) XScuGic_InterruptHandler,
			&gic);

	Xil_ExceptionEnable();

	// Priority is 8 (just below the max of 0), trigger is rising-edge
	XScuGic_SetPriorityTriggerType(&gic, IRQ_NUM, 0x08, 0b11);

	ok = XScuGic_Connect(&gic, IRQ_NUM, csi_irq_handler, NULL);
	if(ok != XST_SUCCESS){
		printf("Failed to connect irq\r\n");
	}

	XScuGic_Enable(&gic, IRQ_NUM);


	csirx0[0] = 0xf; // Interrupts on, run continuously
	csirx0[1] = 0xf; // Start running

	while(1){
		printf("IRQs received: %lu\r\n", irqs_received);
		csirx0[1] = 0xe; // Ack interrupts
		sleep(1);
	}

	//-----------

	// Start the receiver running to get frame 0
	csirx0[0] = 0; // Interrupts off; we'll just poll for now
	csirx0[1] = 0xf; // Ack any old interrupts, and run once
/*
	while(1){
		// Wait until the csirx finishes receiving a frame (frame N)
		while((csirx0[3] & 0x08) == 0){}

		// Record the current time
		uint32_t now = *clock;
		printf("%lu: %ld\r\n", framecount, (int32_t)(now / 100) - (last/100));
		last = now;

		// Grab the next frame from the CSI (frame N+1)
		// Frame N+1 had its settings locked in a while ago, and is exposing
		csirx0[1] = 0xf; // Ack any old interrupts, and run once

		// Wait for some configurable delay, up to the commit-point

		// Configure frame N+2
		// We want frames with a multiple of 10 to be longer
		if((framecount + 2) % 10 == 0){
			exposure_lines = 4000;
		}
		else{
			exposure_lines = 500;
		}
		regvals[2] = exposure_lines >> 8;
		regvals[3] = exposure_lines & 0xff;
		XIic_Send(IIC_BASEADDR, IIC_CAM_ADDR, regvals, 4, XIIC_STOP);

		framecount++;
	}
*/

	// Pick a target time (the start time of the next event from the queue)
	uint32_t target = *clock + 120*(1000*100); // 100MHz = 10ns increments
	int32_t inflight_time = 33327; // N-to-N+1 time of the frame currently in flight (us)

	while(1){
		// Wait until the csirx finishes receiving a frame (frame N)
		while((csirx0[3] & 0x08) == 0){}

		uint32_t now = *clock;
		int32_t delta_us = (int32_t)(target/100) - (now / 100); // Time from now, microseconds

		if(delta_us < 15*1000){
			// Less than 15ms left; this round is over
			xil_printf("error: %d us\r\n", delta_us);

			// Pick a new target time between 100 and 200ms from now
			target = *clock + ((rand() % 200) + 100) *(1000*100); // 100MHz = 10ns increments
			delta_us = (int32_t)(target/100) - (now / 100);
		}

		// TODO: Wait for some configurable delay, up to the commit-point

		uint32_t exposure_lines; // Duration of the exposure in terms of image line times

		if(delta_us - inflight_time < 15*1000){
			// This round will be over after the next frame, so configure a
			// short shot for the beginning of the next round
			exposure_lines = 10; // Anything less than ~2000 will be 33ms
			inflight_time = 33327;
		}
		else{
			// This round isn't going to be over any time soon; configure the
			// exposure to hit the target time
			int32_t exposure = delta_us - inflight_time; // How much after frame N+1
			exposure_lines = (exposure - 80) / 18.90;
			inflight_time = exposure;

			// Probably better to just throw away lots of frames rather than one big one...
			if(exposure_lines > 65535){
				exposure_lines = 65535;
				inflight_time = exposure_lines * 19; // TODO: replace with calculated max
			}
			else if(exposure_lines < 4){
				exposure_lines = 4;
				inflight_time = 33000;
			}
		}

		regvals[2] = exposure_lines >> 8;
		regvals[3] = exposure_lines & 0xff;

		XIic_Send(IIC_BASEADDR, IIC_CAM_ADDR, regvals, 4, XIIC_STOP);

		xil_printf("target: %d  time remaining: %d (%d), exposure %d\r\n", target, delta_us, delta_us - inflight_time, exposure_lines);

		// Grab the next frame from the CSI (frame N+1)
		// Frame N+1 had its settings locked in a while ago, and is exposing
		csirx0[1] = 0xf; // Ack any old interrupts, and run once
	}

/*
    // Clear memory
    for(uint32_t* i = (uint32_t*)DMA0_DATA; (int32_t)i<(DMA0_DATA+0x800000); i++){
    	*i = 0;
    }
    Xil_DCacheFlushRange((uint32_t)DMA0_DATA, 0x800000);

    while(1){
		// Set up a DMA transaction
		//configure_dmarx(DMA0_BASE, DMA0_SG, DMA0_DATA);
		configure_dmarx(DMA1_BASE, DMA1_SG, DMA1_DATA);

		// Start the HLS demosaicker
		uint32_t* demosaic = (uint32_t*)0x80060000;
		*demosaic = 0x01; // Run once

		// Start the CSI receiver
		uint32_t* csirx0 = (uint32_t*)CSIRX0;
		csirx0[0] = 0x0e; // Enable all interrupts
		csirx0[1] = 0x3; // Ack any old interrupts, and run once

		// Wait a bit
		usleep(1000*50);

		// Invalidate the cache
		Xil_DCacheInvalidateRange((uint32_t)DMA1_DATA, 0x800000);

		for(int i = 0; i < 10; i++){
			printf("%lx\n", ((uint32_t*)DMA1_DATA)[i]);
		}

    }
*/

#ifndef USING_AMP
    cleanup_platform();
#endif
    return 0;
}
