#include "i2c.h"
#include "platform.h"


/* defines for interrupt handler
 #define INTC_DEVICE_ID	XPAR_INTC_0_DEVICE_ID
 #define IIC_INTR_ID	XPAR_INTC_0_IIC_0_VEC_ID
 #define INTC			XIntc
 #define INTC_HANDLER	XIntc_InterruptHandler
*/


int stillSending = 0;
int irqCount = 0;
int handlerCount = 0;
XIic IicHandle;

static void sendHandler(XIic *instance)
{
  stillSending = 0;
}

static void statusHandler(XIic* instance, int event)
{
  if(event == XII_SLAVE_NO_ACK_EVENT){
    printf("No ack, continuing!\n");
  }
  else{
    printf("status: %d\n", event);
  }
}

// Catch the interrupts directly and count them
void catchIRQ(void* i2c)
{
  int prevCount = IicHandle.Stats.IicInterrupts;
  irqCount++;

//  u32 IntrPending = XIic_ReadIisr(IicHandle.BaseAddress);
//  u32 IntrEnable = XIic_ReadIier(IicHandle.BaseAddress);

  XIic_InterruptHandler(i2c);
  if(IicHandle.Stats.IicInterrupts == prevCount){
    printf("IRQ didn't increment count!\n");
    sendHandler(i2c);
  }
//  printf("pending: 0x%x\n", IntrPending);
//  printf("enabled: 0x%x\n", IntrEnable);

}


int i2c_init(void)
{
	int ok; // Check the return status of library calls
	XIic_Config *IicConfig;


	// Load the configuration and initialize the driver
	IicConfig = XIic_LookupConfig(IIC_DEVICE_ID);
	if (IicConfig == NULL) {
		return XST_FAILURE;
	}

	ok = XIic_CfgInitialize(&IicHandle, IicConfig, IicConfig->BaseAddress);
	if (ok != XST_SUCCESS) {
		return XST_FAILURE;
	}

  // Connect the I2C driver ISR
	//ok = connect_interrupt(XPAR_FABRIC_CAMERA_IIC_IIC2INTC_IRPT_INTR, XIic_InterruptHandler, &IicHandle);
  ok = connect_interrupt(XPAR_FABRIC_CAMERA_IIC_IIC2INTC_IRPT_INTR, catchIRQ, &IicHandle);
  if (ok != XST_SUCCESS) {
    return XST_FAILURE;
  }

  // Configure the interrupt callbacks
	XIic_SetSendHandler(&IicHandle, &IicHandle, (XIic_Handler) sendHandler);
  XIic_SetStatusHandler(&IicHandle, &IicHandle, (XIic_Handler) statusHandler);

	return XST_SUCCESS;
}

unsigned int i2c_write(u8 addr, u8 data[], unsigned int len)
{
  XIic_SetAddress(&IicHandle, XII_ADDR_TO_SEND_TYPE, addr);
  stillSending = 1;

  IicHandle.Stats.TxErrors = 0;

  int ok = XIic_Start(&IicHandle);
  if (ok != XST_SUCCESS) {
    printf("XIic_Start failed!\n");
    return XST_FAILURE;
  }

  ok = XIic_MasterSend(&IicHandle, data, len);
  if (ok != XST_SUCCESS) {
    printf("XIic_MasterSend failed!\n");
    return XST_FAILURE;
  }

  // Block until we've sent the data
  while (stillSending || (XIic_IsIicBusy(&IicHandle) == TRUE)) {
    if(IicHandle.Stats.TxErrors != 0){
      printf("Hmm, broke because of an error!\n");

      // Just try again
      ok = XIic_Start(&IicHandle);
      ok = XIic_MasterSend(&IicHandle, data, len);
      if(ok == XST_SUCCESS){
        IicHandle.Stats.TxErrors = 0; // All good now
      }
    }

  }

  ok = XIic_Stop(&IicHandle);
  if (ok != XST_SUCCESS) {
    printf("XIic_Stop failed!\n");
    return XST_FAILURE;
  }

  return XST_SUCCESS;
}

void i2c_set_mux(u8 channel)
{
	static u8 current_channel = 0; // Startup value of the mux
	if(channel != current_channel){
	  u8 muxval = 0b00000100 | channel;
    i2c_write(I2C_MUX_ADDR, &muxval, 1);
	  current_channel = channel;
	}
}

