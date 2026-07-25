#include "linuxdir.h"

#include "linuxfile.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

enum SimpleFileStatus openfile(struct SimpleFile * file)
{
	char buffer[256];
	strncpy(&buffer[0],file->path,sizeof(buffer));
	strncat(&buffer[0],"/",sizeof(buffer));
	strncat(&buffer[0],file->name,sizeof(buffer));

	if (0==access(buffer,R_OK|W_OK))
	{
		file->file = fopen(buffer,"r+");
	}
	else if (0==access(buffer,R_OK))
	{
		file->file = fopen(buffer,"r");
	}

	if (file->file)
	{
		return SimpleFile_OK;
	}
	else
	{
		return SimpleFile_FAIL;
	}
}

enum SimpleFileStatus file_open_name(char const * path, struct SimpleFile * file)
{
	strncpy(file->name,path,sizeof(file->name));
	strncpy(file->path,path,sizeof(file->path));

	return openfile(file);
}

enum SimpleFileStatus file_open_name_in_dir(struct SimpleDirEntry * entries, char const * filename, struct SimpleFile * file)
{
	//entries->filename + "/" + filename;
	strncpy(&file->name[0],filename,sizeof(file->name));
	strncpy(&file->path[0],entries->path,sizeof(file->name));

	return openfile(file);
}

enum SimpleFileStatus file_open_dir(struct SimpleDirEntry * entry, struct SimpleFile * file)
{
	strncpy(&file->path[0],entry->path,sizeof(file->path));
	strncpy(&file->name[0],entry->entry.d_name,sizeof(file->path));

	return openfile(file);
}

// Reads entire dir into memory (i.e. give it a decent chunk of sdram)
char * dir_mem;
int dir_space;
enum SimpleFileStatus dir_init(void * mem, int space)
{
	// Does own memory allow...
	dir_mem = mem;
	dir_space = space;
	return SimpleFile_OK;
}

int dircmp(struct SimpleDirEntry * a, struct SimpleDirEntry * b)
{
	if (dir_is_subdir(a)==dir_is_subdir(b))
		return strcmp(a->entry.d_name,b->entry.d_name);
	else
		return (dir_is_subdir(a)<dir_is_subdir(b));
}

void sort_ll(struct SimpleDirEntry * h)
{
//struct SimpleDirEntry
//{
//	char path[MAX_PATH_LENGTH];
//	char * filename_ptr;
//	int size;
//	int is_subdir;
//	struct SimpleDirEntry * next; // as linked list - want to allow sorting...
//};

	struct SimpleDirEntry * p,*temp,*prev;
	int i,j,n,sorted=0;
	temp=h;
	prev=0;
	for(n=0;temp!=0;temp=temp->next) n++;

	for(i=0;i<n-1 && !sorted;i++){
		p=h;sorted=1;
		prev=0;
		for(j=0;j<n-(i+1);j++){
	//		printf("p->issubdir:%d(%s) p->next->issubdir:%d(%s)",p->is_subdir,p->path,p->next->is_subdir,p->next->path);

			if(dircmp(p,p->next)>0) {
	//			printf("SWITCH!\n");
				struct SimpleDirEntry * a = p;
				struct SimpleDirEntry * b = p->next;
				a->next=b->next;
				b->next=a;
				if (prev)
					prev->next=b;
				p=b;

				sorted=0;
			}
			prev=p;
			p=p->next;
		}
	}

	//temp=h;
	//for(n=0;temp!=0;temp=temp->next) printf("POST:%s\n",temp->path);
}

struct SimpleDirEntry * dir_entries_filtered(char const * dirPath, int (*filter)(struct SimpleDirEntry *))
{
	struct SimpleDirEntry * entry = dir_entries(dirPath);
	if (!entry) return 0;

	struct SimpleDirEntry * temp1 = entry;
	while (temp1)
	{
		printf("Entry_PRE filtered:%s\n",temp1->entry.d_name);
		temp1 = temp1->next;
	}

	struct SimpleDirEntry * head = entry;
	struct SimpleDirEntry * prev = 0;
	while (entry)
	{
		if (!(*filter)(entry))
		{
			if (entry == head)
				head = head->next;
			else if (prev)
				prev->next = entry->next;
		}
		else
		{
			prev = entry;
		}

		entry = entry->next;
	}

	struct SimpleDirEntry * temp = head;
	while (temp)
	{
		printf("Entry_filtered:%s\n",temp->entry.d_name);
		temp = temp->next;
	}

	printf("Sorting\n\n");

	if (filter)
	{
		sort_ll((struct SimpleDirEntry *) head);
	}

	printf("sorted\n\n");
	temp = head;
	while (temp)
	{
		printf("Entry_sorted:%s\n",temp->entry.d_name);
		temp = temp->next;
	}

	return head;
}

struct SimpleDirEntry * dir_entries(char const * dirPath)
{
	DIR * dir = opendir(dirPath);
	if (!dir) return 0;

	char * mem = dir_mem;
	int remaining_space = dir_space;

	struct SimpleDirEntry * head = (struct SimpleDirEntry *)(mem);
	mem += sizeof(struct SimpleDirEntry);
        remaining_space -= sizeof(struct SimpleDirEntry);

	struct SimpleDirEntry * prev = head;
        while (1)
        {
		if (remaining_space < (int)sizeof(struct SimpleDirEntry))
		{
			prev->next = 0;
			return head;
		}

		struct SimpleDirEntry * myentry = (struct SimpleDirEntry *)(mem);
		myentry->next = 0;

		struct dirent * result;
		readdir_r(dir,&myentry->entry,&result);

		if (!result)
		{
			break;
		}

		if (strcmp(".",myentry->entry.d_name) == 0) continue;
		if (strcmp("..",myentry->entry.d_name) == 0) 
		{
			strcpy(head->path,myentry->path);
			head->entry = myentry->entry;
			continue;
		}

		mem += sizeof(struct SimpleDirEntry);
		remaining_space -= sizeof(struct SimpleDirEntry);

		strncpy(myentry->path,dirPath,sizeof(myentry->path));

		if (!head) head = myentry;
		if (prev) prev->next = myentry;
		prev = myentry;

		printf("Entry:%s :%d\n",myentry->entry.d_name,dir_is_subdir(myentry));
	}

	closedir(dir);

	printf("All done\n\n");

	return head;
}

char const * dir_filename(struct SimpleDirEntry * entry)
{
	return &entry->entry.d_name[0];
}

char dir_buffer[256];
char const * dir_path(struct SimpleDirEntry * entry)
{
	strncpy(&dir_buffer[0],entry->path,sizeof(dir_buffer));
	strncat(&dir_buffer[0],"/",sizeof(dir_buffer));
	strncat(&dir_buffer[0],entry->entry.d_name,sizeof(dir_buffer));
	
	return &dir_buffer[0];
}

int dir_filesize(struct SimpleDirEntry * entry)
{
	char buffer[256];
	strncpy(&buffer[0],entry->path,sizeof(buffer));
	strncat(&buffer[0],"/",sizeof(buffer));
	strncat(&buffer[0],entry->entry.d_name,sizeof(buffer));

	struct stat buf;
	stat(buffer, &buf);
	return buf.st_size;
}

struct SimpleDirEntry * dir_next(struct SimpleDirEntry * entry)
{
	return entry->next;
}

int dir_is_subdir(struct SimpleDirEntry * entry)
{
	char buffer[256];
	strncpy(&buffer[0],entry->path,sizeof(buffer));
	strncat(&buffer[0],"/",sizeof(buffer));
	strncat(&buffer[0],entry->entry.d_name,sizeof(buffer));

	struct stat buf;
	stat(buffer, &buf);
	return S_ISDIR(buf.st_mode);
}

