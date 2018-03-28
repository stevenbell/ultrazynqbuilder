# Read the user parameters file (usually hwconfig.user) and extract
# parameters for all of the Halide-HLS accelerators specified there.

import yaml
from lxml import etree
from IPython import embed

inpath = 'hwconfig.user'
outpath = 'hwconfig.all'

params = yaml.load(open(inpath))

# Find any hardware accelerators
for accelerator in params['hw']:
  print "Extracting parameters for " + accelerator['name']
  accelerator['streams'] = [] # Start empty list of data streams

  desc = etree.parse(open(accelerator['path']+'/component.xml'))
  # Have to pass the namespace mappings, or lxml chokes
  ns = desc.getroot().nsmap

  # Create the VLNV (formal Xilinx name: vendor-library-name-version)
  accelerator['vlnv'] = \
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

    stream['depth'] = int(iface.xpath('.//spirit:parameter[spirit:name="TDATA_NUM_BYTES"]/spirit:value/text()', namespaces=ns)[0])

    accelerator['streams'].append(stream)

outfile = open(outpath, 'w')
outfile.write(yaml.dump(params))
outfile.close()

