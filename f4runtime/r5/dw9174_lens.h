/*
 * dw9174_lens.h
 */

#ifndef DW9174_LENS_H
#define DW9174_LENS_H

#include "requestqueue.h"

#define I2C_FOCUS_ADDR 0x0c

typedef struct {
  ReqDevice reqId;
  unsigned int i2c_channel; // I2C mux channel, (0-3)
} DW9174_Config;

void dw9174_set_focus(DW9174_Config* config, u16 raw_value);
void dw9174_handle_requests(DW9174_Config* config);

#endif /* DW9174_LENS_H */
