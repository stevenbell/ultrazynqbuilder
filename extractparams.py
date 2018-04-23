# Read the user parameters file (usually hwconfig.user) and extract
# parameters for all of the user-defined modules specified there.

import os
import yaml
from lxml import etree
#from IPython import embed

inpath = 'hwconfig.user'
outpath = 'hwconfig.all'

params = yaml.load(open(inpath))

for module in params['hw']:
  if module['type'] == 'dma':
    # Xilinx provided stuff doesn't need probing
    continue

  if not os.path.isdir(module['path']):
    print "[Error] Couldn't find IP for %s in %s!" % (module['name'], module['path'])
    continue

  print "Extracting parameters for " + module['name']
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
      print "[Warning] Failed to get stream depth for stream %s" % stream['name']

    module['streams'].append(stream)

outfile = open(outpath, 'w')
outfile.write(yaml.dump(params))
outfile.close()

