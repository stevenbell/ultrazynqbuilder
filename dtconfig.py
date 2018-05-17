import yaml
import sys

"""
Example usage:

$> python dtconfig.py path/to/hwconfig.user [output_dt_overlay_filename]
"""

####### PARAMETER DEFINITIONS ###############

# value of "compatible" property for hls nodes
hls_compatible_string = "hls-target"

# value of "compatible" property for dmas
dma_compatible_string = "hls-dma"

# name of hls node's property that references input dmas
prop_name_dmas = "dmas"

# name of dma node's property that references parent hls node
prop_name_hls_ref = "hls-node"

# name of the property that contains device node's name
prop_name_node_name = "hw-name"

######## END OF PARAMETE DEFINITIONS ########

# check command-line args
usage_prompt = "usage: python dtconfig.py <path/to/hwconfig_file> [output_dt_overlay_filename]"
if(len(sys.argv) < 2):
	print(usage_prompt)
	exit()

cfg_file_path = sys.argv[1]

if (len(sys.argv) == 3):
	dts_file_path = sys.argv[2]
else:
	dts_file_path = "system-user-overlay.dtsi"

# read yaml file and convert to dictionary
with open(cfg_file_path, 'r') as cfg_file:
	cfg = yaml.load(cfg_file)

if 'hw' not in cfg:
	sys.exit("[dtconfig]: the 'hw' node not found in the provided hardware configuration file")

overlay = dict()

# overlay initialization
for hw_node in cfg['hw']:
	node_type = hw_node['type']
	node_name = hw_node['name']

	if (node_type ==  'dma' or node_type == 'hls'):
			overlay[node_name] = dict()
			overlay[node_name]['definition'] = hw_node

			if(node_type == 'dma'):
				overlay[node_name]['direction'] = 0
				overlay[node_name]['hls-node'] = ""
			else: # node_type == 'hls'
				overlay[node_name]['dmas'] = []

# filling in overlay info
for key in overlay:
	node = overlay[key]['definition']
	node_type = node['type']
	node_name = node['name']

	if(node_type == 'dma'):
		if 'outputto' in node:
                        overlay[node_name]['direction'] += 1
                        outputto = node["outputto"].split('.')[0]
                        overlay[node_name]['hls-node'] = outputto
                        overlay[outputto]['dmas'].append(node_name)
	else:
		if node['outputto'] in overlay and overlay[node['outputto']]['definition']['type'] == 'dma':
			overlay[node_name]['dmas'].append(node['outputto'])
			overlay[node['outputto']]['hls-node'] = node_name
			overlay[node['outputto']]['direction'] += 2


# creating dt overlay
dt_overlay = ""
for key in sorted(overlay.iterkeys()):
	dt_overlay += "&" + key + " {"
	dt_overlay += "\n\t" + prop_name_node_name + " = " + "\"" + overlay[key]['definition']['name'] + "\";"

	if overlay[key]['definition']['type'] == 'dma':
		dt_overlay += "\n\tcompatible = \"" + dma_compatible_string + "\";"
		dt_overlay += "\n\tdirection = <" + str(overlay[key]['direction']) + ">;"
		dt_overlay += "\n\t" + prop_name_hls_ref + " = <&" + overlay[key]['hls-node'] + ">;"
	else:
		dt_overlay += "\n\tcompatible = \"" + hls_compatible_string + "\";"
		dt_overlay += "\n\tgpio = <&axi_gpio_1>;"
		#input dmas
		if len(overlay[key]['dmas']) == 0:
			dt_overlay += "\n\t" + prop_name_dmas + " = <empty>;"
		else:
			dt_overlay += "\n\t" + prop_name_dmas + " = "
			for counter, value in enumerate(overlay[key]['dmas']):
				dt_overlay += "<&" + value + ">"
				if(counter < len(overlay[key]['dmas']) - 1):
					dt_overlay += ", "
				else:
					dt_overlay += ";"

	dt_overlay += "\n};\n"

with open(dts_file_path, 'a') as dts_file:
	dts_file.write(dt_overlay)

cfg_file.close()
dts_file.close()

print("[dtconfig] Device-tree overlay written to %s" % (dts_file_path))
