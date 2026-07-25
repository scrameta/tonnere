/*
 * Si5351A Rev B Configuration Register Export Header File
 *
 * This file represents a series of Silicon Labs Si5351A Rev B 
 * register writes that can be performed to load a single configuration 
 * on a device. It was created by a Silicon Labs ClockBuilder Pro
 * export tool.
 *
 * Part:		                                       Si5351A Rev B
 * Design ID:                                          
 * Includes Pre/Post Download Control Register Writes: Yes
 * Created By:                                         ClockBuilder Pro v2.21 [2018-01-19]
 * Timestamp:                                          2018-02-04 22:19:44 GMT+01:00
 *
 * A complete design report corresponding to this export is included at the end 
 * of this header file.
 *
 */

#ifndef SI5351A_REVB_REG_CONFIG_HEADER
#define SI5351A_REVB_REG_CONFIG_HEADER

#define SI5351A_REVB_REG_CONFIG_NUM_REGS				55

typedef struct
{
	unsigned char address; /* 16-bit register address */
	unsigned char value; /* 8-bit register data */

} si5351a_revb_register_t;

si5351a_revb_register_t const si5351a_revb_registers[SI5351A_REVB_REG_CONFIG_NUM_REGS] =
{
	{ 0x02, 0x53 },
	{ 0x03, 0x00 },
	{ 0x07, 0x00 },
	{ 0x0F, 0x00 },
	{ 0x10, 0x0D },
	{ 0x11, 0x8C },
	{ 0x12, 0x0D },
	{ 0x13, 0x8C },
	{ 0x14, 0x8C },
	{ 0x15, 0x8C },
	{ 0x16, 0x8C },
	{ 0x17, 0x8C },
	{ 0x1A, 0x00 },
	{ 0x1B, 0x03 },
	{ 0x1C, 0x00 },
	{ 0x1D, 0x0E },
	{ 0x1E, 0xAA },
	{ 0x1F, 0x00 },
	{ 0x20, 0x00 },
	{ 0x21, 0x02 },
	{ 0x2A, 0x00 },
	{ 0x2B, 0x01 },
	{ 0x2C, 0x00 },
	{ 0x2D, 0x2B },
	{ 0x2E, 0x00 },
	{ 0x2F, 0x00 },
	{ 0x30, 0x00 },
	{ 0x31, 0x00 },
	{ 0x3A, 0x00 },
	{ 0x3B, 0x01 },
	{ 0x3C, 0x00 },
	{ 0x3D, 0x0D },
	{ 0x3E, 0x00 },
	{ 0x3F, 0x00 },
	{ 0x40, 0x00 },
	{ 0x41, 0x00 },
	{ 0x5A, 0x00 },
	{ 0x5B, 0x00 },
	{ 0x95, 0x81 },
	{ 0x96, 0x46 },
	{ 0x97, 0x7F },
	{ 0x98, 0xFF },
	{ 0x99, 0x00 },
	{ 0x9A, 0x00 },
	{ 0x9B, 0xD6 },
	{ 0x9C, 0x00 },
	{ 0x9D, 0x00 },
	{ 0x9E, 0x00 },
	{ 0x9F, 0x01 },
	{ 0xA0, 0x00 },
	{ 0xA1, 0x00 },
	{ 0xA2, 0x00 },
	{ 0xA3, 0x00 },
	{ 0xA4, 0x00 },
	{ 0xB7, 0x92 },

};

/*
 * Design Report
 *
 * Overview
 * ========
 * Part:         Si5351A
 * Project File: C:\Users\Mark\Documents\Si5351A-RevB-Project.slabtimeproj
 * Created By:   ClockBuilder Pro v2.21 [2018-01-19]
 * Timestamp:    2018-02-04 22:19:44 GMT+01:00
 * 
 * Design Rule Check
 * =================
 * Errors:
 * - No errors
 * 
 * Warnings:
 * - No warnings
 * 
 * Design
 * ======
 * Inputs:
 *     IN0: 27 MHz
 * 
 * Outputs:
 *    OUT0: 10 MHz
 *          Enabled LVCMOS 4 mA
 *          Offset 0.000 s 
 *    OUT1: Unused
 *    OUT2: 30 MHz
 *          Enabled LVCMOS 4 mA
 *          Offset 0.000 s 
 * 
 * Frequency Plan
 * ==============
 * PLL_A:
 *    Enabled Features = SpreadSpectrum
 *    Fvco             = 900 MHz
 *    M                = 33.3333333333333333... [ 33 + 1/3 ]
 *    Input0:
 *       Source           = Crystal
 *       Source Frequency = 27 MHz
 *       Fpfd             = 27 MHz
 *       Load Capacitance = Load_08pF
 *    Output0:
 *       Features       = SpreadSpectrum
 *       Disabled State = StopLow
 *       R              = 1  (2^0)
 *       Fout           = 10 MHz
 *       N              = 90
 *    Output2:
 *       Features       = SpreadSpectrum
 *       Disabled State = StopLow
 *       R              = 1  (2^0)
 *       Fout           = 30 MHz
 *       N              = 30
 * 
 * Settings
 * ========
 * 
 * Location      Setting Name   Decimal Value      Hex Value        
 * ------------  -------------  -----------------  -----------------
 * 0x0002[3]     XO_LOS_MASK    0                  0x0              
 * 0x0002[4]     CLK_LOS_MASK   1                  0x1              
 * 0x0002[5]     LOL_A_MASK     0                  0x0              
 * 0x0002[6]     LOL_B_MASK     1                  0x1              
 * 0x0002[7]     SYS_INIT_MASK  0                  0x0              
 * 0x0003[7:0]   CLK_OEB        0                  0x00             
 * 0x0007[7:4]   I2C_ADDR_CTRL  0                  0x0              
 * 0x000F[2]     PLLA_SRC       0                  0x0              
 * 0x000F[3]     PLLB_SRC       0                  0x0              
 * 0x000F[4]     PLLA_INSELB    0                  0x0              
 * 0x000F[5]     PLLB_INSELB    0                  0x0              
 * 0x000F[7:6]   CLKIN_DIV      0                  0x0              
 * 0x0010[1:0]   CLK0_IDRV      1                  0x1              
 * 0x0010[3:2]   CLK0_SRC       3                  0x3              
 * 0x0010[4]     CLK0_INV       0                  0x0              
 * 0x0010[5]     MS0_SRC        0                  0x0              
 * 0x0010[6]     MS0_INT        0                  0x0              
 * 0x0010[7]     CLK0_PDN       0                  0x0              
 * 0x0011[1:0]   CLK1_IDRV      0                  0x0              
 * 0x0011[3:2]   CLK1_SRC       3                  0x3              
 * 0x0011[4]     CLK1_INV       0                  0x0              
 * 0x0011[5]     MS1_SRC        0                  0x0              
 * 0x0011[6]     MS1_INT        0                  0x0              
 * 0x0011[7]     CLK1_PDN       1                  0x1              
 * 0x0012[1:0]   CLK2_IDRV      1                  0x1              
 * 0x0012[3:2]   CLK2_SRC       3                  0x3              
 * 0x0012[4]     CLK2_INV       0                  0x0              
 * 0x0012[5]     MS2_SRC        0                  0x0              
 * 0x0012[6]     MS2_INT        0                  0x0              
 * 0x0012[7]     CLK2_PDN       0                  0x0              
 * 0x0013[1:0]   CLK3_IDRV      0                  0x0              
 * 0x0013[3:2]   CLK3_SRC       3                  0x3              
 * 0x0013[4]     CLK3_INV       0                  0x0              
 * 0x0013[5]     MS3_SRC        0                  0x0              
 * 0x0013[6]     MS3_INT        0                  0x0              
 * 0x0013[7]     CLK3_PDN       1                  0x1              
 * 0x0014[1:0]   CLK4_IDRV      0                  0x0              
 * 0x0014[3:2]   CLK4_SRC       3                  0x3              
 * 0x0014[4]     CLK4_INV       0                  0x0              
 * 0x0014[5]     MS4_SRC        0                  0x0              
 * 0x0014[6]     MS4_INT        0                  0x0              
 * 0x0014[7]     CLK4_PDN       1                  0x1              
 * 0x0015[1:0]   CLK5_IDRV      0                  0x0              
 * 0x0015[3:2]   CLK5_SRC       3                  0x3              
 * 0x0015[4]     CLK5_INV       0                  0x0              
 * 0x0015[5]     MS5_SRC        0                  0x0              
 * 0x0015[6]     MS5_INT        0                  0x0              
 * 0x0015[7]     CLK5_PDN       1                  0x1              
 * 0x0016[1:0]   CLK6_IDRV      0                  0x0              
 * 0x0016[3:2]   CLK6_SRC       3                  0x3              
 * 0x0016[4]     CLK6_INV       0                  0x0              
 * 0x0016[5]     MS6_SRC        0                  0x0              
 * 0x0016[6]     FBA_INT        0                  0x0              
 * 0x0016[7]     CLK6_PDN       1                  0x1              
 * 0x0017[1:0]   CLK7_IDRV      0                  0x0              
 * 0x0017[3:2]   CLK7_SRC       3                  0x3              
 * 0x0017[4]     CLK7_INV       0                  0x0              
 * 0x0017[5]     MS7_SRC        0                  0x0              
 * 0x0017[6]     FBB_INT        0                  0x0              
 * 0x0017[7]     CLK7_PDN       1                  0x1              
 * 0x001C[17:0]  MSNA_P1        3754               0x00EAA          
 * 0x001F[19:0]  MSNA_P2        2                  0x00002          
 * 0x001F[23:4]  MSNA_P3        3                  0x00003          
 * 0x002C[17:0]  MS0_P1         11008              0x02B00          
 * 0x002F[19:0]  MS0_P2         0                  0x00000          
 * 0x002F[23:4]  MS0_P4         1                  0x00001          
 * 0x003C[17:0]  MS2_P1         3328               0x00D00          
 * 0x003F[19:0]  MS2_P2         0                  0x00000          
 * 0x003F[23:4]  MS2_P4         1                  0x00001          
 * 0x005A[7:0]   MS6_P2         0                  0x00             
 * 0x005B[7:0]   MS7_P2         0                  0x00             
 * 0x0095[14:0]  SSDN_P2        326                0x0146           
 * 0x0095[7]     SSC_EN         1                  0x1              
 * 0x0097[14:0]  SSDN_P3        32767              0x7FFF           
 * 0x0097[7]     SSC_MODE       0                  0x0              
 * 0x0099[11:0]  SSDN_P1        0                  0x000            
 * 0x009A[15:4]  SSUDP          214                0x0D6            
 * 0x009C[14:0]  SSUP_P2        0                  0x0000           
 * 0x009E[14:0]  SSUP_P3        1                  0x0001           
 * 0x00A0[11:0]  SSUP_P1        0                  0x000            
 * 0x00A2[21:0]  VCXO_PARAM     0                  0x000000         
 * 0x00B7[7:6]   XTAL_CL        2                  0x2
 * 
 *
 */

#endif

