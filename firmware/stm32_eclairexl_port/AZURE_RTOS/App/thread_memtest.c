#include <stdint.h>
#include "tx_api.h"
#include "main.h"

#include "thread_memtest.h"
#include "logger.h"

static TX_THREAD thread_memtest;
static ULONG memtest_stack[128];

static void thread_memtest_entry(ULONG arg)
{
#define FPGA_BASE 0x60000000UL
#define FPGA16    ((volatile uint16_t *)FPGA_BASE)
    log_printf("Memtest startup\r\n");

	//  unsigned int len = 128*1024; //128KB*4 = 512KB
	//  unsigned int len = 512*1024; //2048KB
	unsigned int len = 2048*1024; //4096KB
      for (unsigned int i=0;i!=len;++i)
      {
	      unsigned int val = ~i;
	      unsigned short vall = val&0xffff;
	      unsigned short valh = (val&0xffff0000)>>16;

	      FPGA16[i<<1] = vall; 
	      FPGA16[(i<<1) +1] = valh; 
      }

      int fail = 0;
      for (unsigned int i=0;i!=len;++i)
      {
	      unsigned int val = ~i;

	      unsigned int datal = FPGA16[i<<1];
	      unsigned int datah = FPGA16[(i<<1) +1];
	      unsigned int data = (datah<<16) | datal;
	      if (data!=val)
	      {
		      fail++;
		      if (!fail)
		      {
			      log_printf("Fail at %d - data %x vs val %x\r\n", i,data,val);
		      }
	      }
      }
    log_printf("Memtest done\r\n");

      // Can I just exit?
    while (1)
    {
	FPGA16[0x1234] = 0x5678;
	unsigned volatile blah = FPGA16[0x1234];
        tx_thread_sleep(50);
    }
}

void thread_memtest_init(void)
{
    tx_thread_create(&thread_memtest,
                     "MemTest",
                     thread_memtest_entry,
                     0,
                     memtest_stack,
                     sizeof(memtest_stack),
                     5,
                     5,
                     TX_NO_TIME_SLICE,
                     TX_AUTO_START);
}
