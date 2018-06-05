# Drivers for the UltraZynqBuilder project
## CMA driver
This driver must be loaded before any other drivers are loaded because most other drivers use symbols defined by the CMA driver.

The CMA driver provides a mechanism to:
1. request contiguous DMA-able memory chunks from the OS
2. manipulate those memory chunks using simple IOCTL commands and mmap calls
3. isolate the user's view of a memory chunk from the kernel's view by providing a `struct Ubuffer` and `struct KBuffer`.
The user only sees the limited `struct Ubuffer`

In the user land, a wrapper is provided so that low-level IOCTL commands are hidden. This functionality is found in the following files: 
`cma.h` and `cma.c`

## DMA driver
- implements a S2MM channel driver
- expects a single `M_AXIS` input stream interface to the hardware DMA engine
- automatically binds to DMA compatibility string `xilcam`

## Stream Gen Driver
- controls the custom AXI stream generator
- provides user-land wrapper

Note: example test code for all of these drivers is provided in the `utils` directory.
