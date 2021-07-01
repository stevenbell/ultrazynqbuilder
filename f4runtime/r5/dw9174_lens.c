
#include "i2c.h"
#include "ttc_clock.h"
#include "dw9174_lens.h"

/* Set the focus position of the lens
 * @param raw_value: a 10-bit number which translates to the lens position
 */
void dw9174_set_focus(DW9174_Config* config, u16 raw_value)
{
  i2c_set_mux(config->i2c_channel);

  // VCM controller has only two configuration bytes
  // [ Power down | FLAG | data (10 bits) | slope control (2 bits) | step period (2 bits) ]
  u8 control[2];
  control[0] = (raw_value >> 4) & 0x3F; // Leave PD and FLAG as 0, write 6/10 bits
  control[1] = (raw_value << 4) & 0xF0; // Bottom 4/10 bits, leave control bits 0
  i2c_write(I2C_FOCUS_ADDR, control, 2);
}

void dw9174_handle_requests(DW9174_Config* config)
{
  ZynqRequest req = requestqueue_peek(config->reqId);
  if(req.device != NO_DEVICE && req.time <= ttc_clock_now()){ // TODO: + slack
    printf("move lens %d on at %lld\n", config->i2c_channel, ttc_clock_now());
    requestqueue_pop(config->reqId);
  }
}
