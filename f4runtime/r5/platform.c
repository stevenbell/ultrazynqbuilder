#include "xparameters.h"
#include "platform.h"

XScuGic gic;

/* Perform system-wide initialization */
void init_platform()
{
  // Set up interrupts
  XScuGic_Config* gic_cfg = XScuGic_LookupConfig(XPAR_PSU_RCPU_GIC_DEVICE_ID);
  s32 ok = XScuGic_CfgInitialize(&gic, gic_cfg, gic_cfg->CpuBaseAddress);
  if(ok != XST_SUCCESS){
    printf("Failed to initialize gic\r\n");
  }

  // Connect up the R5 GIC to receive interrupts
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
      (Xil_ExceptionHandler) XScuGic_InterruptHandler,
      &gic);
  Xil_ExceptionEnable();
}

// We run forever, so there's no cleanup...
void cleanup_platform()
{ }

s32 connect_interrupt(u32 irq_id, Xil_InterruptHandler handler, void* irq_data)
{
  // Priority is 8 (just below the max of 0), trigger is rising-edge
  XScuGic_SetPriorityTriggerType(&gic, irq_id, 0x08, 0b11);

  s32 ok = XScuGic_Connect(&gic, irq_id, handler, irq_data);
  if(ok == XST_SUCCESS){
    XScuGic_Enable(&gic, irq_id);
  }
  return ok;
}
