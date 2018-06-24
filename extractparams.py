# Read the user parameters file (usually hwconfig.user) and extract
# parameters for all of the user-defined modules specified there.
from __future__ import print_function
import os
import yaml
import sys
from lxml import etree
#from IPython import embed

def is_power2(num):
    return num != 0 and ((num & (num - 1)) == 0)


inpath = 'hwconfig.user'
outpath = 'hwconfig.all'

params = yaml.load(open(inpath))

for module in params['hw']:
  if module['type'] == 'dma':
    # Xilinx provided stuff doesn't need probing
    continue

  if not os.path.isdir(module['path']):
    print("[Error] Couldn't find IP for %s in %s!" % (module['name'], module['path']))
    continue

  print("Extracting parameters for", module['name'])
  module['streams'] = [] # Start empty list of data streams

  desc = etree.parse(open(module['path']+'/component.xml'))
  # Have to pass the namespace mappings, or lxml chokes
  ns = desc.getroot().nsmap

  # Create the VLNV (formal Xilinx name: vendor-library-name-version)
  module['vlnv'] = \
    str(desc.xpath('/spirit:component/spirit:vendor/text()', namespaces=ns)[0]) + ":" + \
    str(desc.xpath('/spirit:component/spirit:library/text()', namespaces=ns)[0]) + ":" + \
    str(desc.xpath('/spirit:component/spirit:name/text()', namespaces=ns)[0]) +  ":" +\
    str(desc.xpath('/spirit:component/spirit:version/text()', namespaces=ns)[0])

  # Get all of the AXI-stream interfaces
  ifaces = desc.xpath('//spirit:busInterface[spirit:busType/@spirit:name="axis"]', namespaces=ns)
  for iface in ifaces:
    stream = {}
    stream['name'] = str(iface.xpath('spirit:name/text()', namespaces=ns)[0])
    if len(iface.xpath('spirit:master', namespaces=ns)) is not 0:
      stream['type'] = 'output'
    elif len(iface.xpath('spirit:slave', namespaces=ns)) is not 0:
      stream['type'] = 'input'
    else:
      stream['type'] = 'unknown' # We're in serious trouble...

    streamdepths = iface.xpath('.//spirit:parameter[spirit:name="TDATA_NUM_BYTES"]/spirit:value/text()', namespaces=ns)
    if len(streamdepths) == 1:
      stream['depth'] = int(streamdepths[0])
    else:
      print("[Warning] Failed to get stream depth for stream", stream['name'])

    module['streams'].append(stream)

# second pass
for module in params["hw"]:
  if module["type"] == "hls":
    input_types = ["dma", "csi"]
    # we need to collect all the DMA's that are pointing to it
    input_dmas = []
    specified_dmas = []
    for dma in params["hw"]:
      if dma["type"] not in input_types:
        continue
      # we have found an input dma channel
      if dma["outputto"] == module["name"]:
        input_dmas.append(dma)
      elif "." in dma["outputto"] and dma["outputto"].split(".")[0] == module["name"]:
        specified_dmas.append(dma)
      else:
        print("[Warning] Mismatched channel name. Expect module name", module["name"],
              "got", dma["outputto"])
    input_streams = [entry for entry in module["streams"]
                     if entry["type"] == "input"]
    # we need to make sure that the input list matches with the HLS IP
    assert(len(input_dmas + specified_dmas) == len(input_streams))
    # remove the input_streams that's been specified already
    for dma in specified_dmas:
      input_streams = [entry for entry in input_streams
                       if entry["name"] != dma["outputto"].split(".")[-1]]
    # rename the outputto property
    # also double check if the user already specifies the connections
    for i in range(len(input_dmas)):
      # updating the object by reference
      # the format will be hls_module.arg_#
      input_dmas[i]["outputto"] += "." + input_streams[i]["name"]

# thid pass to see if there is any input only dma
for module in params["hw"]:
    if module["type"] == "dma":
        out_connected = False
        for m in params["hw"]:
            if m["type"] == "hls" and m["outputto"] == module["name"]:
                out_connected = True
                break
        module["out_connected"] = out_connected

# fourth pass to properly setup dma width
for module in params["hw"]:
    if module["type"] == "dma":
        # search connected hls module to see its depth
        for m in params["hw"]:
            if m["type"] == "hls" and "outputto" in m and \
                    m["outputto"] == module["name"]:
                # find the output stream
                s = None
                for stream in m["streams"]:
                    if stream["type"] == "output":
                        s = stream
                        break
                if s is None:
                    raise Exception("no output found in " + m["name"])
                depth = s["depth"]
                if not is_power2(depth):
                    depth = 2
                module["s2mm_width"] = depth * 8

            out_args = module["outputto"].split(".")
            module_name = out_args[0]
            stream_name = out_args[1]
            if m["name"] == module_name:
                for stream in m["streams"]:
                    if stream["name"] == stream_name:
                        s = stream
                        break
                depth = s["depth"]
                if not is_power2(depth):
                    depth = 2
                module["mm2s_width"] = depth * 8
        # sanity check
        if (module["out_connected"] and "s2mm_width" not in module) or \
                ("mm2s_width" not in module):
            print(module)
            raise Exception("corrupted hwconfig file")

# last pass to print out the dma channel order without modification
dmas = []
for module in params["hw"]:
    if module["type"] == "dma":
        # assumes only one HLS module
        dmas.append((module["name"], module["out_connected"],
                     module["outputto"].split(".")[-1]))
# notice that the ordering of dma channel loading is
# dma0 (in + out) -> dma1 (in) -> dma2 (in)
# therefore the default loading order is
# dma0 -> dma1 -> dma2. we should print this out and let user modify generated
# dtoverlay file
if not dmas[0][1]:
    # the first one should be output channel. so somehow we messed up
    print("[WARNING]: corrupted dma channel sequence", file=sys.stderr)
print("Default input dma channel loading sequence in kernel:")
for index, dma in enumerate(dmas):
    print("[{0}]: {1} -> {2}".format(index, dma[0], dma[2]))
print("Please modify dtoverlay accordingly")

outfile = open(outpath, 'w+')
outfile.write(yaml.dump(params))
outfile.close()

