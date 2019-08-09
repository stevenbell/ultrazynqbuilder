
#include "pmod_flash.h"

void pmod_flash_init(pmod_flash_config* config)
{
  config->isOn = false;

  // Set the pin as output
  u32* dir = (u32*)PMOD_DIR_REG;
  *dir |= (1 << (PMOD_BIT_OFFSET + config->pin));

  // Enable the output
  u32* oen = (u32*)PMOD_OEN_REG;
  *oen |= (1 << (PMOD_BIT_OFFSET + config->pin));

  // Set the value to off
  u32* data = (u32*)PMOD_DATA_REG;
  *data &= ~(1 << (PMOD_BIT_OFFSET + config->pin));
}

void pmod_flash_handle_requests(pmod_flash_config* config)
{
  if(config->isOn){
    if(ttc_clock_now() >= config->offTime){
      // Set the value to off
      u32* data = (u32*)PMOD_DATA_REG;
      *data &= ~(1 << (PMOD_BIT_OFFSET + config->pin));
      config->isOn = false;
      //printf("flash %d off at %lld\n", config->pin, ttc_clock_now());
    }
  }
  else{ // Off, see if there is a request we need to service
    Request req = requestqueue_peek(config->reqId);
    if(req.device != NO_DEVICE && req.time <= ttc_clock_now()){ // TODO: + slack
      // If the next request is now (or really soon), execute it
      u32* data = (u32*)PMOD_DATA_REG;
      *data |= (1 << (PMOD_BIT_OFFSET + config->pin));
      config->offTime = req.time + req.flashParams.duration;
      config->isOn = true;

      //printf("flash %d on at %lld\n", config->pin, ttc_clock_now());
      requestqueue_pop(config->reqId);
    }
  }

}

