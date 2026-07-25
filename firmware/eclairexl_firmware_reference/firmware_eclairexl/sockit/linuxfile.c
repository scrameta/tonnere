#include <stdio.h>
#include "linuxfile.h"
#include "simplefile.h"
#include <sys/stat.h>
#include <unistd.h>

char const * file_of(char const * path)
{
        char const * start = path + strlen(path);
        while (start!=path)
        {
                --start;
                if (*start == '/')
                {
                        ++start;
                        break;
                }
        }
        return start;
}

void dir_of(char * dir, char const * path)
{
        char const * end = file_of(path);
        if (end != path)
        {
                int len = end-path;
                while (len--)
                {
                        *dir++ = *path++;
                }
                --dir;
        }

        *dir = '\0';
        return;
}


int file_struct_size()
{
	return sizeof(struct SimpleFile);
}

void file_init(struct SimpleFile * file)
{
	file->file = 0;
	file->name[0] = '\0';
	file->path[0] = '\0';
}

char const * file_path(struct SimpleFile * file)
{
	return &file->path[0];
}

char const * file_name(struct SimpleFile * file)
{
	return &file->name[0];
}

enum SimpleFileStatus file_read(struct SimpleFile * file, void * buffer, int bytes, int * bytesread)
{
	*bytesread = fread(buffer,1,bytes,file->file);
	return SimpleFile_OK;
}

enum SimpleFileStatus file_seek(struct SimpleFile * file, int offsetFromStart)
{
	//printf("file_seek:%d\n",offsetFromStart);
	fseek(file->file,offsetFromStart,SEEK_SET);
	return SimpleFile_OK;
}

int file_size(struct SimpleFile * file)
{
/*	int pos = ftell(file->file);
	fseek(file->file,0L,SEEK_SEND);
	int size = ftell(file->file);
	fseek(file->file,pos,SEEK_SET);*/

	struct stat buf;
	fstat(fileno(file->file),&buf);
	return buf.st_size;
}

int file_readonly(struct SimpleFile * file)
{
	return (0==access(file->path,W_OK)) ? 0 : 1;
}


enum SimpleFileStatus file_write(struct SimpleFile * file, void * buffer, int bytes, int * byteswritten)
{
	//printf("file_write:%d\n",bytes);
	*byteswritten = fwrite(buffer,1,bytes,file->file);
	//printf("written:%d\n",*byteswritten);
	return SimpleFile_OK;
}

enum SimpleFileStatus file_write_flush()
{
	return SimpleFile_OK;
}

