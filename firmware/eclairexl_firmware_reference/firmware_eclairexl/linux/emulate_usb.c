#include "memory.h"
#include "integer.h"
#include "log.h"

#include <sys/time.h>

char * usbregname[] =
{
	"HOST_TX_CONTROL_REG", //0
	"HOST_TX_TRANS_TYPE_REG", //1
	"HOST_TX_LINE_CONTROL_REG", //2
	"HOST_TX_TX_SOF_ENABLE_REG", //3
	"HOST_TX_ADDR_REG", //4
	"HOST_TX_ENDP_REG", //5
	"HOST_FRAME_NUM_MSP_REG", //6
	"HOST_FRAME_NUM_LSP_REG", //7
	"HOST_INTERRUPT_STATUS_REG", //8
	"HOST_INTERRUPT_MASK_REG", //9
	"HOST_RX_STATUS_REG", //a
	"HOST_RX_PID_REG", //b
	"HOST_RX_ADDR_REG", //c
	"HOST_RX_ENDP_REG", //d
	"HOST_RX_CONNEXT_STATE_REG", //e
	"HOST_SOF_TIMER_MSB_REG", //f
	"","","","","","","","",
	"","","","","","","","",
	"HOST_RX_FIFO_DATA", //0x20
	"", //0x21
	"HOST_RX_FIFO_DATA_COUNT_MSB", //0x22
	"HOST_RX_FIFO_DATA_COUNT_LSB", //0x23
	"HOST_RX_FIFO_CONTROL_REG", //0x24
	"","","",
	"","","","","","","","",
	"HOST_TX_FIFO_DATA", //0x30
	"", //0x31
	"", //0x32
	"", //0x33
	"HOST_TX_FIFO_CONTROL_REG", //0x34
	"","","",
	"","","","","","","",""
	"","","","","","","","", //0x40
	"","","","","","","","",
	"","","","","","","","", //0x50
	"","","","","","","","",
	"","","","","","","","", //0x60
	"","","","","","","","",
	"","","","","","","","", //0x70
	"","","","","","","","",
	"","","","","","","","", //0x80
	"","","","","","","","",
	"","","","","","","","", //0x90
	"","","","","","","","",
	"","","","","","","","", //0xa0
	"","","","","","","","",
	"","","","","","","","", //0xb0
	"","","","","","","","",
	"","","","","","","","", //0xc0
	"","","","","","","","",
	"","","","","","","","", //0xd0
	"","","","","","","","",
	"HOST_SLAVE_CONTROL_REG", // 0xe0
	"HOST_SLAVE_VERSION_REG",
	"","","","","","",
	"","","","","","","","",
	"","","","","","","","", //0xf0
	"","","","","","","",""
};

#define HOST_TX_CONTROL_REG 0x0
#define HOST_TX_TRANS_TYPE_REG 0x1
#define HOST_TX_LINE_CONTROL_REG 0x2
#define HOST_TX_SOF_ENABLE_REG 0x3
#define HOST_INTERRUPT_STATUS_REG 0x8
#define HOST_RX_CONNECT_STATE_REG 0xe
#define HOST_RX_STATUS_REG 0xa
#define HOST_RX_FIFO_DATA 0x20
#define HOST_RX_FIFO_CONTROL_REG 0x24
#define HOST_RX_FIFO_COUNT_LSB 0x23
#define HOST_TX_FIFO_DATA 0x30
#define HOST_TX_FIFO_CONTROL_REG 0x34

#	define	OHS900_SETUP	0x00 
#	define	OHS900_IN	0x01 
#	define	OHS900_OUT_DATA0	0x02 
#	define	OHS900_OUT_DATA1	0x03 


#	define OHS900_INTMASK_TRANS_DONE	0x01   
#	define OHS900_INTMASK_SOFINTR	0x08   
#	define OHS900_INTMASK_INSRMV	0x04   	
#	define OHS900_INTMASK_RESUME_DET	0x02   

uint8_t txfifo[64];
uint8_t txfifolen = 0;

uint8_t rxfifo[64];
uint8_t rxfifopos = 0;

uint8_t regs[256];

double transmit_complete_time = 0.0;
int in_pending = 0;
int out_pending = 0;
int setup_pending = 0;

int first = true;

char data[] 
=
{
	0,0x80,
	8,0x80,0x12,0x01,0x00,0x01,0x00,0x00,0x00,0x08,
	8,0x80,0x12,0x01,0x00,0x01,0x00,0x00,0x00,0x08,
	8,0,0x79,0x00,0x06,0x00,0x07,0x01,0x01,0x02,
	2,0x80,0x00,0x01,
	8,0x80,0x09,0x02,0x29,0x00,0x01,0x01,0x00,0x80,
	1,0,0xfa,
	8,0x80,0x09,0x02,0x29,0x00,0x01,0x01,0x00,0x80,
	8,0x00,0xfa,0x09,0x04,0x00,0x00,0x02,0x03,0x00,
	8,0x80,0x00,0x00,0x09,0x21,0x10,0x01,0x21,0x01,
	8,0x00,0x22,0x6b,0x00,0x07,0x05,0x81,0x03,0x08,
	8,0x80,0x00,0x0a,0x07,0x05,0x01,0x03,0x08,0x00,
	1,0,0x0a
};
int datapos = 0;

char outdata[] 
=
{
	0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40
};
int outdatapos = 0;

void usbhostslave_init()
{
	first = false;
	int i;
	for (i=0; i!=256;++i) regs[i] = 0 ;
}

double now()
{
	struct timeval tv;
	gettimeofday(&tv,0);
	double res = tv.tv_sec;
	res += (double)tv.tv_usec/1e6;;
	return res;
}

void usbhostslave_update()
{
	if (now()>transmit_complete_time)
	{
		int i;
		if (in_pending)
		{
			in_pending =  0;
			int len = data[datapos++];
			LOG("IN DONE - received %d bytes: ", len);
			u08 stator = data[datapos++];
			for (i=0;i!=len; ++i)
			{
				uint8_t val = data[datapos++];
				LOG("%02x",val);
				rxfifo[regs[HOST_RX_FIFO_COUNT_LSB]++] = val;
			}
			LOG("\n");
			regs[HOST_INTERRUPT_STATUS_REG] |= OHS900_INTMASK_TRANS_DONE;
			regs[HOST_RX_STATUS_REG] = stator;
		}
		if (out_pending)
		{
			out_pending =  0;
			regs[HOST_INTERRUPT_STATUS_REG] |= OHS900_INTMASK_TRANS_DONE;
			regs[HOST_RX_STATUS_REG] = outdata[outdatapos++];
			LOG("OUT DONE - sent ");
			for (i=0;i!=txfifolen; ++i)
			{
				LOG("%02x",txfifo[i]);
			}
			LOG("\n");
			txfifolen = 0;
		}
		if (setup_pending)
		{
			setup_pending = 0;
			regs[HOST_INTERRUPT_STATUS_REG] |= OHS900_INTMASK_TRANS_DONE;
			regs[HOST_RX_STATUS_REG] = 0x40;
			LOG("SETUP DONE - sent ");
			for (i=0;i!=txfifolen; ++i)
			{
				LOG("%02x",txfifo[i]);
			}
			LOG("\n");
			txfifolen = 0;
		}
	}
}

void usbhostslave_write(int address, uint8_t val)
{
	usbhostslave_update();
	int realAddress = address/4;

	if (first) usbhostslave_init();

	switch (realAddress)
	{
	case HOST_TX_CONTROL_REG:
		if (val&0x1) // transmit request!
		{
			switch (regs[HOST_TX_TRANS_TYPE_REG] & OHS900_OUT_DATA1)
			{
			case OHS900_SETUP:
				LOG("SETUP\n");
				setup_pending = true;
				break;
			case OHS900_IN:
				LOG("IN\n");
				in_pending = true;
				break;
			case OHS900_OUT_DATA0:
				LOG("OUT_DATA0\n");
				out_pending = true;
				break;
			case OHS900_OUT_DATA1:
				LOG("OUT_DATA1\n");
				out_pending = true;
				break;
			}

			transmit_complete_time = now(); //+10e-6;;
		}
		else
		{
			LOG("NON transmit write to control??");
		}
		break;
	case HOST_RX_FIFO_CONTROL_REG:
		if (val == 1)
		{
			txfifolen = 0;
		}
		break;
	case HOST_TX_FIFO_DATA:
		if (txfifolen<sizeof(txfifo))
		{
			txfifo[txfifolen++] = val;
		}
		break;
	case HOST_TX_FIFO_CONTROL_REG:
		if (val == 1)
		{
			txfifolen = 0;
		}
		break;
	default:
		//LOG("Write %02x to %s\n", val, usbregname[realAddress]);
		regs[realAddress] = val;
		break;
	}
}

uint8_t usbhostslave_read(int address)
{
	usbhostslave_update();
	int realAddress = address/4;

	if (first) usbhostslave_init();

	switch (realAddress)
	{
	case HOST_RX_CONNECT_STATE_REG:
		{
			static int count = 0;
			++count;
			if (count == 4)
			{
				LOG("Reading from %s - CONNECTED (suppressing...)\n", usbregname[realAddress]);
			}
			if (count > 3)
			{
				return 2;
			}
			//LOG("Reading from %s\n", usbregname[realAddress]);
			return 0;
		}
		break;
	case HOST_RX_FIFO_DATA:
		{
		uint8_t val =  rxfifo[rxfifopos++];
		if (rxfifopos == regs[HOST_RX_FIFO_COUNT_LSB])
		{
			rxfifopos = 0;
			regs[HOST_RX_FIFO_COUNT_LSB] = 0;
		}
		return val;
		}
		break;
	default:
		//LOG("Reading from %s\n", usbregname[realAddress]);
		return regs[realAddress];
	}
}


