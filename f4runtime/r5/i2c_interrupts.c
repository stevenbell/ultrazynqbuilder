#include "i2c.h"
#include "platform.h"
#include <stdlib.h>

// Eventually these global variables should be encapsulated in an object
// But since the system only has one I2C bus it works for now
typedef enum { IDLE, SENDING, COMPLETE } I2CState;
I2CState i2cState = IDLE;
XIic IicHandle;

typedef struct packet_t {
  u8 addr;
  u8 *data;
  size_t len;
  struct packet_t *next;
} Packet;

Packet *head = NULL;
Packet *lastPacket = NULL; // Last data transmitted, in case we need to re-transmit

u32* gpio_cam = (u32*)0x80000008;

static void sendHandler(XIic *instance)
{
  i2cState = COMPLETE; // Write transaction is finished, but HW still must be stopped
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
  XIic_InterruptHandler(i2c);
  // If somehow the interrupt handler ignored the interrupt, handle it ourselves
  // TODO: is this still necessary?
  if(IicHandle.Stats.IicInterrupts == prevCount){
    printf("IRQ didn't increment count!\n");
    sendHandler(i2c);
  }
}


unsigned int i2c_init(void)
{
	int ok;
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

  // Connect I2C interrupt to our ISR, which calls the I2C driver ISR
  ok = connect_interrupt(XPAR_FABRIC_CAMERA_IIC_IIC2INTC_IRPT_INTR, catchIRQ, &IicHandle);
  if (ok != XST_SUCCESS) {
    return XST_FAILURE;
  }

  // Configure the driver callbacks
	XIic_SetSendHandler(&IicHandle, &IicHandle, (XIic_Handler) sendHandler);
  XIic_SetStatusHandler(&IicHandle, &IicHandle, (XIic_Handler) statusHandler);

  i2cState = IDLE;

	return XST_SUCCESS;
}

// Enqueue a message to be transmitted on the I2C bus
// Switching the mux is an I2C message, so they stay in order without extra work
unsigned int i2c_write(u8 addr, u8 data[], unsigned int len)
{
  // Create the object and copy data
  Packet* p = (Packet*)malloc(sizeof(Packet));
  p->addr = addr;
  p->data = malloc(len);
  memcpy(p->data, data, len);
  p->len = len;
  p->next = NULL;

  // Walk the chain until we get to the end
  Packet** insertion = &head;
  while(*insertion != NULL){
    insertion = &((*insertion)->next);
  }
  *insertion = p;

  return XST_SUCCESS;
}


// Call this every iteration of the loop to tend the I2C driver
// This will make sure any failed transactions are retried, and that the driver
// stops the hardware and goes back to idle when finished
unsigned int i2c_check(void)
{
  int ok;

  if(i2cState == IDLE && head != NULL){

    // I2C is idle and there is something to send, so let's do it!
    if(lastPacket){
      free(lastPacket->data);
      free(lastPacket);
    }
    lastPacket = head;
    head = head->next;

    i2cState = SENDING;
    XIic_SetAddress(&IicHandle, XII_ADDR_TO_SEND_TYPE, lastPacket->addr);

    IicHandle.Stats.TxErrors = 0;
    int ok = XIic_Start(&IicHandle);
    if (ok != XST_SUCCESS) {
      printf("XIic_Start failed!\n");
      return XST_FAILURE;
    }

    ok = XIic_MasterSend(&IicHandle, lastPacket->data, lastPacket->len);
    if (ok != XST_SUCCESS) {
      printf("XIic_MasterSend failed!\n");
      return XST_FAILURE;
    }
  }
  // Currently still sending, check that there hasn't been an error and restart
  // if necessary
  else if(i2cState == SENDING || (XIic_IsIicBusy(&IicHandle) == TRUE)){
    if(IicHandle.Stats.TxErrors != 0){
      printf("Hmm, broke because of an error!\n");

      // Just try again
      ok = XIic_Start(&IicHandle);
      ok = XIic_MasterSend(&IicHandle, lastPacket->data, lastPacket->len);
      if(ok == XST_SUCCESS){
        IicHandle.Stats.TxErrors = 0; // All good now
      }
    }
    return XST_DEVICE_BUSY;
  }
  // Finished sending, stop the hardware
  else if(i2cState == COMPLETE) {
    ok = XIic_Stop(&IicHandle);
    if (ok != XST_SUCCESS) {
      printf("XIic_Stop failed!\n");
      return XST_FAILURE;
    }
    i2cState = IDLE;
  }
  // Otherwise, we're idle and have nothing to do, so do nothing.

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

