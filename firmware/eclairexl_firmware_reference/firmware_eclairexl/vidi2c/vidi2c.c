#include "vidi2c.h"
#include "regs.h"
#include "integer.h"

void set_vidi2c()
{
	//wait_us(5000000);
	int i;
	unsigned int slave = 0x70; 
	unsigned int read = 0x1;
	unsigned int write = 0x0;

	// write this to the control reg to change which channel is selected, then stop...
	unsigned int channel0 = 0x4; //VGA
	unsigned int channel1 = 0x5; //HDMI

	unsigned int anotherslave = 0x50; 

//		*zpu_i2c_1 = (slave<<9) | write<<8 | channel0;  // select channel 0
//		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
//
//		*zpu_i2c_1 = (anotherslave<<9) | write<<8 | 0xaa;  
//		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
//
//		*zpu_i2c_1 = (slave<<9) | read<<8;  // read control reg
//		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
//
//		*zpu_i2c_1 = (slave<<9) | write<<8 | channel1;  // select channel 1
//		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
//
//		*zpu_i2c_1 = (anotherslave<<9) | write<<8 | 0xf0;  
//		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
//
//		*zpu_i2c_1 = (slave<<9) | read<<8;  // read control reg
//		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted

	// From here we can either decode or write a function just to say 'VGA connected'/'HDMI connected'
	// If we decode then we can set the timings from here then program clock chip for the pixel clock (TODO)

	// Read DDC from HDMI
	*zpu_i2c_1 = (slave<<9) | write<<8 | channel0;  // select channel 0
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted

	*zpu_i2c_1 = (anotherslave<<9) | write<<8;  // starting from addr 0
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
	for (i=0;i!=128;++i)
	{
		*zpu_i2c_1 = (anotherslave<<9) | read<<8;  
		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
	}

	*zpu_i2c_1 = (slave<<9) | read<<8;  // read control reg
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted

	// Read DDC from VGA
	*zpu_i2c_1 = (slave<<9) | write<<8 | channel1;  // select channel 1
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted

	*zpu_i2c_1 = (anotherslave<<9) | write<<8;   // starting from addr 0
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
	for (i=0;i!=128;++i)
	{
		*zpu_i2c_1 = (anotherslave<<9) | read<<8;  
		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
	}

	*zpu_i2c_1 = (slave<<9) | read<<8;  // read control reg
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
}

void readDDC(int channel, uint8_t * buffer, int maxLen)
{
	int i=0;
	unsigned int slave = 0x70; 
	unsigned int read = 0x1;
	unsigned int write = 0x0;

	unsigned int anotherslave = 0x50; 

	if (1==*zpu_board)
		return;

	*zpu_i2c_1 = (slave<<9) | write<<8 | channel;  // select channel 0
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted

	*zpu_i2c_1 = (anotherslave<<9) | write<<8;  // starting from addr 0
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
	for (i=0;i!=maxLen;++i)
	{
		*zpu_i2c_1 = (anotherslave<<9) | read<<8;  
		while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
		buffer[i] = (*zpu_i2c_1)&0xff;
	}

	*zpu_i2c_1 = (slave<<9) | write<<8;  // deselect channel 0
	while ((*zpu_i2c_1)&0x100); // wait until busy not asserted
}

int isVideoConnected(int channel)
{
	int res = 0;
	uint8_t buffer[5];

	readDDC(channel,&buffer[0],5);

	res = (buffer[0]==0x00 && buffer[1]==0xff && buffer[2]==0xff);
	return res;
}

int isHDMIConnected()
{
	return isVideoConnected(0x5);
}

int isVGAConnected()
{
	return isVideoConnected(0x4);
}

