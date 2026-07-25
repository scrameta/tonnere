#include "debug.h"

#include "stdarg.h"
#include "simplefile.h"
#include "utils.h"

#include "printf/printf.h"

struct SimpleFile * usb_file;
int usb_location;

void usb_log_init(struct SimpleFile * file)
{
	if (SimpleFile_OK == file_open_name("/usb.log", file) && (file_size(file)>=65536 && file_readonly(file)==0))
	{
		char buffer[256];
		memset8(&buffer,0,256);

		file_seek(file,0);

		int i;
		for (i=0;i!=1024;++i)
		{
			int byteswritten = 0;
			file_write(file,(void *)buffer,sizeof(buffer),&byteswritten);
		}

		file_seek(file,0);
		usb_location = 0;
		usb_file = file;
	}
	else
		usb_file = 0;
}

static void putcp(void* p,char c)
	{
	*(*((char**)p))++ = c;
	}

void usb_log(char *fmt, ...)
{
	va_list va;
	va_start(va,fmt);

	if (usb_file)
	{
	        char buffer[256];
		char * ptr = &buffer[0];
		tfp_format(&ptr,putcp,fmt,va);
		putcp(&ptr,0);

		file_seek(usb_file,usb_location);

		int byteswritten = 0;
		file_write(usb_file,(void *)buffer,strlen(buffer),&byteswritten);
		usb_location +=byteswritten;

		file_write(usb_file,(void *)"\n",strlen("\n"),&byteswritten);
		usb_location +=byteswritten;
	}

	va_end(va);
}

