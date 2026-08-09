/*
 * simplefile_filex.c — SimpleFile / SimpleDir implemented on FileX.
 *
 * Design notes:
 *  - Each SimpleFile owns an FX_FILE plus its path, name, size, RO flag, and the
 *    media generation it was opened against. If the card is swapped (generation
 *    bumped), operations on a stale handle fail cleanly rather than touching a
 *    closed media.
 *  - All FileX calls happen under the priority-inheritance filesystem mutex, so
 *    a file op cannot race the SD lifecycle thread's media open/close.
 *  - Directory listing reads the whole directory into caller-provided memory
 *    (dir_init gives us the arena), matching the reference contract.
 *  - FileX uses backslash path separators and 8.3-ish semantics via LFN; the
 *    adapter accepts the reference's '/'-style paths and translates.
 *
 * This file is pure FileX API — identical on the on-target ST 6.1.10 build and
 * the host upstream 6.5.1 build, so the host tests exercise the real port.
 */
#include "simplefile.h"
#include "simpledir.h"
#include "simplefile_filex.h"
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Media binding + generation                                          */
/* ------------------------------------------------------------------ */
static FX_MEDIA *s_media;
static unsigned  s_generation;

/* Lock: real ThreadX mutex on both target and host (linux port). */
#include "tx_api.h"
static TX_MUTEX s_fs_mutex;
static int      s_lock_inited;

void simplefile_init_lock(void) {
    if (!s_lock_inited) { tx_mutex_create(&s_fs_mutex, "simplefile", TX_INHERIT); s_lock_inited = 1; }
}
void simplefile_lock(void)   { if (s_lock_inited) tx_mutex_get(&s_fs_mutex, TX_WAIT_FOREVER); }
void simplefile_unlock(void) { if (s_lock_inited) tx_mutex_put(&s_fs_mutex); }

void simplefile_bind_media(FX_MEDIA *media) {
    simplefile_lock();
    s_media = media;
    s_generation++;
    simplefile_unlock();
}
void simplefile_unbind_media(void) {
    simplefile_lock();
    s_media = NULL;
    s_generation++;
    simplefile_unlock();
}
unsigned simplefile_generation(void) { return s_generation; }

/* ------------------------------------------------------------------ */
/* SimpleFile concrete layout                                          */
/* ------------------------------------------------------------------ */
#define SF_PATH_MAX 256

struct SimpleFile {
    FX_FILE  fx;
    unsigned generation;   /* media generation at open time      */
    int      is_open;
    int      readonly;
    int      size;
    ULONG    pos;
    char     path[SF_PATH_MAX];
    char     name[64];
};

int  file_struct_size(void) { return (int)sizeof(struct SimpleFile); }

/* Close the underlying FX_FILE if open and still valid for the current media.
 * Safe to call on a zeroed or stale handle. Exposed for the port to call, and
 * used internally before re-open/re-init so a reused slot never leaves an
 * FX_FILE linked in the media's open-file list. */
void file_close(struct SimpleFile *file) {
    if (!file || !file->is_open) return;
    simplefile_lock();
    /* Only touch FileX if the media generation still matches; a stale handle's
     * FX_FILE belongs to a closed media and must not be dereferenced. */
    if (s_media && file->generation == s_generation) {
        fx_file_close(&file->fx);
    }
    file->is_open = 0;
    simplefile_unlock();
}

void file_init(struct SimpleFile *file) {
    if (!file) return;
    file_close(file);                 /* close a reused slot before clearing */
    memset(file, 0, sizeof(*file));
}

char const *file_path(struct SimpleFile *file) { return file ? file->path : ""; }
char const *file_name(struct SimpleFile *file) { return file ? file->name : ""; }
int file_size(struct SimpleFile *file) { return file ? file->size : 0; }
int file_readonly(struct SimpleFile *file) { return file ? file->readonly : 1; }

/* '/'-style (reference) -> '\'-style (FileX). Copies into out. */
static void to_filex_path(const char *in, char *out, size_t cap) {
    size_t i = 0;
    for (; in[i] && i + 1 < cap; i++) out[i] = (in[i] == '/') ? '\\' : in[i];
    out[i] = '\0';
    /* Root ("/" -> "\" here, or already empty): leave as EMPTY string. Callers
     * pass NULL to fx_directory_default_set for empty, and FX_NULL is FileX's
     * canonical "root directory" argument. Passing a literal "\" instead asks
     * FileX to navigate into a path, which some FileX versions (notably the ST
     * 6.1.10 fork) enumerate as empty. So collapse a bare "\" to "". */
    if ((out[0] == '\\' && out[1] == '\0') || out[0] == '\0') { out[0] = '\0'; }
}
/* Extract the basename (after last '/'). */
static void basename_of(const char *path, char *out, size_t cap) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    size_t i = 0; for (; base[i] && i + 1 < cap; i++) out[i] = base[i]; out[i] = '\0';
}

/* Join dir + "/" + name into out[cap]. Returns 1 on success, 0 if it would
 * overflow (out is left empty). No silent truncation. */
static int join_path(char *out, size_t cap, const char *dir, const char *name) {
    size_t dl = strlen(dir);
    int need_sep = (dl == 0 || dir[dl-1] != '/');
    size_t total = dl + (need_sep ? 1u : 0u) + strlen(name) + 1u;  /* +1 NUL */
    if (total > cap) { if (cap) out[0] = '\0'; return 0; }
    memcpy(out, dir, dl);
    size_t o = dl;
    if (need_sep) out[o++] = '/';
    size_t nl = strlen(name);
    memcpy(out + o, name, nl);
    out[o + nl] = '\0';
    return 1;
}

static int handle_valid(struct SimpleFile *f) {
    return f && f->is_open && s_media && f->generation == s_generation;
}

/* ------------------------------------------------------------------ */
/* Open / read / seek / write                                          */
/* ------------------------------------------------------------------ */
static enum SimpleFileStatus open_common(const char *unixpath, struct SimpleFile *file) {
    if (!file || !s_media) return SimpleFile_FAIL;
    file_init(file);
    char fxpath[SF_PATH_MAX];
    to_filex_path(unixpath, fxpath, sizeof fxpath);

    simplefile_lock();
    /* Try read/write first; fall back to read-only (media WP or file RO). */
    UINT st = fx_file_open(s_media, &file->fx, fxpath, FX_OPEN_FOR_WRITE);
    int ro = 0;
    if (st != FX_SUCCESS) {
        st = fx_file_open(s_media, &file->fx, fxpath, FX_OPEN_FOR_READ);
        ro = 1;
    }
    if (st != FX_SUCCESS) { simplefile_unlock(); return SimpleFile_FAIL; }

    file->is_open   = 1;
    file->readonly  = ro;
    file->generation= s_generation;
    file->size      = (int)file->fx.fx_file_current_file_size;
    file->pos       = 0;
    /* FX_OPEN_FOR_WRITE positions at EOF; reference callers expect to start at
     * the beginning. Seek to 0 so read/write are deterministic post-open. */
    fx_file_seek(&file->fx, 0);
    strncpy(file->path, unixpath, sizeof(file->path)-1);
    basename_of(unixpath, file->name, sizeof file->name);
    simplefile_unlock();
    return SimpleFile_OK;
}

enum SimpleFileStatus file_open_name(char const *path, struct SimpleFile *file) {
    return open_common(path, file);
}

enum SimpleFileStatus file_read(struct SimpleFile *file, void *buffer, int bytes, int *bytesread) {
    if (bytesread) *bytesread = 0;
    if (!handle_valid(file) || bytes < 0) return SimpleFile_FAIL;
    simplefile_lock();
    ULONG got = 0;
    UINT st = fx_file_read(&file->fx, buffer, (ULONG)bytes, &got);
    if (st == FX_SUCCESS || st == FX_END_OF_FILE) {
        file->pos += got;
        if (bytesread) *bytesread = (int)got;
        simplefile_unlock();
        return SimpleFile_OK;
    }
    simplefile_unlock();
    return SimpleFile_FAIL;
}

enum SimpleFileStatus file_seek(struct SimpleFile *file, int offsetFromStart) {
    if (!handle_valid(file) || offsetFromStart < 0) return SimpleFile_FAIL;
    simplefile_lock();
    UINT st = fx_file_seek(&file->fx, (ULONG)offsetFromStart);
    if (st == FX_SUCCESS) file->pos = (ULONG)offsetFromStart;
    simplefile_unlock();
    return st == FX_SUCCESS ? SimpleFile_OK : SimpleFile_FAIL;
}

enum SimpleFileStatus file_write(struct SimpleFile *file, void *buffer, int bytes, int *byteswritten) {
    if (byteswritten) *byteswritten = 0;
    if (!handle_valid(file) || file->readonly || bytes < 0) return SimpleFile_FAIL;
    simplefile_lock();
    UINT st = fx_file_write(&file->fx, buffer, (ULONG)bytes);
    if (st == FX_SUCCESS) {
        file->pos += (ULONG)bytes;
        if ((int)file->pos > file->size) file->size = (int)file->pos;
        if (byteswritten) *byteswritten = bytes;
    }
    simplefile_unlock();
    return st == FX_SUCCESS ? SimpleFile_OK : SimpleFile_FAIL;
}

enum SimpleFileStatus file_write_flush(void) {
    if (!s_media) return SimpleFile_FAIL;
    simplefile_lock();
    UINT st = fx_media_flush(s_media);
    simplefile_unlock();
    return st == FX_SUCCESS ? SimpleFile_OK : SimpleFile_FAIL;
}

/* ------------------------------------------------------------------ */
/* Directory listing into caller-provided arena                        */
/* ------------------------------------------------------------------ */
struct SimpleDirEntry {
    struct SimpleDirEntry *next;
    int   is_subdir;
    int   filesize;
    char  filename[64];
    char  path[SF_PATH_MAX];
};

static char  *s_dir_arena;
static size_t s_dir_arena_cap;
static size_t s_dir_arena_used;

enum SimpleFileStatus dir_init(void *mem, int space) {
    if (!mem || space <= 0) return SimpleFile_FAIL;
    s_dir_arena = (char *)mem;
    s_dir_arena_cap = (size_t)space;
    s_dir_arena_used = 0;
    return SimpleFile_OK;
}
static void *arena_alloc(size_t n) {
    /* align to pointer size */
    size_t a = (n + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    if (s_dir_arena_used + a > s_dir_arena_cap) return NULL;
    void *p = s_dir_arena + s_dir_arena_used;
    s_dir_arena_used += a;
    return p;
}

struct SimpleDirEntry *dir_entries_filtered(char const *dirPath,
                                            int (*filter)(struct SimpleDirEntry *)) {
    if (!s_media || !s_dir_arena) return NULL;
    char fxpath[SF_PATH_MAX];
    to_filex_path(dirPath, fxpath, sizeof fxpath);

    simplefile_lock();
    s_dir_arena_used = 0;
    struct SimpleDirEntry *head = NULL, *tail = NULL;

    UINT st = fx_directory_default_set(s_media, fxpath[0] ? fxpath : NULL);
    if (st != FX_SUCCESS) { simplefile_unlock(); return NULL; }

    CHAR   name[FX_MAX_LONG_NAME_LEN];
    UINT   attr; ULONG sz;
    UINT   year, month, day, hour, minute, second;
    st = fx_directory_first_full_entry_find(s_media, name, &attr, &sz,
                                            &year,&month,&day,&hour,&minute,&second);
    while (st == FX_SUCCESS) {
        struct SimpleDirEntry *e = (struct SimpleDirEntry *)arena_alloc(sizeof *e);
        if (!e) break;                       /* arena full: stop gracefully */
        memset(e, 0, sizeof *e);
        e->is_subdir = (attr & FX_DIRECTORY) ? 1 : 0;
        e->filesize  = (int)sz;
        /* filename must fit its field; skip pathological over-long names */
        if (strlen(name) >= sizeof e->filename) {
            st = fx_directory_next_full_entry_find(s_media, name, &attr, &sz,
                                                   &year,&month,&day,&hour,&minute,&second);
            continue;
        }
        memcpy(e->filename, name, strlen(name) + 1);
        /* build child path with explicit overflow detection (no silent trunc) */
        if (!join_path(e->path, sizeof e->path, dirPath, name)) {
            /* would overflow: skip this entry rather than emit a corrupt path */
            st = fx_directory_next_full_entry_find(s_media, name, &attr, &sz,
                                                   &year,&month,&day,&hour,&minute,&second);
            continue;
        }

        int keep = 1;
        if (filter) keep = filter(e);
        if (keep) {
            if (!head) head = e; else tail->next = e;
            tail = e;
        } else {
            /* reclaim: simplest is to leave allocated; arena is per-listing */
        }
        st = fx_directory_next_full_entry_find(s_media, name, &attr, &sz,
                                               &year,&month,&day,&hour,&minute,&second);
    }
    simplefile_unlock();
    return head;
}

struct SimpleDirEntry *dir_entries(char const *dirPath) {
    return dir_entries_filtered(dirPath, NULL);
}

char const *dir_filename(struct SimpleDirEntry *e) { return e ? e->filename : ""; }
char const *dir_path(struct SimpleDirEntry *e)     { return e ? e->path : ""; }
int  dir_filesize(struct SimpleDirEntry *e)        { return e ? e->filesize : 0; }
struct SimpleDirEntry *dir_next(struct SimpleDirEntry *e) { return e ? e->next : NULL; }
int  dir_is_subdir(struct SimpleDirEntry *e)       { return e ? e->is_subdir : 0; }

enum SimpleFileStatus file_open_dir(struct SimpleDirEntry *entry, struct SimpleFile *file) {
    if (!entry) return SimpleFile_FAIL;
    return open_common(entry->path, file);
}
enum SimpleFileStatus file_open_name_in_dir(struct SimpleDirEntry *entries,
                                            char const *filename, struct SimpleFile *file) {
    for (struct SimpleDirEntry *e = entries; e; e = e->next) {
        if (strcmp(e->filename, filename) == 0) return open_common(e->path, file);
    }
    return SimpleFile_FAIL;
}
