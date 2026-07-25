#include "pll.h"
#include "regs.h"
#include "integer.h"

void setVideoPLL(uint32_t m,uint32_t c0,uint32_t c1,uint32_t c2,uint32_t c3,uint32_t cp,uint32_t bw,uint32_t mfr)
{
	*pll_m = m;
	*pll_c = c0;
	*pll_c = c1;
	*pll_c = c2;
	*pll_c = c3;
	*pll_chargePump = cp;
	*pll_bandwidth = bw;
	*pll_mFrac = mfr;
	*pll_go = 1;
}

void set_pll(int pal, int hdmi)
{
	switch (pal<<1 | hdmi)
	{
	case 0:
		//ntsc_svideo
		setVideoPLL(0x909,0x404,0x40404,0x80808,0xc1010,0x2,0x7,0x53c7fe31);
		break;

	case 1:
		//ntsc_5994
		//5994! Should be 2700000/525/858 (60/1001) 
		//setVideoPLL(0x20f0e,0x20706,0x60706,0x80d0d,0xc1a1a,0x2,0x7,0xca571058);
		//RAM fails!M=29?? Too high FVCO? setVideoPLL(0x20f0e,0x20706,0x60706,0x80d0d,0xc1a1a,0x2,0x7,0xca590425);
		//RAM fails on proto now? setVideoPLL(0xa0a,0x20504,0x60504,0x80909,0xc1212,0x2,0x7,0x9fc77906);
		setVideoPLL(0x20605,0x20302,0x60302,0x80505,0xc0a0a,0x3,0x8,0x7535EDE7); // lower fVCO seems more stable
		break;
	case 2:
		//pal_svideo
		//RAM fails!!M=29?? Too high FVCO? setVideoPLL(0x20f0e,0x20706,0x60706,0x80d0d,0xc1a1a,0x2,0x7,0x829a43e4);
		//Works, but less accurate setVideoPLL(0x20504,0x202,0x40202,0x80404,0xc0808,0x2,0x7,0x147e3bf0);
		setVideoPLL(0xa0a,0x20504,0x60504,0x80909,0xc1212,0x2,0x7,0x6e1c079e);
		break;

	case 3:
		//pal_50
		setVideoPLL(0x20605,0x20302,0x60302,0x80505,0xc0a0a,0x3,0x8,0x61bb05fb);
		break;

	}
}

//// TODO - menu - need to output. Need to output 'hdmi_activate','vga','scanlines','pal','svideo'
//video settings -> 
//svideo,rgb,vga,hdmi         (2 bits)
//pal/ntsc                    (1 bit)
//scanlines on/off            (1 bit)
//composite on hsync on/off   (1 bit)
//apply...

// flags:
// svideo
// scandouble
// hdmi
// composite_on_hsync
// pal
// scanlines

// realistic modes:
// 00:HDMI 576p(PAL) scanlines off
// 01:HDMI 576p(PAL) scanlines on
// 02:HDMI 480p(NTSC) scanlines off
// 03:HDMI 480p(NTSC) scanlines on
// 04:VGAHV 31Khz PAL scanlines off
// 05:VGAHV 31Khz PAL scanlines on
// 06:VGAC 31Khz PAL scanlines off
// 07:VGAC 31Khz PAL scanlines on
// 08:VGAHV 31Khz NTSC scanlines off
// 09:VGAHV 31Khz NTSC scanlines on
// 10:VGAC 31Khz NTSC scanlines off
// 11:VGAC 31Khz NTSC scanlines on
// 12:VGAHV 15Khz PAL
// 13:VGAC 15Khz PAL 
// 14:VGAHV 15Khz NTSC
// 15:VGAC 15Khz NTSC
// 16:SVIDEO 15Khz PAL
// 17:SVIDEO 15Khz NTSC


