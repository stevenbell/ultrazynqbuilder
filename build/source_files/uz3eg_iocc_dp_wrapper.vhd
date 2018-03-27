--Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2017.2 (lin64) Build 1909853 Thu Jun 15 18:39:10 MDT 2017
--Date        : Thu Mar 22 18:24:40 2018
--Host        : kiwi running 64-bit Ubuntu 16.04.3 LTS
--Command     : generate_target uz3eg_iocc_dp_wrapper.bd
--Design      : uz3eg_iocc_dp_wrapper
--Purpose     : IP block netlist
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
library UNISIM;
use UNISIM.VCOMPONENTS.ALL;
entity uz3eg_iocc_dp_wrapper is
  port (
    dip_switches_8bits_tri_i : in STD_LOGIC_VECTOR ( 7 downto 0 );
    idt5901_clk_n : in STD_LOGIC_VECTOR ( 0 to 0 );
    idt5901_clk_p : in STD_LOGIC_VECTOR ( 0 to 0 );
    led_8bits_tri_o : out STD_LOGIC_VECTOR ( 7 downto 0 );
    push_buttons_3bits_tri_i : in STD_LOGIC_VECTOR ( 2 downto 0 )
  );
end uz3eg_iocc_dp_wrapper;

architecture STRUCTURE of uz3eg_iocc_dp_wrapper is
  component uz3eg_iocc_dp is
  port (
    dip_switches_8bits_tri_i : in STD_LOGIC_VECTOR ( 7 downto 0 );
    led_8bits_tri_o : out STD_LOGIC_VECTOR ( 7 downto 0 );
    push_buttons_3bits_tri_i : in STD_LOGIC_VECTOR ( 2 downto 0 );
    idt5901_clk_p : in STD_LOGIC_VECTOR ( 0 to 0 );
    idt5901_clk_n : in STD_LOGIC_VECTOR ( 0 to 0 )
  );
  end component uz3eg_iocc_dp;
begin
uz3eg_iocc_dp_i: component uz3eg_iocc_dp
     port map (
      dip_switches_8bits_tri_i(7 downto 0) => dip_switches_8bits_tri_i(7 downto 0),
      idt5901_clk_n(0) => idt5901_clk_n(0),
      idt5901_clk_p(0) => idt5901_clk_p(0),
      led_8bits_tri_o(7 downto 0) => led_8bits_tri_o(7 downto 0),
      push_buttons_3bits_tri_i(2 downto 0) => push_buttons_3bits_tri_i(2 downto 0)
    );
end STRUCTURE;
