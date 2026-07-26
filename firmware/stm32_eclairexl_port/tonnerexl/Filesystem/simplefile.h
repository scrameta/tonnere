#pragma once
/* Verbatim from firmware_eclairexl/simplefile.h — the port implements this API
 * on FileX without changing the interface, so ported callers are unmodified. */

enum SimpleFileStatus {SimpleFile_OK, SimpleFile_FAIL};

struct SimpleFile;

// NB when switching file, the other file may loose its position, depending on implementation!

int file_struct_size();

void file_init(struct SimpleFile * file);

char const * file_path(struct SimpleFile * file);
char const * file_name(struct SimpleFile * file);
enum SimpleFileStatus file_read(struct SimpleFile * file, void * buffer, int bytes, int * bytesread);
enum SimpleFileStatus file_seek(struct SimpleFile * file, int offsetFromStart);
int file_size(struct SimpleFile * file);
int file_readonly(struct SimpleFile * file);

enum SimpleFileStatus file_write(struct SimpleFile * file, void * buffer, int bytes, int * byteswritten);
enum SimpleFileStatus file_write_flush();

/* Port addition (not in the original reference): FileX requires every open to be
 * paired with a close, or the media's open-file list is corrupted. The port must
 * call this when done with a file; file_init()/re-open close a reused slot
 * automatically. Safe on a zeroed or stale handle. */
void file_close(struct SimpleFile * file);
