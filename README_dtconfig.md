# How to use the "dtconfig.py" script

dtconfig.py script reads in a mandatory hardware
description file and generates a device-tree overlay
that is needed to help kernel drivers dynamically 
obtain information about underlying hardware resources.

## Usage:
```
$> python dtconfig <path/to/hwconfig_file>
```
This will generate a `system-user-overlay.dtsi` file that
will need to be copied into the petalinux project under the 
folder: project-spec/meta-user/recipes-bsp/device-tree/files/

## Remark
The `system-user.dtsi` file located under the directory specified
above has an "include" statement that expects the generated
`system-user-overlay.dtsi` file.
