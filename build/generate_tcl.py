#!/usr/bin/env python

#Usage "./generate_tcl.py PATH_TO_HLS_CORE 

#ADD date info!!

import fnmatch
import os
import sys
import re

#Check if valid path!

f=open(sys.argv[1] + "/component.xml","r")
lines = f.readlines()
f.close()

count = 7
input_flag = False;
output_flag = False;

hls_input_width = "1"
hls_output_width = "1"
hls_path = sys.argv[1]

for line in lines:
	if "arg_0_TDATA" in line:
		count = 0
		input_flag = True;
#		print("Found One!")

	if "arg_1_TDATA" in line:
		count = 0
		output_flag = True;
#		print("Found One!")

	if count == 4:
#		print(str(count) + " " + line);
		if "<spirit:left spirit:format=\"long\">" in line:
			if input_flag:
				hls_input_width = int(filter(str.isdigit, line))
#				print("hls_input_width is" + str(hls_input_width));
			elif output_flag:
				hls_output_width = int(filter(str.isdigit, line))
#				print("hls_output_width is" + str(hls_output_width));
#			print("Got the right line!")
	
	if count < 5:
		count = count + 1;
	else:
		input_flag = False;
		output_flag = False;

hls_input_width = (hls_input_width + 1)/8
hls_output_width = (hls_output_width + 1)/8

#print("hls_input_width is" + str(hls_input_width))
#print("hls_output_width is" + str(hls_output_width))

f=open("rebuild_bd.tcl.template","r")
lines = f.readlines()
f.close()

f=open("mkproject.tcl","w+")
for line in lines:
	if "<HLS_INPUT_BYTE_WIDTH>" in line:
		f.write(line.replace("<HLS_INPUT_BYTE_WIDTH>", str(hls_input_width)))
	elif "<HLS_OUTPUT_BYTE_WIDTH>" in line:
		f.write(line.replace("<HLS_OUTPUT_BYTE_WIDTH>", str(hls_output_width)))
	elif "<PATH_TO_HLS_IP_DIR>" in line:
		f.write(line.replace("<PATH_TO_HLS_IP_DIR>", hls_path))
	elif "<PWD>" in line:
		f.write(line.replace("<PWD>", os.getcwd()))
	else:
		f.write(line)

f.close()
