#! /usr/bin/env python
from __future__ import print_function
import subprocess
import sys
import os
from collections import OrderedDict


def call_peek(address):
    if not os.path.isfile("./peek"):
        print("peek is not in pwd", file=sys.stderr)
        exit(1)
    output = subprocess.check_output("./peek " + address + " 6", shell=True)
    output = output.decode("ascii")
    # parse the output
    values = [int(entry.split(":")[-1], 16) for entry in [x for x in output.split("\n") if x]]
    return values

def decode_values(mem):
    assert(len(mem) == 6)
    mm2s_ctl = mem[0] # no use here
    mm2s_status = mem[1]
    current_dp = (mem[3] << 32) | (mem[2])
    tail_dp = mem[5] << 32 | mem[4]
    result = OrderedDict()
    hex_result = OrderedDict()

    halted = mm2s_status & 0x00000001
    idle = mm2s_status & 0x00000002
    sg_mode = mm2s_status & 0x00000008

    internal_err = mm2s_status & 0x00000010
    slave_err = mm2s_status & 0x00000020
    decode_err = mm2s_status & 0x00000040

    sg_internal_err = mm2s_status & 0x00000100
    sg_slave_err = mm2s_status & 0x00000200
    sg_decode_error = mm2s_status & 0x00000400

    ioc_irq = mm2s_status & 0x00001000
    delay_irq = mm2s_status & 0x00002000
    err_irq = mm2s_status & 0x00004000

    result["DMA Channel Halted"] = halted
    result["DMA Channel Idle"] = idle
    result["Scatter Gather Enabled"] = sg_mode
    result["DMA Internal Error"] = internal_err
    result["DMA Slave Error"] = slave_err
    result["DMA Decode Error"] = decode_err
    result["Scatter Gather Internal Error"] = sg_internal_err
    result["Scatter Gather Slave Error"] = sg_slave_err
    result["Scatter Gather Decode Error"] = sg_decode_error
    result["Interrupt on Complete"] = ioc_irq
    result["Interrupt on Delay"] = delay_irq
    result["Interrupt on Error"] = err_irq

    hex_result["Current Descriptor Pointer"] = current_dp

    return result, hex_result


def main():
    if len(sys.argv) != 2:
        print("Usage:", sys.argv[0], "<dma_address>")
        exit(1)
    address = sys.argv[1]
    # test if it's hex
    try:
        int(address, 16)
    except:
        print("Invalid address", address)
    mem = call_peek(address)

    bool_values, hex_values = decode_values(mem)
    for key in bool_values:
        print(key, ":", int(bool(bool_values[key])))
    for key in hex_values:
        print(key, ":", "0x{0:X}".format(hex_values[key]))


if __name__ == "__main__":
    main()
