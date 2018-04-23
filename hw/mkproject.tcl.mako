# Generated automatically by ${toolName} on ${date}
# This script was created from write_bd_tcl in Vivado of the original working design.
# Made edits so that it would recreate the entire hardware project from
# scratch and from a few source files.
# Warren Cheng 3/23/18

# Note that since ${  } is valid tcl, there are potential pitfalls here when
# parameterizing this with Mako.
# We *could* do all this with tcl (it is a fully featured language, after all),
# but Python is nicer, and we're already using Mako for other templates.


################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2017.2
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_msg_id "BD_TCL-109" "ERROR" "This script was created for Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Basically you're hosed, because Xilinx has awful version compatibility."}

   return 1
}

################################################################
# Global variables
################################################################
# Project name
set project_name ${name}

set project_dir ./${name}

# Name of the block design (not the name of the project)
set bd_name design_1

# productionsilicon = y/n
% if productionsilicon == 'y':
set board_part xczu3eg-sfva625-1-i
set board_prod _production
% else:
set board_part xczu3eg-sfva625-1-i-es1
set board_prod ""
% endif


################################################################
# Top-level build process
################################################################

create_project $project_name $project_dir -part $board_part

% if board == 'iocc':
set_property BOARD_PART em.avnet.com:ultrazed_eg_iocc$board_prod:part0:1.0 [current_project]
% elif board == 'pciecc':
set_property BOARD_PART em.avnet.com:ultrazed_eg_pciecc$board_prod:part0:1.0 [current_project]
% else:
puts "This script was configured without a carrier board specified.  Expect problems."
% endif

create_bd_design $bd_name

##################################################################
# Procs for building parts of the design
##################################################################

# Hierarchical cell: idt5901_clk
proc create_hier_cell_idt5901_clk { parentCell nameHier } {

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" "create_hier_cell_idt5901_clk() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 CLK_IN_D

  # Create pins
  create_bd_pin -dir O -from 0 -to 0 BUFG_O

  # Create instance: util_ds_buf_0, and set properties
  set util_ds_buf_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 util_ds_buf_0 ]
  set_property -dict [ list \
CONFIG.C_BUF_TYPE {IBUFDS} \
 ] $util_ds_buf_0

  # Create instance: util_ds_buf_1, and set properties
  set util_ds_buf_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 util_ds_buf_1 ]
  set_property -dict [ list \
CONFIG.C_BUF_TYPE {BUFG} \
 ] $util_ds_buf_1

  # Create interface connections
  connect_bd_intf_net -intf_net Conn1 [get_bd_intf_pins CLK_IN_D] [get_bd_intf_pins util_ds_buf_0/CLK_IN_D]

  # Create port connections
  connect_bd_net -net util_ds_buf_0_IBUF_OUT [get_bd_pins util_ds_buf_0/IBUF_OUT] [get_bd_pins util_ds_buf_1/BUFG_I]
  connect_bd_net -net util_ds_buf_1_BUFG_O [get_bd_pins BUFG_O] [get_bd_pins util_ds_buf_1/BUFG_O]

  # Restore current instance
  current_bd_instance $oldCurInst
}


# Create the core design which contains the PS8 and the essentials to connect to it
# This operates on the root of the design.

# General architecture:
# There is one AXI-Lite control bust at 100MHz (clock 0)
# (for now) all the DMA transactions are routed to a single HPC port at 200MHz (clock 1)
# The DPHY receivers are given 200MHz (clock 2)
# The csirx modules are driven by the CSI byte clocks, and go through FIFOs for the clock crossing
proc create_root_design { } {

  set parentCell [get_bd_cells /]

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Set parent object as current
  current_bd_instance $parentObj


  # Create the PS8 and configure it
  set zynq_ultra_ps_e_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.0 zynq_ultra_ps_e_0 ]
  set_property -dict [ list \
CONFIG.PSU_BANK_0_IO_STANDARD {LVCMOS18} \
CONFIG.PSU_BANK_1_IO_STANDARD {LVCMOS33} \
CONFIG.PSU_BANK_2_IO_STANDARD {LVCMOS18} \
CONFIG.PSU__CAN1__PERIPHERAL__ENABLE {0} \
CONFIG.PSU__CRF_APB__ACPU_CTRL__FREQMHZ {1100} \
CONFIG.PSU__CRF_APB__ACPU_CTRL__SRCSEL {APLL} \
CONFIG.PSU__CRF_APB__APLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRF_APB__DBG_FPD_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__DBG_FPD_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__DBG_TRACE_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__DBG_TRACE_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__DBG_TSTMP_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__DBG_TSTMP_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__DDR_CTRL__FREQMHZ {1067} \
CONFIG.PSU__CRF_APB__DDR_CTRL__SRCSEL {DPLL} \
CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__FREQMHZ {550} \
CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__SRCSEL {APLL} \
CONFIG.PSU__CRF_APB__DPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRF_APB__DP_AUDIO_REF_CTRL__SRCSEL {RPLL} \
CONFIG.PSU__CRF_APB__DP_STC_REF_CTRL__SRCSEL {RPLL} \
CONFIG.PSU__CRF_APB__DP_VIDEO_REF_CTRL__SRCSEL {VPLL} \
CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__FREQMHZ {550} \
CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__SRCSEL {APLL} \
CONFIG.PSU__CRF_APB__GPU_REF_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRF_APB__GPU_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__SATA_REF_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__SATA_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__TOPSW_LSBUS_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRF_APB__TOPSW_LSBUS_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__TOPSW_MAIN_CTRL__FREQMHZ {475} \
CONFIG.PSU__CRF_APB__TOPSW_MAIN_CTRL__SRCSEL {DPLL} \
CONFIG.PSU__CRF_APB__VPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRL_APB__ADMA_REF_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRL_APB__ADMA_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__CAN1_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__CAN1_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__CPU_R5_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRL_APB__CPU_R5_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__DBG_LPD_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRL_APB__DBG_LPD_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__GEM3_REF_CTRL__FREQMHZ {125} \
CONFIG.PSU__CRL_APB__GEM3_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__I2C0_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__I2C0_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__I2C1_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__I2C1_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__IOPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRL_APB__IOU_SWITCH_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRL_APB__IOU_SWITCH_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__LPD_LSBUS_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__LPD_LSBUS_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__LPD_SWITCH_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRL_APB__LPD_SWITCH_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__PCAP_CTRL__FREQMHZ {200} \
CONFIG.PSU__CRL_APB__PCAP_CTRL__SRCSEL {RPLL} \
CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__PL0_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__PL1_REF_CTRL__FREQMHZ {200} \
CONFIG.PSU__CRL_APB__PL2_REF_CTRL__FREQMHZ {200} \
CONFIG.PSU__CRL_APB__QSPI_REF_CTRL__FREQMHZ {125} \
CONFIG.PSU__CRL_APB__QSPI_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__RPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRL_APB__SDIO1_REF_CTRL__FREQMHZ {200} \
CONFIG.PSU__CRL_APB__SDIO1_REF_CTRL__SRCSEL {RPLL} \
CONFIG.PSU__CRL_APB__TIMESTAMP_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__TIMESTAMP_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__UART0_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__UART0_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__UART1_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__UART1_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__USB0_BUS_REF_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRL_APB__USB0_BUS_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__USB3_DUAL_REF_CTRL__FREQMHZ {20} \
CONFIG.PSU__CRL_APB__USB3_DUAL_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__DDRC__BANK_ADDR_COUNT {2} \
CONFIG.PSU__DDRC__BG_ADDR_COUNT {1} \
CONFIG.PSU__DDRC__BRC_MAPPING {ROW_BANK_COL} \
CONFIG.PSU__DDRC__BUS_WIDTH {32 Bit} \
CONFIG.PSU__DDRC__CL {15} \
CONFIG.PSU__DDRC__CLOCK_STOP_EN {0} \
CONFIG.PSU__DDRC__COL_ADDR_COUNT {10} \
CONFIG.PSU__DDRC__CWL {14} \
CONFIG.PSU__DDRC__DDR4_ADDR_MAPPING {0} \
CONFIG.PSU__DDRC__DDR4_CAL_MODE_ENABLE {0} \
CONFIG.PSU__DDRC__DDR4_CRC_CONTROL {0} \
CONFIG.PSU__DDRC__DDR4_T_REF_MODE {0} \
CONFIG.PSU__DDRC__DDR4_T_REF_RANGE {Normal (0-85)} \
CONFIG.PSU__DDRC__DEVICE_CAPACITY {8192 MBits} \
CONFIG.PSU__DDRC__DIMM_ADDR_MIRROR {0} \
CONFIG.PSU__DDRC__DM_DBI {DM_NO_DBI} \
CONFIG.PSU__DDRC__DRAM_WIDTH {16 Bits} \
CONFIG.PSU__DDRC__ECC {Disabled} \
CONFIG.PSU__DDRC__ENABLE {1} \
CONFIG.PSU__DDRC__FGRM {1X} \
CONFIG.PSU__DDRC__LP_ASR {manual normal} \
CONFIG.PSU__DDRC__MEMORY_TYPE {DDR 4} \
CONFIG.PSU__DDRC__PARITY_ENABLE {0} \
CONFIG.PSU__DDRC__PER_BANK_REFRESH {0} \
CONFIG.PSU__DDRC__PHY_DBI_MODE {0} \
CONFIG.PSU__DDRC__RANK_ADDR_COUNT {0} \
CONFIG.PSU__DDRC__ROW_ADDR_COUNT {16} \
CONFIG.PSU__DDRC__SELF_REF_ABORT {0} \
CONFIG.PSU__DDRC__SPEED_BIN {DDR4_2133P} \
CONFIG.PSU__DDRC__STATIC_RD_MODE {0} \
CONFIG.PSU__DDRC__TRAIN_DATA_EYE {1} \
CONFIG.PSU__DDRC__TRAIN_READ_GATE {1} \
CONFIG.PSU__DDRC__TRAIN_WRITE_LEVEL {1} \
CONFIG.PSU__DDRC__T_FAW {30.0} \
CONFIG.PSU__DDRC__T_RAS_MIN {33} \
CONFIG.PSU__DDRC__T_RC {47.06} \
CONFIG.PSU__DDRC__T_RCD {15} \
CONFIG.PSU__DDRC__T_RP {15} \
CONFIG.PSU__DDRC__VREF {1} \
CONFIG.PSU__DISPLAYPORT__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__DPAUX__PERIPHERAL__IO {MIO 27 .. 30} \
% if board == 'iocc':
CONFIG.PSU__DP__LANE_SEL {Dual Higher} \
% elif board == 'pciecc':
CONFIG.PSU__DP__LANE_SEL {Single Higher} \
% endif
CONFIG.PSU__DP__REF_CLK_FREQ {27} \
CONFIG.PSU__DP__REF_CLK_SEL {Ref Clk3} \
CONFIG.PSU__ENET3__GRP_MDIO__ENABLE {1} \
CONFIG.PSU__ENET3__GRP_MDIO__IO {MIO 76 .. 77} \
CONFIG.PSU__ENET3__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__ENET3__PERIPHERAL__IO {MIO 64 .. 75} \
CONFIG.PSU__FPGA_PL0_ENABLE {1} \
CONFIG.PSU__FPGA_PL1_ENABLE {1} \
CONFIG.PSU__FPGA_PL2_ENABLE {1} \
CONFIG.PSU__GPIO0_MIO__IO {MIO 0 .. 25} \
CONFIG.PSU__GPIO0_MIO__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__GPIO1_MIO__IO {MIO 26 .. 51} \
CONFIG.PSU__GPIO1_MIO__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__I2C0__PERIPHERAL__ENABLE {0} \
CONFIG.PSU__I2C1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__I2C1__PERIPHERAL__IO {MIO 24 .. 25} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC0_SEL {APB} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC1_SEL {APB} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC2_SEL {APB} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC3_SEL {APB} \
CONFIG.PSU__MAXIGP2__DATA_WIDTH {64} \
CONFIG.PSU__OVERRIDE__BASIC_CLOCK {0} \
CONFIG.PSU__PSS_REF_CLK__FREQMHZ {33.333} \
CONFIG.PSU__QSPI__GRP_FBCLK__ENABLE {1} \
CONFIG.PSU__QSPI__GRP_FBCLK__IO {MIO 6} \
CONFIG.PSU__QSPI__PERIPHERAL__DATA_MODE {x4} \
CONFIG.PSU__QSPI__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__QSPI__PERIPHERAL__IO {MIO 0 .. 12} \
CONFIG.PSU__QSPI__PERIPHERAL__MODE {Dual Parallel} \
CONFIG.PSU__SAXIGP0__DATA_WIDTH {128} \
CONFIG.PSU__SAXIGP2__DATA_WIDTH {32} \
CONFIG.PSU__SAXIGP3__DATA_WIDTH {32} \
CONFIG.PSU__SAXIGP4__DATA_WIDTH {32} \
CONFIG.PSU__SD0__DATA_TRANSFER_MODE {8Bit} \
CONFIG.PSU__SD0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__SD0__PERIPHERAL__IO {MIO 13 .. 22} \
CONFIG.PSU__SD0__SLOT_TYPE {eMMC} \
CONFIG.PSU__SD1__DATA_TRANSFER_MODE {4Bit} \
CONFIG.PSU__SD1__GRP_CD__ENABLE {1} \
CONFIG.PSU__SD1__GRP_CD__IO {MIO 45} \
CONFIG.PSU__SD1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__SD1__PERIPHERAL__IO {MIO 46 .. 51} \
CONFIG.PSU__SD1__SLOT_TYPE {SD 2.0} \
CONFIG.PSU__SWDT0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__SWDT1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC2__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC3__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__UART0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__UART0__PERIPHERAL__IO {MIO 34 .. 35} \
CONFIG.PSU__UART1__MODEM__ENABLE {0} \
CONFIG.PSU__UART1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__UART1__PERIPHERAL__IO {MIO 32 .. 33} \
CONFIG.PSU__USB0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__USB0__PERIPHERAL__IO {MIO 52 .. 63} \
CONFIG.PSU__USB0__REF_CLK_FREQ {26} \
CONFIG.PSU__USB0__REF_CLK_SEL {Ref Clk0} \
CONFIG.PSU__USE__IRQ0 {1} \
CONFIG.PSU__USE__M_AXI_GP0 {0} \
CONFIG.PSU__USE__M_AXI_GP1 {0} \
CONFIG.PSU__USE__M_AXI_GP2 {1} \
CONFIG.PSU__USE__S_AXI_ACE {0} \
CONFIG.PSU__USE__S_AXI_ACP {0} \
CONFIG.PSU__USE__S_AXI_GP0 {1} \
CONFIG.PSU__USE__S_AXI_GP2 {0} \
CONFIG.PSU__USE__S_AXI_GP3 {0} \
CONFIG.PSU__USE__S_AXI_GP4 {0} \
CONFIG.PSU__USE__S_AXI_GP5 {0} \
CONFIG.PSU__USE__VIDEO {1} \
 ] $zynq_ultra_ps_e_0

  # Create the reset for clock0
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 clk0_reset
  connect_bd_net [get_bd_pins clk0_reset/slowest_sync_clk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk0]
  connect_bd_net [get_bd_pins clk0_reset/ext_reset_in] [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0]

  # Create the reset for clock1
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 clk1_reset
  connect_bd_net [get_bd_pins clk1_reset/slowest_sync_clk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk1]
  connect_bd_net [get_bd_pins clk1_reset/ext_reset_in] [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0]

  # Create the reset for clock2
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 clk2_reset
  connect_bd_net [get_bd_pins clk2_reset/slowest_sync_clk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk2]
  connect_bd_net [get_bd_pins clk2_reset/ext_reset_in] [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0]

  # Specify these explicitly, because block automation does weird things sometimes
  set controlclk /zynq_ultra_ps_e_0/pl_clk0
  set dataclk /zynq_ultra_ps_e_0/pl_clk1

  # Create the interconnect for the AXI-lite control interfaces
  create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 control_xconn
  # Start with just 1 master, 1 slave; we'll add more as necessary
  set_property -dict [list CONFIG.NUM_SI {1}] [get_bd_cells control_xconn]

  # Connect AXI, clock and reset for the interconnect
  connect_bd_intf_net [get_bd_intf_pins control_xconn/S00_AXI] [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_LPD]
  connect_bd_net [get_bd_pins control_xconn/aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk0]
  connect_bd_net [get_bd_pins control_xconn/aresetn] [get_bd_pins clk0_reset/interconnect_aresetn]

  # Create the interconnect for the AXI high-performance data transfer
  create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 data_xconn
  # Start with just 1 master, 1 slave; we'll add more as necessary
  set_property -dict [list CONFIG.NUM_SI {1}] [get_bd_cells data_xconn]

  # Connect AXI, clock and reset for the interconnect
  connect_bd_intf_net [get_bd_intf_pins data_xconn/M00_AXI] [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HPC0_FPD]
  connect_bd_net [get_bd_pins data_xconn/aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk1]
  connect_bd_net [get_bd_pins data_xconn/aresetn] [get_bd_pins clk1_reset/interconnect_aresetn]

  # Add GPIO for LEDs and camera enable lines
  set axi_gpio_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_0 ]
  set_property -dict [ list \
CONFIG.C_IS_DUAL {1} \
CONFIG.C_ALL_OUTPUTS {1} \
CONFIG.C_ALL_OUTPUTS_2 {1} \
CONFIG.C_GPIO_WIDTH {8} \
CONFIG.C_GPIO2_WIDTH {4} \
CONFIG.GPIO_BOARD_INTERFACE {led_8bits} \
 ] $axi_gpio_0

  set led_8bits [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 led_8bits ]
  connect_bd_intf_net -intf_net axi_gpio_0_GPIO [get_bd_intf_ports led_8bits] [get_bd_intf_pins axi_gpio_0/GPIO]

% if board == 'pciecc':
  set CAM_GPIO [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 CAM_GPIO ]
  connect_bd_intf_net -intf_net axi_gpio_0_GPIO2 [get_bd_intf_ports CAM_GPIO] [get_bd_intf_pins axi_gpio_0/GPIO2]
% else:
  set_property -dict [list CONFIG.C_IS_DUAL {0}] $axi_gpio_0
%endif

  # Automagically connect to control bus
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Master /zynq_ultra_ps_e_0/M_AXI_HPM0_LPD intc_ip /control_xconn Clk_xbar Auto Clk_master $controlclk Clk_slave $controlclk "  [get_bd_intf_pins axi_gpio_0/S_AXI]


  # Add GPIO for controlling AxPROT for coherency
  set axi_gpio_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_1 ]
  set_property -dict [ list \
CONFIG.C_ALL_OUTPUTS {1} \
CONFIG.C_GPIO_WIDTH {3} \
 ] $axi_gpio_1

  # Connect to AWPROT/ARPROT for AXI HPC port
  connect_bd_net -net axi_gpio_1_gpio_io_o [get_bd_pins axi_gpio_1/gpio_io_o] [get_bd_pins zynq_ultra_ps_e_0/saxigp0_arprot] [get_bd_pins zynq_ultra_ps_e_0/saxigp0_awprot]

  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Master /zynq_ultra_ps_e_0/M_AXI_HPM0_LPD intc_ip /control_xconn Clk_xbar Auto Clk_master $controlclk Clk_slave $controlclk "  [get_bd_intf_pins axi_gpio_1/S_AXI]

% if board == 'pciecc':
  # Add I2C controller for cameras (and other I2C stuff on the FMC)
  set FMC_IIC [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:iic_rtl:1.0 FMC_IIC ]
  set camera_iic [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_iic:2.0 camera_iic ]
  connect_bd_intf_net -intf_net camera_iic_IIC [get_bd_intf_ports FMC_IIC] [get_bd_intf_pins camera_iic/IIC]
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Master /zynq_ultra_ps_e_0/M_AXI_HPM0_LPD intc_ip /control_xconn Clk_xbar Auto Clk_master $controlclk Clk_slave $controlclk"  [get_bd_intf_pins camera_iic/S_AXI]
% endif


  # Set up the DisplayPort connections
  set idt5901 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 idt5901 ]
  create_hier_cell_idt5901_clk [current_bd_instance .] idt5901_clk
  connect_bd_intf_net -intf_net CLK_IN_D_1 [get_bd_intf_ports idt5901] [get_bd_intf_pins idt5901_clk/CLK_IN_D]
  connect_bd_net -net video_clk_1 [get_bd_pins idt5901_clk/BUFG_O] [get_bd_pins zynq_ultra_ps_e_0/dp_video_in_clk]


  # End of core design

  # Beginning of processing peripheral section
  # This works in two passes:
  # 1) Instantiate and configure all the nodes
  # 2) Connect the nodes, now that everything exists

  # PASS 1: instantiate and configure nodes
  ##
  ## We'll use this to track how many IRQs we need, and handle them at the end
  <% irqs = [] %>

% for module in hw:
  % if module['type'] == 'csi':

  # Create and configure the DPHY receiver
  set ${module['name']}_dphy [ create_bd_cell -type ip -vlnv xilinx.com:ip:mipi_dphy:3.1 ${module['name']}_dphy ]
  set_property -dict [ list \
    CONFIG.C_DATA_LANE1_IO_POSITION {4} \
    CONFIG.C_DPHY_LANES {2} \
    CONFIG.C_DPHY_MODE {SLAVE} \
    CONFIG.C_INIT {100000} \
    CONFIG.DATA_LANE0_BYTE {All_Byte} \
    CONFIG.DATA_LANE1_BYTE {All_Byte} \
    CONFIG.DATA_LANE2_BYTE {All_Byte} \
    CONFIG.DATA_LANE3_BYTE {All_Byte} \
    CONFIG.SupportLevel {1} \
    CONFIG.CLK_LANE_IO_LOC {${module['clk_loc']}} \
    CONFIG.DATA_LANE0_IO_LOC {${module['data0_loc']}} \
    CONFIG.DATA_LANE1_IO_LOC {${module['data1_loc']}} \
     ] $${module['name']}_dphy
## TODO: to save resources, we could not put the PLL in each dphy block

  # Connect clk and reset
  connect_bd_net [get_bd_pins ${module['name']}_dphy/core_clk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk2]
  connect_bd_net [get_bd_pins ${module['name']}_dphy/core_rst] [get_bd_pins clk2_reset/peripheral_reset]


  # Input ports for DPHY
  set ${module['name']}_dphy_clk_rxn [ create_bd_port -dir I ${module['name']}_dphy_clk_rxn ]
  set ${module['name']}_dphy_clk_rxp [ create_bd_port -dir I ${module['name']}_dphy_clk_rxp ]
  set ${module['name']}_dphy_data_rxn [ create_bd_port -dir I -from 1 -to 0 ${module['name']}_dphy_data_rxn ]
  set ${module['name']}_dphy_data_rxp [ create_bd_port -dir I -from 1 -to 0 ${module['name']}_dphy_data_rxp ]

  connect_bd_net [get_bd_ports ${module['name']}_dphy_clk_rxn] [get_bd_pins ${module['name']}_dphy/clk_rxn]
  connect_bd_net [get_bd_ports ${module['name']}_dphy_clk_rxp] [get_bd_pins ${module['name']}_dphy/clk_rxp]
  connect_bd_net [get_bd_ports ${module['name']}_dphy_data_rxn] [get_bd_pins ${module['name']}_dphy/data_rxn]
  connect_bd_net [get_bd_ports ${module['name']}_dphy_data_rxp] [get_bd_pins ${module['name']}_dphy/data_rxp]

  # Create the csirx module
  set_property ip_repo_paths "${module['path']} [get_property  ip_repo_paths [current_project]]" [current_project]
  update_ip_catalog  
  set ${module['name']}_csirx [ create_bd_cell -type ip -vlnv ${module['vlnv']} ${module['name']}_csirx ]

  <% irqs.append(module['name'] + "_csirx/csi_intr")  %>

  # Create reset for the byte clock
  set ${module['name']}_reset [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 ${module['name']}_reset ]
  connect_bd_net [get_bd_pins ${module['name']}_reset/slowest_sync_clk] [get_bd_pins ${module['name']}_dphy/rxbyteclkhs]
  connect_bd_net [get_bd_pins ${module['name']}_reset/ext_reset_in] [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0]

  # Connect byte clock and reset for csirx
  connect_bd_net [get_bd_pins ${module['name']}_dphy/rxbyteclkhs] [get_bd_pins ${module['name']}_csirx/ppi_rxbyteclkhs_clk]
  connect_bd_net [get_bd_pins ${module['name']}_csirx/rxbyteclkhs_resetn] [get_bd_pins ${module['name']}_reset/peripheral_aresetn]

  # Connect AXI-lite for csirx
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Master /zynq_ultra_ps_e_0/M_AXI_HPM0_LPD intc_ip /control_xconn Clk_xbar Auto Clk_master $controlclk Clk_slave $controlclk "  [get_bd_intf_pins ${module['name']}_csirx/RegSpace_S_AXI]

  # And connect the PPI
  connect_bd_intf_net [get_bd_intf_pins ${module['name']}_dphy/rx_mipi_ppi_if] [get_bd_intf_pins ${module['name']}_csirx/rx_mipi_ppi_if_1]

  # Connect FIFO to handle clock crossing
  set ${module['name']}_fifo [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:1.1 ${module['name']}_fifo ]
  set_property -dict [list CONFIG.FIFO_DEPTH {256} CONFIG.IS_ACLK_ASYNC {1} ] $${module['name']}_fifo
  connect_bd_intf_net [get_bd_intf_pins ${module['name']}_fifo/S_AXIS] [get_bd_intf_pins ${module['name']}_csirx/Output_M_AXIS]

  connect_bd_net [get_bd_pins ${module['name']}_fifo/s_axis_aclk] [get_bd_pins ${module['name']}_dphy/rxbyteclkhs]
  connect_bd_net [get_bd_pins ${module['name']}_fifo/s_axis_aresetn] [get_bd_pins ${module['name']}_reset/peripheral_aresetn]
  connect_bd_net [get_bd_pins ${module['name']}_fifo/m_axis_aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk1]
  connect_bd_net [get_bd_pins ${module['name']}_fifo/m_axis_aresetn] [get_bd_pins clk1_reset/peripheral_aresetn]

  # And finally a dwidth-converter to drop down to 1 pix/cycle
  set ${module['name']}_dwidth [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 ${module['name']}_dwidth ]
  set_property -dict [ list CONFIG.M_TDATA_NUM_BYTES {2} ] $${module['name']}_dwidth

  connect_bd_intf_net [get_bd_intf_pins ${module['name']}_fifo/M_AXIS] [get_bd_intf_pins ${module['name']}_dwidth/S_AXIS]
  connect_bd_net [get_bd_pins ${module['name']}_dwidth/aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk1]
  connect_bd_net [get_bd_pins ${module['name']}_dwidth/aresetn] [get_bd_pins clk1_reset/peripheral_aresetn]

  % elif module['type'] == 'dma':
  # Instantiate the DMA
  set ${module['name']} [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.1 ${module['name']} ]
  set_property -dict [ list \
CONFIG.c_enable_multi_channel {1} \
CONFIG.c_include_mm2s {0} \
CONFIG.c_sg_include_stscntrl_strm {0} \
 ] $${module['name']}

  # Connect AXI-lite and scatter-gather
  # Leave the others disconnected, since they may not exist...
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Master /zynq_ultra_ps_e_0/M_AXI_HPM0_LPD intc_ip /control_xconn Clk_xbar Auto Clk_master $controlclk Clk_slave $controlclk "  [get_bd_intf_pins ${module['name']}/S_AXI_LITE]
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Slave /zynq_ultra_ps_e_0/S_AXI_HPC0_FPD intc_ip /data_xconn Clk_xbar $dataclk Clk_master $dataclk Clk_slave $dataclk "  [get_bd_intf_pins ${module['name']}/M_AXI_SG]

  % elif module['type'] == 'hls':

  # Instantiate the HLS module
  set_property ip_repo_paths "${module['path']} [get_property  ip_repo_paths [current_project]]" [current_project]
  update_ip_catalog  
  set ${module['name']} [ create_bd_cell -type ip -vlnv ${module['vlnv']} ${module['name']} ]

  # Use block automation, but connect the clock to the high-speed data clock
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Master /zynq_ultra_ps_e_0/M_AXI_HPM0_LPD intc_ip /control_xconn Clk_xbar Auto Clk_master $controlclk Clk_slave $dataclk"  [get_bd_intf_pins ${module['name']}/s_axi_config]

  % for stream in module['streams']:
  # AXI-stream interface ${stream['name']}
    % if stream['type'] == 'input':
      % if stream['depth'] != 2:
  # Input stream width is ${stream['depth']}, so we need a dwidth converter
  <% stream['dwc'] = module['name'] + '_dwidth_' + stream['name'] %>
  set ${stream['dwc']} [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 ${stream['dwc']} ]
  set_property -dict [ list \
      CONFIG.M_TDATA_NUM_BYTES {2} \
      CONFIG.S_TDATA_NUM_BYTES {${stream['depth']}} \
    ] $${stream['dwc']}

  connect_bd_intf_net -intf_net ${stream['dwc']}_M_AXIS [get_bd_intf_pins ${stream['dwc']}/M_AXIS] [get_bd_intf_pins ${module['name']}/${stream['name']}]
  connect_bd_net [get_bd_pins ${stream['dwc']}/aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk1]
  connect_bd_net [get_bd_pins ${stream['dwc']}/aresetn] [get_bd_pins clk1_reset/peripheral_aresetn]

  set ${module['name']}_${stream['name']}_connection ${stream['dwc']}/S_AXIS
      % else:
  # Connect the IP core directly to the DMA
  set ${module['name']}_${stream['name']}_connection ${module['name']}/${stream['name']}
      % endif  
    % elif stream['type'] == 'output':
      % if stream['depth'] != 2:
  # Output stream width is ${stream['depth']}, so we need a dwidth converter
  <% stream['dwc'] = module['name'] + '_dwidth_' + stream['name'] %>
  set ${stream['dwc']} [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 ${stream['dwc']} ]
  set_property -dict [ list \
      CONFIG.M_TDATA_NUM_BYTES {2} \
      CONFIG.S_TDATA_NUM_BYTES {${stream['depth']}} \
    ] $${stream['dwc']}

  connect_bd_intf_net -intf_net ${module['name']}_M_AXIS [get_bd_intf_pins ${module['name']}/${stream['name']}] [get_bd_intf_pins ${stream['dwc']}/S_AXIS]
  connect_bd_net [get_bd_pins ${stream['dwc']}/aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk1]
  connect_bd_net [get_bd_pins ${stream['dwc']}/aresetn] [get_bd_pins clk1_reset/peripheral_aresetn]

  set ${module['name']}_${stream['name']}_connection ${stream['dwc']}/M_AXIS
      % else:
  # Connect the IP core directly to the DMA
  set ${module['name']}_${stream['name']}_connection ${module['name']}/${stream['name']}
      % endif  
    % endif
  % endfor

  % endif
  ## Any other type just gets ignored

% endfor


  # PASS 2: connect nodes (and configure DMA engines apppropriately)
% for module in hw:
  # Connect ${module['name']}
  ## If the module connects to something, then connect it. :-)
  % if module.has_key('outputto'):
<%
  targetmodule = module['outputto'].split('.')[0]
  targets = [t for t in hw if t['name'] == targetmodule]
  connection = targets[0]
 %>
    # First prepare the destination
    % if connection['type'] == 'dma':
      ## Configure the DMA S2MM channel
  set_property -dict [list CONFIG.c_include_s2mm {1}] [get_bd_cells ${connection['name']}]
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Slave /zynq_ultra_ps_e_0/S_AXI_HPC0_FPD intc_ip /data_xconn Clk_xbar $dataclk Clk_master $dataclk Clk_slave $dataclk "  [get_bd_intf_pins ${connection['name']}/M_AXI_S2MM]
  set dstpin ${connection['name']}/S_AXIS_S2MM
  <% irqs.append(connection['name'] + "/s2mm_introut")  %>

    % elif connection['type'] == 'hls':
## HACK: just get the first input
<%
  streams = [s for s in connection['streams'] if s['type'] == 'input']
  stream = streams[0]
%>
  set dstpin $${connection['name']}_${stream['name']}_connection
    %endif
    ## That's it; only HLS and DMA can take input

    
    % if module['type'] == 'dma':
      ## Configure the DMA MM2S channel
  set_property -dict [list CONFIG.c_include_mm2s {1} CONFIG.c_m_axis_mm2s_tdata_width {16}] [get_bd_cells ${module['name']}]
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config "Slave /zynq_ultra_ps_e_0/S_AXI_HPC0_FPD intc_ip /data_xconn Clk_xbar $dataclk Clk_master $dataclk Clk_slave $dataclk "  [get_bd_intf_pins ${module['name']}/M_AXI_MM2S]
  set srcpin ${module['name']}/M_AXIS_MM2S
  <% irqs.append(module['name'] + "/mm2s_introut")  %>

    % elif module['type'] == 'hls':
## HACK: just get the first output
<%
  streams = [s for s in module['streams'] if s['type'] == 'output']
  stream = streams[0]
%>
  set srcpin $${module['name']}_${stream['name']}_connection

    % elif module['type'] == 'csi':
  set srcpin ${module['name']}_dwidth/M_AXIS
    %endif

  # Now make the actual connection
  connect_bd_intf_net [get_bd_intf_pins $srcpin] [get_bd_intf_pins $dstpin]

  % endif
% endfor


  # Connect the interrupts
  set irqconcat [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 irqconcat ]
  set_property -dict [list CONFIG.NUM_PORTS {${len(irqs)}}] [get_bd_cells irqconcat]
  connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_ps_irq0] [get_bd_pins irqconcat/dout]

% for irq in irqs:
  connect_bd_net [get_bd_pins ${irq}] [get_bd_pins irqconcat/In${loop.index}]
% endfor

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design

# Create the HDL wrapper
set wrapper [make_wrapper -files [get_files $project_dir/$project_name.srcs/sources_1/bd/$bd_name/$bd_name.bd] -top]
add_files -norecurse $wrapper
update_compile_order -fileset sources_1

# Adds the constraint files
% if board == 'iocc':
import_files -fileset constrs_1 -norecurse ${pwd}/constrs/dpclock_iocc.xdc
# User will have to desicde what to do with I2C and GPIO
% elif board == 'pciecc':
import_files -fileset constrs_1 -norecurse ${pwd}/constrs/dpclock_pciecc.xdc
import_files -fileset constrs_1 -norecurse ${pwd}/constrs/csimodules.xdc
% endif

# Kick off the bitstream generation
launch_runs impl_1 -to_step write_bitstream -jobs 12
wait_on_run impl_1

# Export the hdf file
file mkdir ./${name}/${name}.sdk
## This requires a little hackery since ${ will get picked up by Mako
file copy -force ./${name}/${name}.runs/impl_1/<%text>${bd_name}</%text>_wrapper.sysdef ./${name}/${name}.sdk/

