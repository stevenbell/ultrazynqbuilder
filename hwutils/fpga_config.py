#!/usr/bin/env python

from __future__ import print_function

import zipfile
import os
from lxml import etree # may consider some drop in replacement lib
import sys
import tempfile
import subprocess
import re
import shutil


def unzip_hdf(src):
    working_dir = tempfile.mkdtemp()
    # ZipFile() will check the file for us
    z = zipfile.ZipFile(src)
    z.extractall(working_dir)
    return working_dir

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

def get_ambapl_phandle():
    # this runs query on the living device tree and obtained the phandle
    # number through dtc decompile
    # notice that for some reason /proc/ gives wrong phandle value
    with open(os.devnull, 'w') as FNULL:
     device_tree = subprocess.check_output(
		   "dtc -I fs /sys/firmware/devicetree/base".split(),
		   stderr=FNULL)
    # run regex to obtain the phandle value
    regex = r"amba_pl(.|\n)*?phandle = <(?P<phandle>0x(\d+))>;"
    m = re.search(regex, device_tree, re.MULTILINE)
    phandle_str = m.group("phandle")
    int(phandle_str, 16) # check if it's actually hex str
    # return the hex str
    return phandle_str

def parse_gpio(elem):
    result = {}
    def parse_int(val, base=10):
        return int(val, base)
    # assume elem is gpio
    for param in elem.iter("PARAMETER"):
        param_attr = param.attrib
        name = param_attr["NAME"]
        val = param_attr["VALUE"]
        if name == "C_GPIO_WIDTH":
            result["gpio_width"] = parse_int(val)
        elif name == "C_GPIO2_WIDTH":
            result["gpio2_width"] = parse_int(val)
        elif name == "C_ALL_INPUTS":
            result["all_inputs"] = parse_int(val)
        elif name == "C_ALL_INPUTS_2":
            result["all_inputs2"] = parse_int(val)
        elif name == "C_ALL_OUTPUTS":
            result["all_outputs"] = parse_int(val)
        elif name == "C_ALL_OUTPUTS_2":
            result["all_outputs2"] = parse_int(val)
        elif name == "C_DOUT_DEFAULT":
            result["dout_default"] = parse_int(val, 16)
        elif name == "C_DOUT_DEFAULT_2":
            result["dout_default2"] = parse_int(val, 16)
        elif name == "C_IS_DUAL":
            result["is_dual"] = parse_int(val)
        elif name == "C_TRI_DEFAULT":
            result["tri_default"] = parse_int(val, 16)
        elif name == "C_TRI_DEFAULT_2":
            result["tri_default2"] = parse_int(val, 16)
        # reg
        elif name == "C_BASEADDR":
            result["base_addr"] = parse_int(val, 16)
        elif name == "C_HIGHADDR":
            result["high_addr"] = parse_int(val, 16)
    if len(result) != 13:
        raise Exception(
                "expected {0} elements in GPIO parameters. got {1}"\
                .format(13, len(result)))
    # compute the reg size
    reg_size = result["high_addr"] - result["base_addr"] + 1
    result["reg_size"] = reg_size
    return result

def parse_hwh(filename):
    # this might not be the most efficient way to parse since it went through
    # the tree multiple times
    root = etree.parse(filename)
    result = {"dma": [], "hls": [], "gpio": []}
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
        elif mod_attr.get("MODTYPE") and mod_attr["MODTYPE"] == "axi_gpio":
            gpio = parse_gpio(mod)
            gpio["module_name"] = mod_attr["INSTANCE"]
            result["gpio"].append(gpio)
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

def generate_gpio_overlay(indent, gpio):
    result = ""
    def add_line(result, new_indent, val):
        # notice that this one has closing ";"
        return add_to_line(result, indent + new_indent, val + ";")
    # opening gpio element
    result = add_to_line(result, indent,
                        "{0}: gpio@{1:x} ".format(gpio["module_name"],
                                                  gpio["base_addr"]) + "{")

    result = add_line(result, 1, "#gpio-cells = <2>")
    result = add_line(result, 1, "compatible = \"xlnx,xps-gpio-1.00.a\"")
    result = add_line(result, 1, "gpio-controller")
    result = add_line(result, 1, "reg = <0x0 0x{0:x} 0x0 0x{1:x}>".format(
                                 gpio["base_addr"], gpio["reg_size"]))
    result = add_line(result, 1, "xlnx,all-inputs = <0x{0:x}>".format(
                                 gpio["all_inputs"]))
    result = add_line(result, 1, "xlnx,all-inputs-2 = <0x{0:x}>".format(
                                 gpio["all_inputs2"]))
    result = add_line(result, 1, "xlnx,all-outputs = <0x{0:x}>".format(
                                 gpio["all_outputs"]))
    result = add_line(result, 1, "xlnx,all-outputs-2 = <0x{0:x}>".format(
                                 gpio["all_outputs2"]))
    result = add_line(result, 1, "xlnx,dout-default = <0x{0:08X}>".format(
                                 gpio["dout_default"]))
    result = add_line(result, 1, "xlnx,dout-default-2 = <0x{0:08X}>".format(
                                 gpio["dout_default2"]))
    result = add_line(result, 1, "xlnx,gpio-width = <0x{0:x}>".format(
                                 gpio["gpio_width"]))
    result = add_line(result, 1, "xlnx,gpio2-width = <0x{0:x}>".format(
                                 gpio["gpio2_width"]))
    # assuming the interupt parent is none
    result = add_line(result, 1, "xlnx,interrupt-present = <0x0>")
    result = add_line(result, 1, "xlnx,is-dual = <0x{0:x}>".format(
                                 gpio["is_dual"]))
    result = add_line(result, 1, "xlnx,tri-default = <0x{0:8X}>".format(
                                 gpio["tri_default"]))
    result = add_line(result, 1, "xlnx,tri-default-2 = <0x{0:8X}>".format(
                                 gpio["tri_default2"]))
    # closing gpio element
    result = add_line(result, 0, "}")

    return result

def generate_dma_overlay(indent, dma):
    result = ""
    def add_line(result, new_indent, val):
        # notice that this one has closing ";"
        return add_to_line(result, indent + new_indent, val + ";")

    tag = "{0}: dma@{1:08X} ".format(dma["module_name"], dma["base_addr"]) \
        + "{"
    # opening the dma element
    result = add_to_line(result, indent, tag)
    result = add_line(result, 1, "#dma-cells = <1>")
    result = add_line(result, 1, "clock-names = \"" +
                                 "\", \"".join(dma["clks"]) + "\"")
    result = add_line(result, 1, "clocks = " + ", ".join(["<&misc_clk_0>"
                                  for i in range(len(dma["clks"]))]))
    result = add_line(result, 1, "compatible = \"xlnx,axi-dma-1.00.a\"")
    result = add_line(result, 1, "interrupt-parent = <&gic>")

    # get interrupt numbers
    irqs = get_irqs(len(dma["dma_channels"]))
    irq_tag = " ".join(["0 {0} 4".format(irq) for irq in irqs])
    result = add_line(result, 1, "interrupts = <" + irq_tag + ">")
    base_addr = dma["base_addr"]
    result = add_line(result, 1, "reg = <0x0 0x{0:08X} 0x0 0x{1:x}>".format(
                                  base_addr, dma["reg_size"]));
    # all 32 bit
    result = add_line(result, 1, "xlnx,addrwidth = <0x20>");
    result = add_line(result, 1, "xlnx,include-sg");
    result = add_line(result, 1, "xlnx,multichannel-dma")

    # enumerate through the channels
    for idx, chan in enumerate(dma["dma_channels"]):
        addr = base_addr + chan[1] # (type, offset)
        # opening dma channel
        result = add_to_line(result, indent + 1,
                             "dma-channel@{0:08X} ".format(addr) + "{")
        result = add_line(result, 2, "compatible = \"" + chan[0] + "\"")
        result = add_line(result, 2, "dma-channels = <0x1>")
        result = add_line(result, 2,
                          "interrupts = <0 {0} 4>".format(irqs[idx]));
        is_mm2s = "mm2s" in chan[0]
        result = add_line(result, 2, "xlnx,datawidth = <0x{0:x}>".format(
                                      dma["mm2s_datawidth"] if is_mm2s else
                                      dma["s2mm_datawidth"]))
        result = add_line(result, 2, "xlnx,device-id = <0x0>")
        # closing dma channel
        result = add_line(result, 1, "}")
    # closing dma element
    result = add_line(result, 0, "}")
    return result

def generate_hls_overlay(indent, hls):
    result = ""
    def add_line(result, new_indent, val):
        # notice that this one has closing ";"
        return add_to_line(result, indent + new_indent, val + ";")

    base_addr = hls["base_addr"]
    # opening hls element
    result = add_to_line(result, indent, "{0}: hls_target@{1:08X}".format(
                         hls["module_name"], base_addr) + " {")
    result = add_line(result, 1, "compatible = \"xlnx,hls-target-1.0\"")
    result = add_line(result, 1, "reg = <0x0 0x{0:08X} 0x0 0x{1:x}>".format(
                         base_addr, hls["reg_size"]))
    # closing hls element
    result = add_line(result, 0, "}")

    return result

def hardcoded_overlay(indent):
    result = ""
    def add_line(result, new_indent, val):
        # notice that this one has closing ";"
        return add_to_line(result, indent + new_indent, val + ";")
    result = add_to_line(result, indent, "psu_ctrl_ipi: PERIPHERAL@ff380000 {")
    result = add_line(result, 1, "compatible = \"xlnx,PERIPHERAL-1.0\"")
    result = add_line(result, 1, "reg = <0x0 0xff380000 0x0 0x80000>")
    result = add_line(result, 0, "}")

    result = add_to_line(result, indent,
                         "psu_message_buffers: PERIPHERAL@ff990000 {")
    result = add_line(result, 1, "compatible = \"xlnx,PERIPHERAL-1.0\"")
    result = add_line(result, 1, "reg = <0x0 0xff990000 0x0 0x10000>")
    result = add_line(result, 0, "}")

    result = add_to_line(result, indent, "misc_clk_0: misc_clk_0 {")
    return add_line(result, 0, "}")

def generate_overlay(hw_info):
    # based on https://lkml.org/lkml/2014/5/28/280
    phandle = "0x37" #get_ambapl_phandle()
    # may consider to make a dt builder
    result = ""
    result += "/dts-v1/;\n/plugin/;	" \
              "/* allow undefined label references and record them */\n\n"
    result += "/ {\n"

    # opening fragment
    result = add_to_line(result, 1, "fragment@0 {")
    # write cell format
    result = add_to_line(result, 2, "#address-cells = <2>;")
    result = add_to_line(result, 2, "#size-cells = <2>;")
    # overlay to amba_pl element
    result = add_to_line(result, 2, "target = <{0}>;".format(phandle))
    # start the overlay
    result = add_to_line(result, 2, "__overlay__ {")
    # more cell format
    result = add_to_line(result, 3, "#address-cells = <2>;")
    result = add_to_line(result, 3, "#size-cells = <2>;")

    # loop through each module
    for gpio in hw_info["gpio"]:
        result += generate_gpio_overlay(3, gpio)

    for dma in hw_info["dma"]:
        result += generate_dma_overlay(3, dma)

    for hls in hw_info["hls"]:
        result += generate_hls_overlay(3, hls)

    # hardcoded
    # TODO: fix this?
    result += hardcoded_overlay(3)

    # ending overlay
    result = add_to_line(result, 2, "};")
    # ending fragment
    result = add_to_line(result, 1, "};")
    # this is the end
    result += "};"
    return result

def compile_dtb(str_val):
    working_dir = tempfile.mkdtemp()
    dst_file = tempfile.mkstemp(suffix=".dst", dir=working_dir)[1]
    dtb_file = tempfile.mkstemp(suffix=".dtb", dir=working_dir)[1]
    # write to the source file
    with open(dst_file, "w") as f:
        f.write(str_val)
    # assume user already has dtc installed
    # add no warning flag because current dtc doesn't like overlay that well
    flags = " -I dts -O dtb -W no-unit_address_vs_reg"
    command = "dtc " + dst_file + " -o " + dtb_file + flags
    print(command)
    subprocess.check_call(command.split())

    return dtb_file

def install_overlay(dtb_file):
    # assume we are already sudo
    # we only want one activate so remove the previous one, if it exists
    OVERLAY_DIRNAME = "fcam4"
    OVERLAY_ROOT = "/sys/kernel/config/device-tree/overlays/"
    if not os.isdir(OVERLAY_ROOT):
        raise Exception(OVERLAY_ROOT + " not found")
    overlay_dir = os.path.join(OVERLAY_ROOT, OVERLAY_DIRNAME)
    if os.isdir(overlay_dir):
        os.rmdir(overlay_dir) # uninstall the old one
    os.mkdir(overlay_dir)
    dst = os.path.join(overlay_dir, "dtbo")
    shutil.copyfile(dtb_file, dst)
    # the rest is up to kernel

def is_root():
    return os.geteuid() == 0

def clean_up(files):
    # clean up stuff due to memory concern
    # remove the temp dir we created
    for filename in files:
        dirname = os.path.dirname(filename)
        os.rmdir(dirname)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("[Usage]:", sys.argv[0], "<project.hdf>", file=sys.stderr)
        exit(1)

    if not is_root():
        print("please run as root or `sudo", sys.argv[0], "<project.hdf>`",
              file=sys.stderr)
        exit(1)
    hdf_dir = unzip_hdf(sys.argv[1])
    hwh_file = find_hwh(hdf_dir)
    hw_info = parse_hwh(hwh_file)
    sanity_check(hw_info)
    dts_str = generate_overlay(hw_info)
    dtb_file = compile_dtb(dts_str)
    install_overlay(dtb_file)
    clean_up([dtb_file, hwh_file])
