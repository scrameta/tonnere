#include "sockit/memory.h"

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sched.h>

unsigned char volatile * virtual_base;
unsigned char volatile * zpu_base;

unsigned int addr = 0xc0000000;
unsigned int span = 0x1002000;

void init_bridge()
{
	int fd = open("/dev/mem",(O_RDWR|O_SYNC));
	virtual_base = (unsigned char volatile *)mmap(NULL,span,(PROT_READ|PROT_WRITE),MAP_SHARED,fd,addr);
	printf("ADDR:%x %x\n",(int)virtual_base, (int)addr);

	zpu_base = (unsigned char *)virtual_base + 0x1000000; 
	//pokey_regbase = (unsigned char *)virtual_base + 0x1000400;

	//struct sched_param param;
	//param.sched_priority = 50;
	//sched_setscheduler(0,SCHED_RR,&param);
	nice(-20);
}

