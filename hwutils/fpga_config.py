#!/usr/bin/env python

from __future__ import print_function

import zipfile
import os
from lxml import etree
import sys


def unzip_hdf(src, dst):
    if not os.path.isfile(src):
        raise Exception(src + " does not exist")
    if not os.path.isdir(dst):
        raise Exception(dst + " does not exist")

    z = zipfile.ZipFile(src)
    z.extractall(dst)

def find_hdf(path):
    if os.path.isdir(path):
        # since a dir is given, we need to find the hdf file
        for filename in os.listdir(path):
            if filename.endswith(".hdf"):
                return filename
    elif os.path.isfile(path) and path.endswith(".hdf"):
        return path;
    else:
        raise Exception(path + " is not a valid path")

def find_hwh(dirname):
    # notice that there is a sysdef.xml in the dirname that we can parse
    sysdef = os.path.join(dirname, "sysdef.xml")
    if not os.path.isfile(sysdef):
        raise Exception(sysdef + " does not exist")
    root = etree.parse(sysdef)
    for path in root.iter("File"):
        attr = path.attrib
        if attr.get("BD_TYPE") and attr["BD_TYPE"] == "DEFAULT_BD":
            return os.path.join(dirname, attr["Name"])
    raise Exception(dirname + " is not valid")


def get_irqs(num_irq):
    # return a list of irqs currently available on the system
    # to avoid colission, it will try to assign any unused ports
    base_irq = 32
    search_from = 121
    used_irqs = []
    with open("/proc/interrupts") as f:
        for line in f.readlines():
            irq_str = line.split(":")[0].strip()
            if irq_str.isdigit():
                irq = int(irq_str)
                if irq >= search_from:
                    used_irqs.append(irq)
    result = []
    search_irq = search_from
    while search_irq < 256 and len(result) < num_irq:
        if search_irq not in used_irqs:
            result.append(search_irq - base_irq)
        search_irq += 1
    if len(result) < num_irq:
        raise Exception("cannot find " + str(num_irq) + " irq numbers")
    return result

def get_clks(elem):
    result = []
    for port in elem.iter("PORT"):
        port_attr = port.attrib
        if port_attr["SIGIS"] == "clk":
            result.append(port_attr["NAME"])
    return result

def parse_hwh(filename):
    # this might not be the most efficient way to parse since it went through
    # the tree multiple times
    root = etree.parse(filename)
    result = {"dma": [], "hls": []}
    # find the dma block
    for mod in root.iter("MODULE"):
        mod_attr = mod.attrib
        if mod_attr.get("MODTYPE") and mod_attr["MODTYPE"] == "axi_dma":
            # we have a dma block
            s2mm_datawidth = -1
            mm2s_datawidth = -1
            base_addr = -1
            high_addr = -1
            dma_channels = []
            module_name = mod_attr["INSTANCE"] # get the name first
            clks = get_clks(mod)

            # get control register offset and type
            for reg in mod.iter("REGISTER"):
                reg_attr = reg.attrib
                if reg_attr["NAME"] == "S2MM_DMACR"  or \
                   reg_attr["NAME"] == "MM2S_DMACR":
                    # get the address offset
                    address_offset = -1
                    for prop in reg.iter("PROPERTY"):
                        prop_attr = prop.attrib
                        if prop_attr["NAME"] == "ADDRESS_OFFSET":
                            str_val = prop_attr["VALUE"]
                            address_offset = int(str_val, 16)
                            break
                    if address_offset == -1:
                        raise Exception("address offset not found for " + \
                                        module_name)
                    channel_type = "xlnx,axi-dma-mm2s-channel" \
                                   if reg_attr["NAME"] == "MM2S_DMACR" else \
                                   "xlnx,axi-dma-s2mm-channel"
                    dma_channels.append((channel_type, address_offset))
            # get the data width values
            for param in mod.iter("PARAMETER"):
                param_attr = param.attrib
                if param_attr["NAME"] == "c_s_axis_s2mm_tdata_width":
                    str_val = param_attr["VALUE"]
                    s2mm_datawidth = int(str_val)
                elif param_attr["NAME"] == "c_m_axis_mm2s_tdata_width":
                    str_val = param_attr["VALUE"]
                    mm2s_datawidth = int(str_val)
                elif param_attr["NAME"] == "C_BASEADDR":
                    str_val = param_attr["VALUE"]
                    base_addr = int(str_val, 16)
                elif param_attr["NAME"] == "C_HIGHADDR":
                    str_val = param_attr["VALUE"]
                    high_addr = int(str_val, 16)
            if s2mm_datawidth < 0 or mm2s_datawidth < 0:
                raise Exception("data width not found for " + module_name)
            elif base_addr < 0 or high_addr < 0:
                raise Exception("reg address not found for " + module_name)

            # compute the actual reg size
            reg_size = high_addr - base_addr + 1

            # put them into the result
            module_result = {}
            module_result["mm2s_datawidth"] = mm2s_datawidth
            module_result["s2mm_datawidth"] = s2mm_datawidth
            module_result["base_addr"] = base_addr
            module_result["reg_size"] = reg_size
            module_result["dma_channels"] = dma_channels
            module_result["clks"] = clks
            module_result["module_name"] = module_name
            result["dma"].append(module_result)
        elif mod_attr.get("MODTYPE") and mod_attr["MODTYPE"] == "hls_target":
            # we have a hls block
            # get the name as well
            module_name = mod_attr["INSTANCE"]
            base_addr = -1
            high_addr = -1
            for param in mod.iter("PARAMETER"):
                param_attr = param.attrib
                if param_attr["NAME"] == "C_S_AXI_CONFIG_BASEADDR":
                    str_val = param_attr["VALUE"]
                    base_addr = int(str_val, 16)
                elif param_attr["NAME"] == "C_S_AXI_CONFIG_HIGHADDR":
                    str_val = param_attr["VALUE"]
                    high_addr = int(str_val, 16)
            if base_addr < 0 or high_addr < 0:
                raise Exception(module_name + " does not have address info")
            reg_size = high_addr - base_addr + 1

            module_result = {}
            module_result["base_addr"] = base_addr
            module_result["reg_size"] = reg_size
            module_result["module_name"] = module_name
            result["hls"].append(module_result)
    return result

def add_to_line(result, indent_num, val):
    # 4 space indent
    indent = "    "
    return result + indent * indent_num + val + "\n"


def sanity_check(hw_info):
    for dma in hw_info["dma"]:
        if dma["mm2s_datawidth"] != 0x10:
            print("mm2s datawidth is 0x{0:x}, expecting 0x{1:x}".format(
                  dma["mm2s_datawidth"], 16), file=sys.stderr)

        if dma["s2mm_datawidth"] != 0x10:
            print("s2mm datawidth is 0x{0:x}, expecting 0x{1:x}".format(
                  dma["s2mm_width"], 16), file=sys.stderr)
        if len(dma["clks"]) != 4:
            print("dma clks array len is {0}, expecting {1}".format(
                  len(dma["clks"]), 4), file=sys.stderr)
    if len(hw_info["hls"]) != 1:
        print("num of hls module found", len(hw_info["hls"]), "expecting",
              1, file=sys.stderr)

def generate_overlay(hw_info):
    # based on https://lkml.org/lkml/2014/5/28/280
    # may consider to make a dt builder
    result = ""
    result += "/plugin/;	"\
              "/* allow undefined label references and record them */\n"
    result += "/ {\n"

    # opening fragment
    result = add_to_line(result, 1, "fragment@0")
    # overlay to amba_pl element
    result = add_to_line(result, 2, "target = <&amba_pl>;")
    # start the overlay
    result = add_to_line(result, 2, "__overlay__ {")
    # loop through each module
    for dma in hw_info["dma"]:
        tag = "{0}: dma@0x{1:08x} ".format(dma["module_name"],
                                            dma["base_addr"]) + "{"
        # opening the dma element
        result = add_to_line(result, 3, tag)
        result = add_to_line(result, 4, "#dma-cells = <1>;")
        result = add_to_line(result, 4, "clock-names = \"" + \
                                        "\", \"".join(dma["clks"]) + "\";")
        result = add_to_line(result, 4, "clocks = " + \
                            ", ".join(["<&misc_clk_0>"
                                      for i in range(len(dma["clks"]))]) + ";")
        result = add_to_line(result, 4,
                             "compatible = \"xlnx,axi-dma-1.00.a\";")
        result = add_to_line(result, 4, "interrupt-parent = <&gic>;")

        # get interrupt numbers
        irqs = get_irqs(len(dma["dma_channels"]))
        irq_tag = " ".join(["0 {0} 4".format(irq) for irq in irqs])
        result = add_to_line(result, 4, "interrupts = <" + irq_tag + ">;")
        base_addr = dma["base_addr"]
        result = add_to_line(result, 4,
                             "reg = <0x0 0x{0:08x} 0x0 0x{1:x}>;".format(
                             base_addr, dma["reg_size"]));
        # all 32 bit
        result = add_to_line(result, 4, "xlnx,addrwidth = <0x20>;");
        result = add_to_line(result, 4, "xlnx,include-sg;");
        result = add_to_line(result, 4, "xlnx,multichannel-dma;")

        # enumerate through the channels
        for idx, chan in enumerate(dma["dma_channels"]):
            addr = base_addr + chan[1] # (type, offset)
            # opening dma channel
            result = add_to_line(result, 4,
                                 "dma-channel@{0:08x} ".format(addr) + "{")
            result = add_to_line(result, 5, "compatible = \"" + \
                                 chan[0] + "\";")
            result = add_to_line(result, 5, "dma-channels = <0x1>;")
            result = add_to_line(result, 5,
                                 "interrupts = <0 {0} 4>;".format(irqs[idx]));
            is_mm2s = "mm2s" in chan[0]
            result = add_to_line(result, 5,
                                 "xlnx,datawidth = <0x{0:x}>;".format(
                                 dma["mm2s_datawidth"] if is_mm2s else
                                 dma["s2mm_datawidth"]))
            result = add_to_line(result, 5, "xlnx,device-id = <0x0>;")
            # closing dma channel
            result = add_to_line(result, 4, "};")
        # closing dma element
        result = add_to_line(result, 3, "};")

    for hls in hw_info["hls"]:
        base_addr = hls["base_addr"]
        # opening hls element
        result = add_to_line(result, 3, "{0}: hls_target@{1:08x}".format(
                             hls["module_name"], base_addr) + " {")
        result = add_to_line(result, 4,
                             "compatible = \"xlnx,hls-target-1.0\";")
        result = add_to_line(result, 4,
                             "reg = <0x0 0x{0:08x} 0x0 0x{1:x}>;".format(
                             base_addr, hls["reg_size"]))
        # closing hls element
        result = add_to_line(result, 3, "};")

    # ending overlay
    result = add_to_line(result, 2, "};")
    # ending fragment
    result = add_to_line(result, 1, "};")
    # this is the end
    result += "};"
    return result

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("[Usage]:", sys.argv[0], "<project.sdk>", file=sys.stderr)

    hwh_file = find_hwh(sys.argv[1])

    hw_info = parse_hwh(hwh_file)
    sanity_check(hw_info)
    dts_str = generate_overlay(hw_info)
    print(dts_str)
