# DisplayPort clock for the PCIe carrier card
# Clock is routed through SOM JX2_HP_DP_11_GC
set_property PACKAGE_PIN E5  [get_ports {idt5901_clk_n}]
set_property PACKAGE_PIN E6  [get_ports {idt5901_clk_p}]

set_property IOSTANDARD LVDS [get_ports {idt5901_clk_p idt5901_clk_n}]
