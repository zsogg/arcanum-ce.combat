// FILECACHE
//
// The FILECACHE module provides `TigFileCache` object used to cache files
// loaded from FILE module.
//
// Implements a least-recently-used cache. The maximum number of files and
// maximum the cache can hold is provided during creation.
//
// NOTES
//
// - Unlike many other subsystems which track allocations, file cache objects
// are not tracked at subsystem level. Which in turn means `tig_file_cache_exit`
// does not release existing cache objects. You're responsible for releasing of
// file cache objects with `tig_file_cache_destroy` before calling
// `tig_file_cache_exit`.
//
// - The `TigFileCache` resembles `Cache` object from Fallouts, but it`s API
// and implementation is much simpler.
//
// - There is only one use case in both Arcanum and ToEE - caching sound files
// (see SOUND subsystem). This is different from Fallouts where `Cache` was also
// used for art files. In TIG the ART subsystem has it's own cache, which is
// considered implementation detail and have no public API.

#include "tig/file_cache.h"

#include "tig/file.h"
#include "tig/memory.h"

#define FOURCC_FILC SDL_FOURCC('C', 'L', 'I', 'F')

static void tig_file_cache_entry_remove(TigFileCache* cache, TigFileCacheItem* entry);
static bool tig_file_cache_read_contents_into(const char* path, void** data, int* size);
static bool tig_file_cache_prepare_item(TigFileCache* cache, TigFileCacheItem* item, const char* path);
static TigFileCacheItem* tig_file_cache_entry_acquire(TigFileCache* cache);
static TigFileCacheItem* tig_file_cache_entry_acquire_random(TigFileCache* cache);
static TigFileCacheItem* tig_file_cache_entry_acquire_scan(TigFileCache* cache);
static void tig_file_cache_shrink(TigFileCache* cache, int size);
static TigFileCacheEntry* tig_file_cache_acquire_internal(TigFileCache* cache, TigFileCacheItem* item);
static void tig_file_cache_release_internal(TigFileCache* cache, TigFileCacheItem* entry);

// 0x6364F8
static int tig_file_cache_hit_count;

// 0x6364FC
static int tig_file_cache_miss_count;

// 0x636500
static int tig_file_cache_hit_bytes;

// 0x636504
static int tig_file_cache_miss_bytes;

// 0x636508
static int tig_file_cache_removed_bytes;

// 0x538A80
int tig_file_cache_init(TigInitInfo* init_info)
{
    (void)init_info;

    return TIG_OK;
}

// 0x538A90
void tig_file_cache_exit(void)
{
}

// 0x538AA0
void tig_file_cache_flush(TigFileCache* cache)
{
    int index;

    for (index = 0; index < cache->capacity; index++) {
        if (cache->items[index].refcount == 0) {
            tig_file_cache_entry_remove(cache, &(cache->items[index]));
        }
    }
}

// 0x538AE0
void tig_file_cache_entry_remove(TigFileCache* cache, TigFileCacheItem* item)
{
    if (item->entry.data != NULL) {
        cache->items_count--;
        cache->bytes -= item->entry.size;
        tig_file_cache_removed_bytes += item->entry.size;

        if (item->entry.path) {
            FREE(item->entry.path);
            item->entry.path = NULL;
        }

        FREE(item->entry.data);
        item->entry.data = NULL;

        memset(item, 0, sizeof(*item));
    }
}

// 0x538B50
TigFileCache* tig_file_cache_create(int capacity, int max_size)
{
    TigFileCache* cache;

    cache = (TigFileCache*)MALLOC(sizeof(*cache));
    cache->signature = FOURCC_FILC;
    cache->capacity = capacity;
    cache->max_size = max_size;
    cache->items = (TigFileCacheItem*)CALLOC(capacity, sizeof(*cache->items));
    cache->items_count = 0;
    cache->bytes = 0;
    return cache;
}

// 0x538BA0
void tig_file_cache_destroy(TigFileCache* cache)
{
    tig_file_cache_flush(cache);
    FREE(cache->items);
    FREE(cache);
}

// NOTE: neither `tig_file_filelength`'s result nor `tig_file_fread`'s is
// checked, and both are FAITHFUL. Disassembled 2026-08-25 (roadmap P5.3);
// the original at 0x538BC0 is this and nothing else:
//
//   call 0x52fb40      ; tig_file_filelength
//   mov  [edi], eax    ; *size = it, unchecked
//   push eax
//   call 0x58a716      ; malloc, result unchecked
//   mov  [ecx], eax    ; *data = it
//   ... push stream, 1, *size, *data
//   call 0x530640      ; tig_file_fread, result discarded
//   call 0x530330      ; fclose
//   mov  eax, 1        ; always true past the fopen
//
// The only failure the original reports is `fopen` returning NULL. So a
// zero-length or unreadable entry yields `MALLOC(0)` or `MALLOC(-1)` and a
// `true` return in both engines. Recorded rather than guarded: a caller that
// currently gets garbage would start getting `false`, which is a behaviour
// change, not a repair.
//
// 0x538BC0
bool tig_file_cache_read_contents_into(const char* path, void** data, int* size)
{
    TigFile* stream;

    stream = tig_file_fopen(path, "rb");
    if (stream == NULL) {
        return false;
    }

    *size = tig_file_filelength(stream);
    *data = MALLOC(*size);
    tig_file_fread(*data, *size, 1, stream);
    tig_file_fclose(stream);

    return true;
}

// 0x538C20
bool tig_file_cache_prepare_item(TigFileCache* cache, TigFileCacheItem* item, const char* path)
{
    if (!tig_file_cache_read_contents_into(path, &(item->entry.data), &(item->entry.size))) {
        return false;
    }

    item->entry.path = STRDUP(path);

    cache->items_count++;
    cache->bytes += item->entry.size;
    item->entry.index = (int)(item - cache->items);

    return true;
}

// 0x538C90
TigFileCacheEntry* tig_file_cache_acquire(TigFileCache* cache, const char* path)
{
    // 0x6364E8
    static TigFileCacheEntry null_entry;

    int index;
    TigFileCacheItem* item;
    TigFileCacheItem* new_item = NULL;
    TigFileCacheEntry* entry;

    for (index = 0; index < cache->capacity; index++) {
        item = &(cache->items[index]);
        if (item->entry.data != NULL) {
            if (SDL_strcasecmp(item->entry.path, path) == 0) {
                tig_file_cache_hit_count++;
                tig_file_cache_hit_bytes += item->entry.size;
                return tig_file_cache_acquire_internal(cache, item);
            }
        } else {
            if (new_item == NULL) {
                new_item = item;
            }
        }
    }

    if (new_item == NULL) {
        new_item = tig_file_cache_entry_acquire(cache);
        if (new_item == NULL) {
            return &null_entry;
        }
    }

    if (!tig_file_cache_prepare_item(cache, new_item, path)) {
        return &null_entry;
    }

    tig_file_cache_miss_count++;
    tig_file_cache_miss_bytes += new_item->entry.size;

    entry = tig_file_cache_acquire_internal(cache, new_item);

    if (cache->bytes > cache->max_size) {
        tig_file_cache_shrink(cache, cache->bytes - cache->max_size);
    }

    return entry;
}

// 0x538D80
TigFileCacheItem* tig_file_cache_entry_acquire(TigFileCache* cache)
{
    return tig_file_cache_entry_acquire_random(cache);
}

// 0x538D90
TigFileCacheItem* tig_file_cache_entry_acquire_random(TigFileCache* cache)
{
    int attempt;
    int index;
    TigFileCacheItem* entry;

    for (attempt = 0; attempt < 4 * cache->capacity; attempt++) {
        index = rand() % cache->capacity;
        entry = &(cache->items[index]);
        if (entry->entry.data == NULL) {
            return entry;
        }

        if (entry->refcount == 0) {
            tig_file_cache_entry_remove(cache, entry);
            return entry;
        }
    }

    return tig_file_cache_entry_acquire_scan(cache);
}

// FIX: This scans; the original's does not, and the difference is upstream's
// rather than something introduced here - the body is unchanged from the
// pristine snapshot, only the name was ours.
//
// The original at 0x538E00 loads `esi = cache->items` once and NEVER ADVANCES
// IT. The loop head is at 0x538E14 and the only pointer that moves is `eax`,
// which starts as the *cache* and gains 0x18 per iteration while the bound is
// re-read from `[eax+0x1c]` - past the end of a 0x18-byte TigFileCache. So it
// tests `items[0]` over and over and returns NULL unless item 0 itself is free.
//
// Established by RUNNING it, not by reading it, because a claim this odd
// deserved better than one disassembly - and it is re-runnable:
// `harness/emudiff/probe_file_cache_scan.py` asserts the table below and fails
// loudly if it ever stops reproducing. With a four-item cache built to the
// layout `tig_file_cache_entry_remove` (0x538AE0) attests - item stride 0x18,
// `rep stos` of six dwords:
//
//   refcounts [1,0,1,1] -> NULL   (a real scan gives items[1])
//   refcounts [1,1,1,0] -> NULL   (a real scan gives items[3])
//   refcounts [0,1,1,1] -> items[0]
//   refcounts [1,1,1,1] -> NULL
//
// The control that makes those readable: `tig_file_cache_entry_acquire_random`
// (0x538D90) on the SAME fixture returns items[1] correctly, so the capacity
// and items offsets are right and the fault is this loop's alone.
//
// Reachable in principle, and NOT reached in practice. `_acquire_random` falls
// through to here only after 4 * capacity draws all land on held entries; with
// sound.c's capacity of 20 and exactly one free slot that is (19/20)^80, about
// 1.6% (computed). But that is conditional on the cache being full, and it is
// not: a 300-frame instrumented run of Slot0001 on 2026-08-29 acquired **one**
// cache entry in total, and `_acquire_random`, `_acquire_scan` and
// `tig_file_cache_shrink` were **never called** - all three are present as
// symbols in that build, so absent from the census means not called rather than
// inlined. See `harness/tig_census.py`.
//
// One run of one save is not proof of unreachability, and combat with many
// simultaneous effects would use more of the cache. But nothing observed so far
// gets near twenty held sounds. When it does happen, the original fails the
// load and `tig_file_cache_acquire` hands back the static null entry - which per
// quirk 27 is silence with no error. Here it loads.
//
// KEPT, and the divergence is deliberate. zsogg's call, 2026-08-29, on three
// grounds: the body is unchanged from the pristine snapshot, so this is
// upstream's divergence rather than one introduced here; matching the original
// would mean reintroducing a defect that silently drops sounds, to match a build
// whose author plainly meant this loop to scan; and the path is not reached in a
// measured run, so bug-compatibility would buy no observable fidelity at all.
//
// Recorded rather than hidden - `DIVERGENCES.md`, and quirk 28 in
// `MODDING-NOTES.md` for the modder-facing half. Still worth raising upstream,
// which is a separate matter from what this port does.
//
// 0x538E00
TigFileCacheItem* tig_file_cache_entry_acquire_scan(TigFileCache* cache)
{
    int index;
    TigFileCacheItem* entry;

    for (index = 0; index < cache->capacity; index++) {
        entry = &(cache->items[index]);
        if (entry->entry.data == NULL) {
            return entry;
        }

        if (entry->refcount == 0) {
            tig_file_cache_entry_remove(cache, entry);
            return entry;
        }
    }

    return NULL;
}

// 0x538E40
void tig_file_cache_shrink(TigFileCache* cache, int size)
{
    int attempt;

    if (size > cache->bytes) {
        tig_file_cache_flush(cache);
    } else {
        tig_file_cache_removed_bytes = 0;

        for (attempt = 0; attempt < 4 * cache->capacity; attempt++) {
            tig_file_cache_entry_acquire_random(cache);
            if (tig_file_cache_removed_bytes >= size) {
                break;
            }
        }
    }
}

// 0x538E90
TigFileCacheEntry* tig_file_cache_acquire_internal(TigFileCache* cache, TigFileCacheItem* item)
{
    (void)cache;

    item->refcount++;
    return &(item->entry);
}

// 0x538EA0
void tig_file_cache_release(TigFileCache* cache, TigFileCacheEntry* entry)
{
    tig_file_cache_release_internal(cache, &(cache->items[entry->index]));
}

// 0x538EC0
void tig_file_cache_release_internal(TigFileCache* cache, TigFileCacheItem* item)
{
    (void)cache;

    time(&(item->timestamp));
    item->refcount--;
}
