#ifndef _CMA_H_
#define _CMA_H_

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "ubuffer.h"
#include "ioctl_cma.h"

/* get cma driver to map the requested region
 * @param buf The buffer region to map from
 * @param addr The user space virtual address where the mapping should start (@see mmap(2))
 * @param length The length of the mapping
 * @param prot (@see mmap(2))
 * @param flags (@see mmap(2))
 * @param fd (@see mmap(2))
 * @param offset (@see mmap(2))
 * @return Pointer to mapped area on success, MAP_FAILED on failure
 */
void *cma_map(struct UBuffer *buf, void *addr, size_t length, int prot, int flags, int fd, off_t offset);

/* get cma driver to delete the mapping for the specified address range
 * @see mmap(2)
 * @return 0 on success, -1 on failure (errno set to cause of error)
 */
int cma_unmap(void *addr, size_t length);

/* get a buffer from cma driver
 * @param fd The cma driver file descriptor
 * @param buf Pointer to pre-filled buffer
 * @return 0 if successful, non-zero number otherwise
 */
int cma_get_buffer(int fd, struct UBuffer *buf);

/* get cna driver to free a buffer
 * @param fd The cma driver file descriptor
 * @param buf Pointer to buffer previously returned by cma driver
 * @return 0 if successful, non-zero number otherwise
 */
int cma_free_buffer(int fd, struct UBuffer *buf);

#endif /* _CMA_H_ */
