#ifndef TIG_FILE_MEMORY_H_
#define TIG_FILE_MEMORY_H_

// CE (`save scratch in memory`, card HW1): the save's scratch folder,
// `Save\Current`, held in memory. Private to `file.c`, which routes every
// path under it here (see `tig_file_set_scratch_in_memory`).

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "tig/file.h"

typedef struct TigMemFile TigMemFile;

void tig_memfs_set_enabled(bool enabled);
bool tig_memfs_enabled(void);
void tig_memfs_clear(void);

// Whether `path`, a relative path as the `_native` functions get it, lies in
// the scratch folder: its first component is the save folder (`save`, or the
// `ce save folder` name) and its second is `Current`.
bool tig_memfs_claims(const char* path);

TigMemFile* tig_memfs_open(const char* path, const char* mode);
bool tig_memfs_exists(const char* path, TigFileInfo* info);
void tig_memfs_list(const char* pattern, void (*add)(TigFileList*, TigFileInfo*), TigFileList* list);
int tig_memfs_remove(const char* path);
int tig_memfs_rename(const char* old_path, const char* new_path);
int tig_memfs_mkdir(const char* path);
int tig_memfs_rmdir(const char* path);

// The stream half, with the C library's answers for each call.
int tig_memfile_close(TigMemFile* file);
size_t tig_memfile_read(void* buffer, size_t size, size_t count, TigMemFile* file);
size_t tig_memfile_write(const void* buffer, size_t size, size_t count, TigMemFile* file);
int tig_memfile_seek(TigMemFile* file, long offset, int origin);
long tig_memfile_tell(TigMemFile* file);
long tig_memfile_length(TigMemFile* file);
int tig_memfile_getc(TigMemFile* file);
int tig_memfile_ungetc(int ch, TigMemFile* file);
char* tig_memfile_gets(char* buffer, int max_count, TigMemFile* file);
int tig_memfile_putc(int ch, TigMemFile* file);
int tig_memfile_puts(const char* str, TigMemFile* file);
int tig_memfile_vprintf(TigMemFile* file, const char* format, va_list args);
int tig_memfile_eof(TigMemFile* file);
int tig_memfile_error(TigMemFile* file);
void tig_memfile_clearerr(TigMemFile* file);
void tig_memfile_rewind(TigMemFile* file);

#endif /* TIG_FILE_MEMORY_H_ */
