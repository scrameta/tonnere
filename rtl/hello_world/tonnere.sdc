create_clock -period 27MHz [get_ports CLK27_A12]
create_clock -period 27MHz [get_ports PLL1[0]]
create_clock -period 27MHz [get_ports PLL1[1]]
create_clock -period 27MHz [get_ports PLL1[2]]
create_clock -period 27MHz [get_ports PLL2[0]]
create_clock -period 27MHz [get_ports PLL2[1]]
create_clock -period 27MHz [get_ports PLL2[2]]

derive_pll_clocks
derive_clock_uncertainty

set_clock_groups -asynchronous \
  -group { CLK27_A12 } \
  -group { PLL1[0] } \
  -group { PLL1[1] } \
  -group { PLL1[2] } \
  -group { PLL2[0] } \
  -group { PLL2[1] } \
  -group { PLL2[2] } \
  -group { pll_vdac1|altpll_component|auto_generated|pll1|clk[0] } \
  -group { pll_video1|altpll_component|auto_generated|pll1|clk[0] } \
  -group { pll_video1|altpll_component|auto_generated|pll1|clk[1] } \
  -group { pll_hdmi1|altpll_component|auto_generated|pll1|clk[0] \
           pll_hdmi1|altpll_component|auto_generated|pll1|clk[1] } 



# Waive port rate check on DAC outputs.
# Slow/100C model is overly pessimistic for 5-40C operating range.
# THS7316 36MHz reconstruction filter absorbs any marginal edge timing.
#set_max_delay -to [get_ports {VDAC_R VDAC_G VDAC_B}] 4.210

#  -group { \
#    pll_acore_inst|pll_acore_inst|altera_pll_i|cyclonev_pll|counter[0].output_counter|divclk \
#    pll_acore_inst|pll_acore_inst|altera_pll_i|cyclonev_pll|counter[1].output_counter|divclk \
#    pll_acore_inst|pll_acore_inst|altera_pll_i|cyclonev_pll|counter[2].output_counter|divclk \
#    pll_acore_inst|pll_acore_inst|altera_pll_i|cyclonev_pll|counter[3].output_counter|divclk \
#    pll_acore_inst|pll_acore_inst|altera_pll_i|cyclonev_pll|fpll_0|fpll|vcoph[0]  \
#  } \
#  -group { \
#    pll_hdmi_inst|pll_hdmi_inst|altera_pll_i|general[0].gpll~FRACTIONAL_PLL|vcoph[0] \
#    pll_hdmi_inst|pll_hdmi_inst|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk \
#    pll_hdmi_inst|pll_hdmi_inst|altera_pll_i|general[1].gpll~PLL_OUTPUT_COUNTER|divclk \
#  } \
#  -group { \
#    pllusbinstance|pll_usb_inst|altera_pll_i|general[0].gpll~FRACTIONAL_PLL|vcoph[0] \
#    pllusbinstance|pll_usb_inst|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk \
#  }

#set_max_skew -to [get_ports {USB1DP USB1DM}] 1
#set_max_skew -to [get_ports {USB2DP USB2DM}] 1

#create_generated_clock -name sdram_clk -source pll_acore_inst|pll_acore_inst|altera_pll_i|cyclonev_pll|counter[1].output_counter|divclk
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_DQ[*]]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_DQ[*]] 
#
#set_input_delay -clock sdram_clk -max 6.0 [get_ports DRAM_DQ[*]]
#set_input_delay -clock sdram_clk -min 0.0 [get_ports DRAM_DQ[*]] 
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_ADDR[*]]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_ADDR[*]] 
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_BA_0]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_BA_0] 
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_BA_1]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_BA_1] 
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_RAS_N]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_RAS_N]
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_CAS_N]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_CAS_N]
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_WE_N]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_WE_N]
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_LDQM]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_LDQM]
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_UDQM]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_UDQM]
#
#set_output_delay -clock sdram_clk -max 6.0 [get_ports DRAM_CKE]
#set_output_delay -clock sdram_clk -min -1.0 [get_ports DRAM_CKE]
#
