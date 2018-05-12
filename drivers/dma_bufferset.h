/* based on the original dma_bufferset.h.mako
 */

#ifndef _DMA_BUFFERPAIR_H_
#define _DMA_BUFFERPAIR_H_

#include <linux/semaphore.h>
#include <linux/device.h>

#include "buffer.h"

struct chan_buf {
        unsigned long *sg;          /* memory for SG table */
        unsigned long sg_phys;     /* physical address of SG table */
        Buffer buf;

        int chan_id;
};

typedef struct BufferSet {
  int id;
  /* each input/output stream is allocated separately and shuffled around.
   * we have full copies since pointers we're handed may not persist.
   */
  struct chan_buf *chan_buf_list;

  unsigned long output_sg_phys; // Physical address of SG table
  // TODO: what if the tap width is zero?
  //unsigned char tap_vals[$//{tapwidth}]; // Data that describes the tap state
  struct BufferSet* next; // Next buffer set in chain, if any
  /* length of chan_buf_list, i.e., number of channels */
  int nr_channels;
} BufferSet;


typedef struct BufferList{
  BufferSet* head;
  BufferSet* tail;
  struct semaphore mutex;
} BufferList;


void buffer_initlist(BufferList *list);
void buffer_enqueue(BufferList* list, BufferSet* buf);
BufferSet* buffer_dequeue(BufferList* list);
BufferSet* buffer_dequeueid(BufferList* list, int id);
bool buffer_listempty(BufferList* list);
bool buffer_hasid(BufferList* list, int id);

#endif
