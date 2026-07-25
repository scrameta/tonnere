#include "simpledir.h"
#include "simplefile.h"
#include "fileselector.h"

//#include "fat/pff_file.h"
// XXX - BEST NOT to include this?
char USER_DIR[]="/";
char ROM_DIR[]="/";

#include "stdio.h"
#include "stdlib.h"

int debug_pos = 0;
int prev_debug_pos = 0;
int debug_adjust = 0;
void wait_us(int us)
{
	usleep(us);
}

void char_out ( void* p, char c)
{
	if (debug_pos!=prev_debug_pos)
	{
		fprintf(stderr,"\n");
	}
	//fprintf(stderr,"\n%dWTFWTF\n", debug_pos);
	if (debug_adjust == 128)
	{
		putc('*',stderr);
	}
	putc(c, stderr);
	++debug_pos;
	prev_debug_pos = debug_pos;
}

struct SimpleFile * temp_file;

/*void loadrom(char const * path, int size, void * ram_address)
{
	filter = 0;
	fprintf(stderr,"loadrom:%s\n",path);
	ram_address += 0x800000;
	if (SimpleFile_OK == file_open_name(path, temp_file))
	{
		int read = 0;
		//file_read(temp_file, ram_address, size, &read);
		printf("file_read:%s %x %x\n",file_name(temp_file), ram_address,size); 
	}
	else
	{
		printf("%s:FAILED\n",path);
	}
}*/

void loadromfile(struct SimpleFile * file, int size, void * ram_address)
{
	ram_address += 0x800000;
	int read = 0;
	//file_read(file, ram_address, size, &read);
	printf("file_read:%s %x %x\n",file_name(temp_file), ram_address,size); 
}

void loadrom(char const * path, int size, void * ram_address)
{
	if (SimpleFile_OK == file_open_name(path, temp_file))
	{
		loadromfile(temp_file, size, ram_address);
	}
	else
	{
		printf("%s:FAILED\n",path);
	}
}

void loadrom_indir(struct SimpleDirEntry * entries, char const * filename, int size, void * ram_address)
{
	if (SimpleFile_OK == file_open_name_in_dir(entries, filename, temp_file))
	{
		loadromfile(temp_file, size, ram_address);
	}
	else
	{
		printf("FAILED\n");
	}
}

int main(void)
{
	init_printf(NULL, char_out);

	char * mem = (char *)malloc(65536);
	if (SimpleFile_OK != dir_init(mem, 65536))
	{
		fprintf(stderr,"Failed to open dir!");
		return -1;
	}
	struct SimpleDirEntry * entry = dir_entries("");
	while (entry)
	{
		fprintf(stderr, "Path:%s", dir_path(entry));
		fprintf(stderr, " Name:%s", dir_filename(entry));
		fprintf(stderr, " Size:%d", dir_filesize(entry));
		fprintf(stderr, " Subdir:%d\n", dir_is_subdir(entry));

		entry = dir_next(entry);
	}

	fprintf(stderr,"\n\n");

	entry = dir_entries("/atari800");
	while (entry)
	{
		fprintf(stderr, "Path:%s", dir_path(entry));
		fprintf(stderr, " Name:%s", dir_filename(entry));
		fprintf(stderr, " Size:%d", dir_filesize(entry));
		fprintf(stderr, " Subdir:%d\n", dir_is_subdir(entry));

		entry = dir_next(entry);
	}

	fprintf(stderr,"\n\n");

	entry = dir_entries("/atari800/user");
	while (entry)
	{
		fprintf(stderr, "Path:%s", dir_path(entry));
		fprintf(stderr, " Name:%s", dir_filename(entry));
		fprintf(stderr, " Size:%d", dir_filesize(entry));
		fprintf(stderr, " Subdir:%d\n", dir_is_subdir(entry));

		entry = dir_next(entry);
	}


/*
	fprintf(stderr,"\n\n");
//enum SimpleFileStatus file_open_name(char const * path, struct SimpleFile * file);
//enum SimpleFileStatus file_open_dir(struct SimpleDirEntry * filename, struct SimpleFile * file);
	entry = dir_entries("/UAE4ALL");
	entry = dir_next(entry);
	fprintf(stderr, " Name:%s", dir_filename(entry));
	struct SimpleFile * file = alloca(file_struct_size());
	file_open_dir(entry,file);

//char const * file_name(struct SimpleFile * file);
//enum SimpleFileStatus file_read(struct SimpleFile * file, void * buffer, int bytes, int * bytesread);
//enum SimpleFileStatus file_write(struct SimpleFile * file, void * buffer, int bytes, int * byteswritten);
//enum SimpleFileStatus file_seek(struct SimpleFile * file, int offsetFromStart);
//int file_size(struct SimpleFile * file);
	fprintf(stderr, " Size:%d\n---\n", file_size(file));

	int read = 0;
	char buffer[2048];
	file_seek(file,10);
	file_read(file,&buffer[0],2048,&read);
	int i;
	for (i=0; i!=read; ++i)
	{
		//fprintf(stderr,"%02x", buffer[i]);
		fprintf(stderr,"%c", buffer[i]);
	}

	fprintf(stderr,"\n\n");

	int written;
	file_seek(file,10);
	file_write(file,"Mark",4,&written); // in order to write 'mark' as position 10...
	//file_write_flush(); // Done automatically on next read, or can be forced with this...

	file_seek(file,512);
//	for (i=0;i!=130;++i)
//	{
//		char towrite[9];
//		sprintf(&towrite[0],"Mark:%03d",i);
//		fprintf(stderr,"Writing \"%s\"\n",towrite);
//		file_write(file,towrite,8,&written);
//	}
//	file_write(file,"Blah",4,&written);

	printf("\n*** WTF:%s - %s - %s\n",file_name(file), file_of("/WTF"),file_of("/BLAH/BOOP"));

	// So... for write can only seek to nearest 512...

	fprintf(stderr,"\n\n");
	file_seek(file,10);
	file_read(file,&buffer[0],2048,&read);
	for (i=0; i!=read; ++i)
	{
		//fprintf(stderr,"%02x", buffer[i]);
		fprintf(stderr,"%c", buffer[i]);
	}
*/

	temp_file = alloca(file_struct_size());
		loadrom("ASTEROID.BIN",0x4000, (void *)0x704000);
		loadrom("asteroid.bin",0x4000, (void *)0x704000);
		loadrom("xlorig.rom",0x4000, (void *)0x704000);
		loadrom("xlhias.rom",0x4000, (void *)0x708000);
		loadrom("ultimon.rom",0x4000, (void *)0x70c000);
		loadrom("osbhias.rom",0x4000, (void *)0x710000);
		loadrom("osborig.rom",0x2800, (void *)0x715800);
		loadrom("osaorig.rom",0x2800, (void *)0x719800);
		loadrom("ataribas.rom",0x2000,(void *)0x700000);

	/*{
		printf("WTF\n");
		struct SimpleDirEntry * entries = dir_entries("/system/rom/atari800");
		entries = dir_next(entries);
		printf("WTF:%s\n",dir_filename(entries));
		
		loadrom_indir(entries,"xlorig.rom",0x4000, (void *)0x704000);
		loadrom_indir(entries,"xlhias.rom",0x4000, (void *)0x708000);
		loadrom_indir(entries,"ultimon.rom",0x4000, (void *)0x70c000);
		loadrom_indir(entries,"osbhias.rom",0x4000, (void *)0x710000);
		loadrom_indir(entries,"osborig.rom",0x2800, (void *)0x715800);
		loadrom_indir(entries,"osaorig.rom",0x2800, (void *)0x719800);
		loadrom_indir(entries,"ataribas.rom",0x2000,(void *)0x700000);
	}*/

	//entry = dir_entries("/atari800/user");
	//entry = dir_next(entry);
	//fprintf(stderr, " Name:%s", dir_filename(entry));
	struct SimpleFile * file = alloca(file_struct_size());
	//file_open_name("/atari800/user/acid800.atr",file);
	//fprintf(stderr, "XXX Name:%s", file_name(file));
	filter = filter_disks;
	file_selector(file);

	return 0;
}


