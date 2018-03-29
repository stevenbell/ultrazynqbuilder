# DisplayPort input clock for the IO carrier card
# Clock is routed through SOM JX1_HP_DP_36_GC
set_property PACKAGE_PIN N3  [get_ports {idt5901_clk_n}]
set_property PACKAGE_PIN N4  [get_ports {idt5901_clk_p}]
set_property IOSTANDARD LVDS [get_ports {idt5901_clk_p idt5901_clk_n}]

