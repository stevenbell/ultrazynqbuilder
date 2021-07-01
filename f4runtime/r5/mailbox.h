#ifndef MAILBOX_H
#define MAILBOX_H

// WARNING: the size of an enum is not defined by the C standard, and may
// differ across platforms.

typedef enum {
  CAMERA0,
  CAMERA1,
  CAMERA2,
  CAMERA3,
  FLASH0,
  FLASH1,
  FLASH2,
  FLASH3,
  LENS0,
  LENS1,
  LENS2,
  LENS3,
  SHUTTERBUTTON,
  NO_DEVICE
} ZynqDevice;

typedef struct {
  uint32_t exposure;
} CameraParams;

typedef struct {
  uint32_t duration;
} FlashParams;

typedef struct {
  uint32_t reqId;
  ZynqDevice device;
  Time time;
  union{
    CameraParams camParams;
    FlashParams flashParams;
  };
} ZynqRequest;

#define QUEUE_LEN 10

/* Mailbox layout:
 * 0x00: APU pointer
 * 0x04: RPU pointer
 * 0x08: APU -> RPU buffers
 * ...
 * QUEUE_LEN*sizeof(Request) + 0x08 : APU pointer
 * QUEUE_LEN*sizeof(Request) + 0x0c : RPU pointer
 * QUEUE_LEN*sizeof(Request) + 0x10 : RPU -> APU buffers
 */

#define A2R_HEAD (0x00)
#define A2R_TAIL (0x04)
#define A2R_BUFFER_BASE (0x08)

#define R2A_HEAD (QUEUE_LEN*sizeof(ZynqRequest) + 0x08)
#define R2A_TAIL (QUEUE_LEN*sizeof(ZynqRequest) + 0x0c)
#define R2A_BUFFER_BASE (QUEUE_LEN*sizeof(ZynqRequest) + 0x10)

#endif
