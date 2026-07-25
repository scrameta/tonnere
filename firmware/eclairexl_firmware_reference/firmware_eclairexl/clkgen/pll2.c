#include "pll2.h"
#include "Si5351A-RevB-Registers.h"
#include "regs.h"
#include "integer.h"

void set_pll2()
{
	//wait_us(5000000);
	int i;
	unsigned int slave = 0x60; 
	unsigned int read = 0x1;
	unsigned int write = 0x0;

	if (1==*zpu_board)
		return;

	for (i=0;i!=SI5351A_REVB_REG_CONFIG_NUM_REGS;++i)
	{
		unsigned int addr = si5351a_revb_registers[i].address;
		unsigned int value = si5351a_revb_registers[i].value;

		*zpu_i2c_0 = (slave<<9) | write<<8 | addr;  // write addr (blocks until accepted)
		*zpu_i2c_0 = (slave<<9) | write<<8 | value; // write data (blocks until accepted)

		while ((*zpu_i2c_0)&0x100); // wait until busy not asserted
	}
}

