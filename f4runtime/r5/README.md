# Build instructions

* In the Xilinx SDK, crate a new project
    - Select R5 core, standalone
    - Use an existing BSP or create a new one

* Edit the linker script (`lscript.ld`)
    - RAM address should be 0x3e000000, length 0x100000 (1MB).  This must match the device tree.
    - The TCM addresses should be 0x0000_0000 and 0x0002_0000
    - The vectors should be on the TCM, everything else should be in RAM

* Build libmetal
    - The libmetal version included with 2017.2 is too old (doesn't include the `metal_io_block_read()` function.  Anything from 2017.3 or newer should work.
    - Follow the cmake build instructions to build libmetal for ZynqMP-R5.  You'll need to edit the platform file with a `-I` directive to point to the header path of your BSP.
    - Copy `libmetal.a` and the `include` directory into the runtime project (or some other convenient location).
    - Add the `include` directory, the library name (`metal`), and the library search path in the project build settings.

