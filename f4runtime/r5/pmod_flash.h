/* pmod_flash.h
 *
 * Control one or more LEDs on the GPIO pins of the UltraZed board.
 * These do not have any kind of PWM or dimming; they're on or off.
 */

#ifndef PMOD_FLASH_H
#define PMOD_FLASH_H

#include <stdbool.h>
#include "requestqueue.h"

#define PMOD_DATA_REG   0xFF0A0044
#define PMOD_MASKWR_REG 0xFF0A0008
#define PMOD_DIR_REG    0xFF0A0244
#define PMOD_OEN_REG    0xFF0A0248

#define PMOD_BIT_OFFSET 10 // Lowest bit is 11

typedef struct
{
  ReqDevice reqId;
  u8 pin;
  bool isOn;
  Time offTime; // If we're on, when to turn off
} pmod_flash_config;

void pmod_flash_init(pmod_flash_config* config);
void pmod_flash_handle_requests(pmod_flash_config* config);

#endif /* PMOD_FLASH_H */
