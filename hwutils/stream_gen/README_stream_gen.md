# Simple AXI Stream Generator
This custom AXI stream generator emulates our CSI RX block.
Since the CSI RX block has a 64-bit output (4 16-bit pixels), this stream
generator should also be used to produce output with the same width.

## Kernel Driver
kernel driver files can be found under the folder `kernel`.

Notes: 
- One needs to manually change the `DEV_BASE_ADDR` and `DEV_ADDR_RANGE` parameters
in the driver source file to match the base address and range of the stream generator's
register address space (driver code does not read these parameters from device tree).
- The Makefile provided is to be used within the petalinux environment. To build the driver 
out of the petalinux environment, one has to modify the Makefile to point to the kernel sources.
- the kernel driver registers the device under the name `/dev/streamgen0`

## User-Level driver
user-level code (with an example) is provided under the `user-land` folder.

## device registers
The stream generator has 5 registers: `CONFIG, CTRL_SET, CTRL_CLR, SIZE, STATUS`
1. `CONFIG` (reset value = 0x00000000)
- bit [0] = Mode (1 = run continuous, 0 = Run once and Stop)
- bit [1] = Interrupt Enable (1 = Enable Interrupts, 0 = disable interrupts)
- bit [2] = TLAST signal disable (1 = TLAST signal is disabled, 0 = TLAST signal enabled)
- bit [3] to bit [31]: RESERVED

2. `CTRL_SET` (reset value = 0x00000000)
- bit [0] = RUN (1 = start run, 0 is ignored)
- bit [1] to bit [31] = RESERVED

3. `CTRL_CLR` (reset value = 0x00000000)
- bit [0] = Stop (1 = request stop, 0 is ignored)
- bit [1] = interrupt acknowledge (1 = ack interrupt, 0 is ignored)
- bit [2]  to bit [31] = RESERVED

4. `SIZE` (reset value = 0x00000000)
this register should contain the number of 2-byte values to output.
For proper functionality consistent with our CSI RX block, this register should be 
set to a value of at least 4 (this means 4 2-byte values).

5. `STATUS` (reset value = 0x00000000)
- bit [0] = Run state (1 = running, 0 = stopped)
- bit [1] = interrupt state (1 = pending interrupt, 0 = no interrupts pending)
- bit [2]  to bit [31] = RESERVED

## Output of Stream Generator
Assuming that the`SIZE` register is set to `N`, then
the stream generator will output `N` 16-bit values (corresponding to pixel values)
starting from 1 to `N`.

Example: if N = 8, the device will output the following 8 16-bit values:

0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008

For comments or questions, please contact @thenextged `nyengele@stanford.edu`
