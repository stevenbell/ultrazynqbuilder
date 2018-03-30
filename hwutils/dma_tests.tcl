# Tools for probing and debugging DMA engines on Zynq Ultrascale
# Steven Bell <sebell@stanford.edu>

puts "*** Ultrazynqbuilder DMA testing utilities ***"
puts "Commands:"
puts "  dma ADDR # Prints human-readable status of a DMA engine"
puts "  sg ADDR # Parses and displays a scatter-gather descriptor"
puts "  sg2d_s2mm ADDR # Parses and displays a 2D scatter-gather descriptor"

# strip takes a register/value string from XMD and returns just the value
proc strip {input} {
  return [string range $input 12 19]
}

# Parse a scatter-gather descriptor and print it in a human-readable way
proc sg {sg_base} {
  puts "SG descriptor $sg_base$"
  puts [mrd -force $sg_base 10];
  set DESC_CTRL [mrd -force [expr {"$sg_base" + 0x18}]];
  set DESC_CTRL [strip $DESC_CTRL];
  puts "  Control: $DESC_CTRL"
  if {[expr 0x$DESC_CTRL & 0x08000000]} {
    puts "    TX Start-of-frame";
  }
  if {[expr 0x$DESC_CTRL & 0x04000000]} {
    puts "    TX End-of-frame";
  }
  puts "    Length: [string range $DESC_CTRL 3 7]";
  
  set DESC_STATUS [mrd -force [expr {"$sg_base" + 0x1C}]];
  set DESC_STATUS [strip $DESC_STATUS];
  
  puts "  Status: $DESC_STATUS"
  if {[expr 0x$DESC_STATUS & 0x80000000]} {
    puts "    Completed";
  }
  if {[expr 0x$DESC_STATUS & 0x40000000]} {
    puts "    DMADecErr";
  }
  if {[expr 0x$DESC_STATUS & 0x20000000]} {
    puts "    DMASlaveErr";
  }
  if {[expr 0x$DESC_STATUS & 0x10000000]} {
    puts "    DMAIntErr";
  }
  puts "    Transferred: [string range $DESC_STATUS 3 7]";
}

# Parse a 2D scatter-gather descriptor and print it in a human-readable way
proc sg2d_s2mm {sg_base} {
  puts "2D SG descriptor $sg_base$"
  puts [mrd $sg_base 10];
#   set DESC_CTRL [mrd [expr {"$sg_base" + 0x18}]];
#   set DESC_CTRL [strip $DESC_CTRL];
#   puts "  Control: $DESC_CTRL"
#   if {[expr 0x$DESC_CTRL & 0x08000000]} {
#     puts "    TX Start-of-packet";
#   }
#   if {[expr 0x$DESC_CTRL & 0x04000000]} {
#     puts "    TX End-of-packet";
#   }
#   puts "    Length: [string range $DESC_CTRL 3 7]";
#   
  set DESC_STATUS [mrd [expr {"$sg_base" + 0x1C}]];
  set DESC_STATUS [strip $DESC_STATUS];

  puts "  Status: $DESC_STATUS"
  if {[expr 0x$DESC_STATUS & 0x80000000]} {
    puts "    Completed";
  }
  if {[expr 0x$DESC_STATUS & 0x40000000]} {
    puts "    DMADecErr";
  }
  if {[expr 0x$DESC_STATUS & 0x20000000]} {
    puts "    DMASlaveErr";
  }
  if {[expr 0x$DESC_STATUS & 0x10000000]} {
    puts "    DMAIntErr";
  }
#   puts "    Transferred: [string range $DESC_STATUS 3 7]";
}


# Parse all the bits in the DMA register space, and print them in a
# human-readable way.
# This assumes that the MM2S and S2MM channels are identical but separate;
# to print both you'll need to call this twice with the base address and
# then offset of 0x30.
proc dma {address} {
  set DMACR [mrd -force [expr {$address + 0x00}]];
  set DMACR [strip $DMACR];
  puts "DMACR: $DMACR";
  if {[expr 0x$DMACR % 2] == 1} {
    puts "  run";
  } else {
    puts "  stop";
  }
  
  set DMASR [mrd -force [expr {$address + 0x04}]];
  set DMASR [strip $DMASR];
  puts "DMASR: $DMASR"
  if {[expr 0x$DMASR & 0x00000001]} {
    puts "  Halted";
  } else {
    puts "  Running";
  }
  if {[expr 0x$DMASR & 0x00000002]} {
    puts "  Idle";
  } else {
    puts "  Not idle";
  }
  if {[expr 0x$DMASR & 0x00000008]} {
    puts "  Scatter-gather mode";
  } else {
    puts "  Simple DMA mode";
  }
  
  puts "  error: [string range $DMASR 4 6]"
  if {[expr 0x$DMASR & 0x00004000]} {
    puts "    Error interrupt occured";
  }
  if {[expr 0x$DMASR & 0x00001000]} {
    puts "    IOC_Irq: Completed descriptor";
  }
  if {[expr 0x$DMASR & 0x00000200]} {
    puts "    DMADecErr: SG slave error";
  }
  if {[expr 0x$DMASR & 0x00000100]} {
    puts "    DMADecErr: SG internal error";
  }
  if {[expr 0x$DMASR & 0x00000040]} {
    puts "    DMADecErr: Decode error";
  }
  if {[expr 0x$DMASR & 0x00000020]} {
    puts "    DMASlvErr";
  }

  set CURDESC [mrd -force [expr {$address + 0x08}]];
  set CURDESC [strip $CURDESC];
  puts "CURDESC: $CURDESC"
}

