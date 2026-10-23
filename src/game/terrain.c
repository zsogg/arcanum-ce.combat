#include "game/terrain.h"

#include <stdio.h>

#include "game/mes.h"
#include "game/random.h"
#include "game/tile.h"

#define FIRST_TERRAIN_COLOR_ID 100
#define FIRST_TERRAIN_TRANSITION_ID 200

#define TERRAIN_TYPE_MAX 32

typedef struct TerrainHeader {
    /* 0000 */ float version;
    /* 0004 */ unsigned int flags;
    /* 0008 */ int64_t width;
    /* 0010 */ int64_t height;
    /* 0018 */ int base_terrain_type;
    /* 001C */ int padding_1C;
} TerrainHeader;

// Serializeable.
static_assert(sizeof(TerrainHeader) == 0x20, "wrong size");

static void terrain_editor_render(int a1, int a2, TigVideoBuffer* vb, TigRect* rect);
static void terrain_tid_set(int64_t sec, uint16_t tid, void (*callback)(uint64_t sec));
static const char* terrain_base_name(int terrain_type);
static int terrain_match_base_name(const char* base_name);
static void terrain_color(int terrain_type, int* red, int* green, int* blue);
static void terrain_tid_pack(int a1, int a2, int a3, int a4, uint16_t* value);
static uint16_t terrain_tid_random(int base, int from, int transition);
static int64_t terrain_sec_file_num(int a1);
static bool terrain_init_types(void);
static bool terrain_scan_types(void);
static bool terrain_init_transitions(void);
static bool terrain_parse_transition(char* str, int* from_terrain_type, int* to_terrain_type);
static bool terrain_transition_find_route(int from, int to, bool* transitions, int* a4);
static bool terrain_transition_route_dfs(int* a1, int a2, int a3, bool* transitions);
static uint16_t terrain_transition_tid(int from, int to);
static bool terrain_transition_is_valid(int from, int to);
static int terrain_row_cache_load(int64_t a1);
static void terrain_row_decompress(void* dst, uint16_t* src);
static void terrain_row_inflate(void* dst, void* src, int size);
static uint16_t* terrain_row_find(int64_t a1);
static bool terrain_load(TigFile* stream);
static bool terrain_save(TigFile* stream);

// 0x5B9968
static int64_t terrain_transition_codes[] = {
    -1,
    1,
    (2 << 26) | 1,
    (1 << 26) | 1,
    (2 << 26) | 3,
    0,
    (2 << 26) | 2,
    -1,
    3,
    2,
    -1,
    -1,
    (1 << 26) | 3,
    -1,
    -1,
    -1,
};

// NOTE: Unreferenced, so it keeps its placeholder name. Nothing in CE reads it,
// and 0x5B99E8 has no references in the original either, so no code ever gave
// the values a meaning. The contents (1, 3, 2, 6, 4, 12, 8, 9) are corner-mask
// bit pairs of the same family terrain_transition_codes is indexed by, which
// suggests an abandoned second path through the transition lookup - but that is
// inference, and a name would state it as fact.
//
// 0x5B99E8
static int dword_5B99E8[] = {
    1,
    3,
    2,
    6,
    4,
    12,
    8,
    9,
};

// 0x603748
static int terrain_sector_count[TERRAIN_TYPE_MAX];

// 0x6037C8
static char terrain_save_path[TIG_MAX_PATH];

// 0x6038CC
// A four-slot round-robin cache of decompressed terrain rows, spread over
// three globals: this is the next slot to evict, `terrain_row_cache` holds the
// rows and `terrain_row_cache_key` the y each slot currently holds.
// `terrain_row_cache_load` fills a slot and advances `(slot + 1) % 4`; `terrain_tid_get` scans
// the four keys for a hit and reads `terrain_row_cache[idx][x]`. Decompressing
// a row is what this avoids - see the "Error decompressing terrain data" path.
static int terrain_row_cache_next;

// 0x6038D0
static int16_t* terrain_row_cache[4];

// 0x6038E0
static bool terrain_editor;

// 0x6038E4
static char terrain_base_path[TIG_MAX_PATH];

// 0x6039E8
static int* terrain_transitions;

// 0x6039EC
static uint16_t* terrain_tiles;

// 0x6039F0
static TerrainHeader terrain_header;

// 0x603A10
static bool terrain_modified;

// 0x603A14
static int terrain_type_count;

// 0x603A18
static int terrain_tiles_size;

// 0x603A1C
static mes_file_handle_t terrain_mes_file;

// 0x603A20
// Bit 0 set means the terrain type blocks movement, and it comes from the
// data rather than from code: `terrain_init_types` marks the type when its
// `terrain.mes` name contains the marker "/b". `terrain_is_blocked` tests both
// of a sector's two types against it.
static int terrain_type_blocking[TERRAIN_TYPE_MAX];

// 0x603AA0
static int64_t terrain_row_cache_key[4];

// 0x4E7B00
bool terrain_init(GameInitInfo* init_info)
{
    if (!mes_load("terrain\\terrain.mes", &terrain_mes_file)) {
        return false;
    }

    if (!terrain_init_types()) {
        return false;
    }

    if (!terrain_scan_types()) {
        return false;
    }

    if (!terrain_init_transitions()) {
        return false;
    }

    terrain_editor = init_info->editor;

    return true;
}

// 0x4E7B50
void terrain_reset(void)
{
}

// 0x4E7B60
void terrain_exit(void)
{
    if (terrain_transitions != NULL) {
        FREE(terrain_transitions);
        terrain_transitions = NULL;
    }

    mes_unload(terrain_mes_file);
}

// 0x4E7B90
bool terrain_map_new(MapNewInfo* new_map_info)
{
    int index;

    terrain_map_close();

    terrain_header.version = 1.2f;
    terrain_header.flags = 0;
    terrain_header.width = new_map_info->width;
    terrain_header.height = new_map_info->height;
    terrain_header.base_terrain_type = new_map_info->base_terrain_type;

    terrain_tiles_size = sizeof(uint16_t) * (int)terrain_header.width * (int)terrain_header.height;
    terrain_tiles = (uint16_t*)MALLOC(terrain_tiles_size);

    for (index = 0; index < terrain_header.height * terrain_header.width; index++) {
        terrain_tiles[index] = terrain_tid_random(new_map_info->base_terrain_type, new_map_info->base_terrain_type, 15);
    }

    snprintf(terrain_base_path, sizeof(terrain_base_path), "%s\\terrain.tdf", new_map_info->base_path);

    if (terrain_editor) {
        snprintf(terrain_save_path, sizeof(terrain_save_path), "%s\\terrain.tdf", new_map_info->base_path);
    }

    terrain_modified = true;

    return terrain_flush();
}

// 0x4E7CB0
bool terrain_open(const char* base_path, const char* save_path)
{
    TigFile* stream;
    bool v1 = false;

    terrain_map_close();

    snprintf(terrain_base_path, sizeof(terrain_base_path), "%s\\terrain.tdf", base_path);

    if (terrain_editor) {
        snprintf(terrain_save_path, sizeof(terrain_save_path), "%s\\terrain.tdf", save_path);
    }

    stream = tig_file_fopen(terrain_save_path, "rb");
    if (stream == NULL) {
        stream = tig_file_fopen(terrain_base_path, "rb");
        if (stream == NULL) {
            tig_debug_printf("Terrain file doesn't exist [%s]\n", terrain_base_path);
            return false;
        }
    }

    if (tig_file_fread(&terrain_header, sizeof(terrain_header), 1, stream) != 1) {
        tig_debug_println("Unable to read terrain header");
        tig_file_fclose(stream);
        return false;
    }

    if (terrain_header.version != 1.2f) {
        tig_debug_println("Wrong terrain header version");
        tig_file_fclose(stream);
        return false;
    }

    // FIX: the dimensions size the row cache and, in the editor, the tile
    // array; only the version was checked, so a header claiming a huge or
    // negative map allocated accordingly (review 2026-10-22, finding 4).
    // 32768 a side keeps the tile count inside an `int`.
    if (terrain_header.width <= 0 || terrain_header.height <= 0
        || terrain_header.width > 32768 || terrain_header.height > 32768) {
        tig_debug_println("Implausible terrain dimensions");
        tig_file_fclose(stream);
        return false;
    }

    terrain_tiles_size = tig_file_filelength(stream) - sizeof(terrain_header);

    if ((terrain_header.flags & 0x1) != 0) {
        if (terrain_editor) {
            v1 = true;
            terrain_tiles_size = sizeof(int16_t) * (int)terrain_header.width * (int)terrain_header.height;
        } else {
            int index;

            for (index = 0; index < 4; index++) {
                terrain_row_cache[index] = (int16_t*)MALLOC(sizeof(int16_t) * terrain_header.width);
                terrain_row_cache_key[index] = -1;
            }

            terrain_row_cache_next = 0;
        }
    }

    terrain_tiles = (uint16_t*)MALLOC(terrain_tiles_size);

    if (v1) {
        if (!terrain_load(stream)) {
            tig_debug_println("Unable to decompress terrain data");
            tig_file_fclose(stream);
            return false;
        }

        terrain_header.flags &= ~0x1;
    } else {
        if (tig_file_fread(terrain_tiles, terrain_tiles_size, 1, stream) != 1) {
            tig_debug_println("Unable to read terrain data");
            tig_file_fclose(stream);
            return false;
        }
    }

    tig_file_fclose(stream);

    return true;
}

// 0x4E7E80
void terrain_map_close(void)
{
    int index;

    if ((terrain_header.flags & 0x1) != 0) {
        for (index = 0; index < 4; index++) {
            FREE(terrain_row_cache[index]);
            // FIX (porting bug #189): the freed row is forgotten (0x4E7E9B
            // stores 0 after the free); CE left the pointer dangling.
            terrain_row_cache[index] = NULL;
        }
    }

    if (terrain_tiles != NULL) {
        FREE(terrain_tiles);
        terrain_tiles = NULL;
    }

    memset(&terrain_header, 0, sizeof(terrain_header));
    terrain_base_path[0] = '\0';
    terrain_save_path[0] = '\0';
    terrain_modified = false;
}

// 0x4E7EF0
void terrain_reload(void)
{
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char path1[TIG_MAX_PATH];
    char path2[TIG_MAX_PATH];

    compat_splitpath(terrain_base_path, drive, dir, NULL, NULL);
    snprintf(path1, sizeof(path1), "%s%s", drive, dir);
    compat_splitpath(terrain_save_path, drive, dir, NULL, NULL);
    snprintf(path2, sizeof(path2), "%s%s", drive, dir);
    terrain_map_close();
    terrain_open(path1, path2);
}

// 0x4E7F90
bool terrain_editor_dump_bmp(void)
{
    TigVideoBufferCreateInfo vb_create_info;
    TigVideoBuffer* vb;
    TigRect rect;
    TigVideoBufferSaveToBmpInfo vb_to_bmp_info;
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char fname[COMPAT_MAX_FNAME];

    if (!terrain_editor) {
        return false;
    }

    if ((terrain_header.flags & 0x1) != 0) {
        return false;
    }

    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = (int)terrain_header.width; // NOTE: Verified against 0x4E7FBE-0x4E7FD1 - original loads the low 32 bits of the 64-bit width/height; (int) cast is faithful.
    vb_create_info.height = (int)terrain_header.height;
    vb_create_info.background_color = 0;
    if (tig_video_buffer_create(&vb_create_info, &vb) != TIG_OK) {
        return false;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = (int)terrain_header.width; // NOTE: Verified against 0x4E7FCD/0x4E7FD5 - same int64->int32 truncation as above.
    rect.height = (int)terrain_header.height;
    terrain_editor_render(0, 0, vb, &rect);

    vb_to_bmp_info.flags = 0;
    vb_to_bmp_info.video_buffer = vb;
    compat_splitpath(terrain_base_path, drive, dir, fname, NULL);

    // FIX: Was `compat_makepath(path, drive, dir, fname, "bmp")`.
    //
    // The original drops the trailing separator from `dir` and then passes the
    // result in the *fname* slot with no dir at all (0x4E7FE9-0x4E8003):
    //
    //     FUN_0058A3B0(terrain_base_path, drive, dir, fname, 0)   ; _splitpath
    //     ... inline strlen over `dir` ...
    //     dir[~len - 2] = '\0'                                    ; strlen - 1
    //     FUN_0058B67B(path, drive, 0, dir, 0x5B9AD0)             ; _makepath, "bmp"
    //
    // `terrain_base_path` is always "<base>\\terrain.tdf" (the only two writers
    // are the `snprintf`s above), so `dir` is "<base>\\" and the original writes
    // the dump to **<base>.bmp**, a sibling of the terrain directory. Passing
    // `dir` and `fname` through unchanged wrote <base>\terrain.bmp instead.
    //
    // Unexercised either way: everything here is behind `terrain_editor`, and
    // `main.c` sets `game_init_info.editor = false`. `terrain_editor_render`, which fills
    // the buffer being dumped, is still marked incomplete.
    dir[strlen(dir) - 1] = '\0';
    compat_makepath(vb_to_bmp_info.path, drive, NULL, dir, "bmp");
    vb_to_bmp_info.rect = NULL;
    if (tig_video_buffer_save_to_bmp(&vb_to_bmp_info) != TIG_OK) {
        // NOTE: Verified against 0x4E8096-0x4E809E - original leaks vb on save failure; transcription faithful.
        return false;
    }

    tig_video_buffer_destroy(vb);

    return true;
}

// 0x4E80C0
// NOTE: Incomplete transcription - the original has a real body here (it
// renders the terrain bitmap into the video buffer for the editor dump).
// Never exercised: everything calling this is behind `terrain_editor`, and
// main.c sets editor = false.
void terrain_editor_render(int a1, int a2, TigVideoBuffer* vb, TigRect* rect)
{
    // TODO: Incomplete.
}

// 0x4E84C0
void terrain_sector_path(int64_t sector_id, char* path)
{
    uint16_t tid;
    int base;
    int v1;
    int v2;
    int v3;
    int64_t v4;
    const char* to;
    const char* from;

    tid = terrain_tid_get(sector_id);
    base = terrain_tid_base(tid);

    if (base < 0 || base >= terrain_type_count) {
        tig_debug_printf("Error: Invalid base in terrain_sector_path\n  tid: %d  base: %d\n", tid, base);
    }

    v1 = terrain_tid_from(tid);
    v2 = terrain_tid_transition(tid);
    if (base == v1 || v2 == 0) {
        v3 = terrain_tid_tile(tid);
        v4 = terrain_sec_file_num(v3);

        to = terrain_base_name(base);
        if (to == NULL) {
            tig_debug_printf("Error: terrain_base_name failed in terrain_sector_path\n  tid: %d  base: $d\n", tid);
        }

        // FIX: bounded writes; every caller passes a TIG_MAX_PATH buffer, and
        // the terrain names come from terrain.mes (review 2026-10-22,
        // finding 5).
        snprintf(path, TIG_MAX_PATH, "terrain\\%s", to);
    } else {
        v4 = terrain_transition_codes[v2];
        if (v4 != -1) {
            from = terrain_base_name(base);
            to = terrain_base_name(v1);
        } else {
            from = terrain_base_name(v1);
            to = terrain_base_name(base);
            v4 = terrain_transition_codes[15 - v2];
        }

        snprintf(path, TIG_MAX_PATH, "terrain\\%s to %s", from, to);
    }

    snprintf(path + strlen(path), TIG_MAX_PATH - strlen(path), "\\%lld.sec", (long long)v4);
}

// 0x4E86F0
void terrain_fill(Sector* sector)
{
    tig_art_id_t art_id;
    int index;

    tig_art_tile_id_create(7, 7, 15, 0, 0, 0, 0, 0, &art_id);

    for (index = 0; index < 4096; index++) {
        sector->tiles.art_ids[index] = tile_variant_art_id_get(art_id, 7, 0, 15);
    }
}

// 0x4E8740
bool terrain_flush(void)
{
    TigFile* stream;
    TerrainHeader hdr;

    if (!terrain_editor) {
        return false;
    }

    if (terrain_modified) {
        stream = tig_file_fopen(terrain_save_path, "wb");
        if (stream == NULL) {
            return false;
        }

        hdr = terrain_header;
        hdr.flags = terrain_header.flags | 0x1;
        if (tig_file_fwrite(&hdr, sizeof(hdr), 1, stream) != 1) {
            // NOTE: Verified against 0x4E87AA-0x4E87B7 - original leaks the stream on fwrite failure; transcription faithful.
            return false;
        }

        if (!terrain_save(stream)) {
            // NOTE: Verified against 0x4E87C1-0x4E87C9 - original leaks the stream when terrain_save fails; transcription faithful.
            return false;
        }

        terrain_modified = false;
        tig_file_fclose(stream);
    }

    return true;
}

// 0x4E87F0
uint16_t terrain_tid_get(int64_t sec)
{
    int idx;
    int64_t x;
    int64_t y;

    x = SECTOR_X(sec);
    y = SECTOR_Y(sec);
    if (x < 0
        || x >= terrain_header.width
        || y < 0
        || y >= terrain_header.height) {
        return -1;
    }

    if ((terrain_header.flags & 0x1) == 0) {
        return terrain_tiles[x + y * terrain_header.width];
    }

    for (idx = 0; idx < 4; idx++) {
        if (terrain_row_cache_key[idx] == y) {
            break;
        }
    }

    if (idx == 4) {
        idx = terrain_row_cache_load(y);
    }

    return terrain_row_cache[idx][x];
}

// 0x4E88C0
void terrain_tid_set(int64_t sec, uint16_t tid, void (*callback)(uint64_t sec))
{
    int x;
    int y;

    x = (int)SECTOR_X(sec);
    y = (int)SECTOR_Y(sec);

    terrain_tiles[terrain_header.width * y + x] = tid;

    if (callback != NULL) {
        callback(sec);
    }

    terrain_modified = true;
}

// 0x4E8B20
const char* terrain_base_name(int terrain_type)
{
    MesFileEntry mes_file_entry;

    if (terrain_type >= 0 && terrain_type < terrain_type_count) {
        mes_file_entry.num = terrain_type;

        if (mes_search(terrain_mes_file, &mes_file_entry)) {
            return mes_file_entry.str;
        }
    }

    return NULL;
}

// 0x4E8B60
int terrain_match_base_name(const char* base_name)
{
    MesFileEntry mes_file_entry;

    mes_file_entry.num = 0;
    if (mes_search(terrain_mes_file, &mes_file_entry)) {
        do {
            if (SDL_strcasecmp(base_name, mes_file_entry.str) == 0) {
                return mes_file_entry.num;
            }
        } while (mes_find_next(terrain_mes_file, &mes_file_entry));
    }

    return -1;
}

// 0x4E8BE0
void terrain_color(int terrain_type, int* red, int* green, int* blue)
{
    MesFileEntry mes_file_entry;
    char buffer[MAX_STRING];
    char* tok;

    *red = 255;
    *green = 0;
    *blue = 0;

    if (terrain_type >= 0 && terrain_type < terrain_type_count) {
        mes_file_entry.num = terrain_type + FIRST_TERRAIN_COLOR_ID;
        if (mes_search(terrain_mes_file, &mes_file_entry)) {
            strcpy(buffer, mes_file_entry.str);

            tok = strtok(buffer, ",");
            if (tok != NULL) {
                *red = atoi(tok);

                tok = strtok(NULL, ",");
                if (tok != NULL) {
                    *green = atoi(tok);

                    tok = strtok(NULL, ",");
                    if (tok != NULL) {
                        *blue = atoi(tok);
                    }
                }
            }
        }
    }
}

// 0x4E8CE0
void terrain_tid_pack(int a1, int a2, int a3, int a4, uint16_t* value)
{
    a1 = SDL_clamp(a1, 0, 31);
    a2 = SDL_clamp(a2, 0, 31);
    a3 = SDL_clamp(a3, 0, 15);
    a4 = SDL_clamp(a4, 0, 3);

    // FIX (porting bug #187): the "from" type sits at bit 6 (0x4E8D45
    // `shl 5; or; shl 4; or; shl 2; or` = a1<<11 | a2<<6 | a3<<2 | a4), where
    // terrain_tid_from reads it; CE packed it at bit 4, over the transition.
    *value = (uint16_t)(a4 | (a3 << 2) | (a2 << 6) | (a1 << 11));
}

// 0x4E8D60
uint16_t terrain_tid_random(int base, int from, int transition)
{
    uint16_t value;
    int tile;

    // Transcribed from the original: with no transition the "from" side is
    // the base itself, and with the all-transitions code 15 the base stands
    // in for the from side; a real transition between two different types
    // skips the random variant and packs the endpoints directly.
    tile = 0;
    if (transition == 0) {
        base = from;
    } else if (transition == 15) {
        from = base;
    } else if (base != from) {
        terrain_tid_pack(base, from, transition, 0, &value);
        return value;
    }

    if (terrain_sector_count[base] > 1) {
        tile = random_between(0, terrain_sector_count[base] - 1);
    }

    // FIX (porting bug #188): the random variant is packed with NO
    // transition (0x4E8D91 `xor edi,edi` before the pack at 0x4E8DAC); CE
    // passed the caller's code through, 15 for the editor's new map.
    terrain_tid_pack(base, from, 0, tile, &value);
    return value;
}

// 0x4E8DC0
int terrain_tid_base(uint16_t a1)
{
    return (a1 >> 11) & 0x1F;
}

// 0x4E8DD0
int terrain_tid_from(uint16_t a1)
{
    return (a1 >> 6) & 0x1F;
}

// 0x4E8DE0
int terrain_tid_transition(uint16_t a1)
{
    return (a1 >> 2) & 0xF;
}

// 0x4E8DF0
int terrain_tid_tile(uint16_t a1)
{
    return a1 & 3;
}

// 0x4E8E00
bool terrain_is_blocked(int64_t sec)
{
    uint16_t v1;

    v1 = terrain_tid_get(sec);
    if (v1 == 0xFFFF) {
        return true;
    }

    if ((terrain_type_blocking[terrain_tid_base(v1)] & 1) != 0) {
        return true;
    }

    if ((terrain_type_blocking[terrain_tid_from(v1)] & 1) != 0) {
        return true;
    }

    return false;
}

// 0x4E8E60
int64_t terrain_sec_file_num(int a1)
{
    // The parity formula is the original's: 0x4E8E60 masks the index with
    // 0x80000001, so it keeps bit 0 and shifts the halved index into bit 26.
    // The shipped .sec names agree - base terrains are numbered
    // 0, 1, 67108864, 67108865 for tiles 0..3.
    int64_t v1 = a1;
    return (v1 % 2) | ((v1 / 2) << 26);
}

// 0x4E8EA0
bool terrain_init_types(void)
{
    MesFileEntry mes_file_entry;
    int index;
    char* pch;

    for (index = 0; index < TERRAIN_TYPE_MAX; index++) {
        mes_file_entry.num = index;
        if (!mes_search(terrain_mes_file, &mes_file_entry)) {
            break;
        }

        pch = strstr(mes_file_entry.str, "/b");
        if (pch != NULL) {
            terrain_type_blocking[index] = 1;

            do {
                pch--;
            } while (SDL_isspace(*pch));

            pch[1] = '\0';
        }
    }

    return true;
}

// 0x4E8F40
bool terrain_scan_types(void)
{
    MesFileEntry msg;
    char path[TIG_MAX_PATH];
    int index;
    int k;

    for (index = 0; index < TERRAIN_TYPE_MAX; index++) {
        msg.num = index;
        if (!mes_search(terrain_mes_file, &msg)) {
            break;
        }

        for (k = 0; k <= 3; k++) {
            strcpy(path, "terrain\\");
            strcat(path, msg.str);
            strcat(path, "\\");
            SDL_lltoa(terrain_sec_file_num(k), path + strlen(path), 10);
            strcat(path, ".sec");

            if (!tig_file_exists(path, NULL)) {
                break;
            }

            terrain_sector_count[index]++;
        }

        if (k == 0 && index == 0) {
            return false;
        }
    }

    if (index == 0) {
        return false;
    }

    terrain_type_count = index;

    return true;
}

// 0x4E90B0
bool terrain_init_transitions(void)
{
    MesFileEntry mes_file_entry;
    int transition_count;
    bool* transitions;
    int index;
    int from;
    int to;
    int v1;

    mes_file_entry.num = FIRST_TERRAIN_TRANSITION_ID;
    if (!mes_search(terrain_mes_file, &mes_file_entry)) {
        return false;
    }

    transition_count = terrain_type_count * terrain_type_count;
    transitions = (bool*)MALLOC(sizeof(*transitions) * transition_count);
    for (index = 0; index < transition_count; index++) {
        transitions[index] = false;
    }

    do {
        if (!terrain_parse_transition(mes_file_entry.str, &from, &to)) {
            FREE(transitions);
            return false;
        }

        transitions[to + from * terrain_type_count] = true;
        transitions[from + to * terrain_type_count] = true;
    } while (mes_find_next(terrain_mes_file, &mes_file_entry));

    terrain_transitions = (int*)MALLOC(sizeof(*terrain_transitions) * transition_count);

    for (from = 0; from < terrain_type_count; from++) {
        for (to = 0; to < terrain_type_count; to++) {
            if (!terrain_transition_find_route(from, to, transitions, &v1)) {
                FREE(transitions);
                return false;
            }

            terrain_transitions[to + from * terrain_type_count] = v1;
        }
    }

    FREE(transitions);
    return true;
}

// 0x4E9240
bool terrain_parse_transition(char* str, int* from_terrain_type, int* to_terrain_type)
{
    char* pch;
    int from;
    int to;

    pch = strstr(str, " to ");
    if (pch == NULL) {
        return false;
    }

    *pch = '\0';

    from = terrain_match_base_name(str);
    if (from == -1) {
        // NOTE: Verified against 0x4E925E/0x4E9270 - original returns false from the from==-1 path without restoring *pch; transcription faithful.
        return false;
    }

    *pch = ' ';

    to = terrain_match_base_name(pch + 4);
    if (to == -1) {
        return false;
    }

    *from_terrain_type = from;
    *to_terrain_type = to;

    return true;
}

// 0x4E92B0
bool terrain_transition_find_route(int from, int to, bool* transitions, int* a4)
{
    int* v1;
    bool result;

    if (from == to) {
        *a4 = from;
        return true;
    }

    v1 = (int*)MALLOC(sizeof(*v1) * terrain_type_count);
    v1[0] = from;

    result = terrain_transition_route_dfs(v1, 0, to, transitions);

    *a4 = v1[1];
    FREE(v1);

    return result;
}

// 0x4E9310
bool terrain_transition_route_dfs(int* a1, int a2, int a3, bool* transitions)
{
    int v4;
    int v6;
    int v7;

    v6 = 0;
    for (v4 = 0; v4 < terrain_type_count; v4++) {
        if (transitions[v6 + a1[a2]]) {
            for (v7 = 0; v7 <= a2; v7++) {
                if (v4 == a1[v7]) {
                    break;
                }
            }

            if (v7 > a2) {
                a1[a2 + 1] = v4;
                if (v4 == a3) {
                    return true;
                }

                if (terrain_transition_route_dfs(a1, a2 + 1, a3, transitions)) {
                    return true;
                }
            }
        }
        v6 += terrain_type_count;
    }

    return false;
}

// 0x4E93A0
uint16_t terrain_transition_tid(int from, int to)
{
    uint16_t value;
    int v1;

    v1 = terrain_transitions[to + from * terrain_type_count];
    terrain_tid_pack(v1, v1, 15, 0, &value);

    return value;
}

// 0x4E93E0
bool terrain_transition_is_valid(int from, int to)
{
    int terrain_type;

    terrain_type = terrain_tid_base(terrain_transition_tid(from, to));
    return terrain_type == from || terrain_type == to;
}

// 0x4E9410
int terrain_row_cache_load(int64_t a1)
{
    int slot;

    slot = terrain_row_cache_next;
    terrain_row_decompress(terrain_row_cache[slot], terrain_row_find(a1));
    terrain_row_cache_key[slot] = a1;
    terrain_row_cache_next = (slot + 1) % 4;

    return slot;
}

// 0x4E9470
void terrain_row_decompress(void* dst, uint16_t* src)
{
    terrain_row_inflate(dst, (uint32_t*)src + 1, *(uint32_t*)src);
}

// 0x4E9490
void terrain_row_inflate(void* dst, void* src, int size)
{
    z_stream strm;
    int rc;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = src;
    strm.avail_in = 0;

    // FIX: a row that will not inflate is zero-filled and logged. The
    // original (and CE until 2026-10-22) called `exit()` here, so one corrupt
    // row in a mod's terrain file ended the process mid-game (review
    // 2026-10-22, finding 4).
    rc = inflateInit(&strm);
    if (rc != Z_OK) {
        tig_debug_printf("Error decompressing terrain data\n");
        memset(dst, 0, 2 * (size_t)terrain_header.width);
        return;
    }

    strm.next_out = dst;
    strm.avail_in = size;
    strm.next_in = src;
    strm.avail_out = 2 * (int)terrain_header.width;

    rc = inflate(&strm, Z_FINISH);
    if (rc != Z_OK && rc != Z_STREAM_END) {
        tig_debug_printf("Error decompressing terrain data\n");
        memset(dst, 0, 2 * (size_t)terrain_header.width);
    }

    inflateEnd(&strm);
}

// 0x4E9540
uint16_t* terrain_row_find(int64_t a1)
{
    int64_t v1;
    uint16_t* v2 = terrain_tiles;

    for (v1 = 0; v1 < a1; v1++) {
        v2 = (uint16_t*)((uint8_t*)v2 + *(uint32_t*)v2 + 4);
    }
    return v2;
}

// 0x4E9580
bool terrain_load(TigFile* stream)
{
    int64_t y;
    int size;
    int tmp_size = 0;
    void* tmp_buf = NULL;
    uint16_t* dst = terrain_tiles;

    for (y = 0; y < terrain_header.height; y++) {
        if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
            if (tmp_size != 0) {
                FREE(tmp_buf);
            }
            return false;
        }

        if (size > tmp_size) {
            tmp_buf = REALLOC(tmp_buf, size);
            tmp_size = size;
        }

        if (tig_file_fread(tmp_buf, size, 1, stream) != 1) {
            if (tmp_size != 0) {
                FREE(tmp_buf);
            }
            return false;
        }

        terrain_row_inflate(dst, tmp_buf, size);

        dst += terrain_header.width;
    }

    if (tmp_size != 0) {
        FREE(tmp_buf);
    }

    return true;
}

// 0x4E9680
bool terrain_save(TigFile* stream)
{
    int64_t y;
    int pos;
    int size;
    z_stream strm;
    uint16_t* src = terrain_tiles;
    int stride_size = sizeof(*src) * (int)terrain_header.width;
    uint8_t tmp_buf[4096];
    int rc;

    for (y = 0; y < terrain_header.height; y++) {
        pos = tig_file_ftell(stream);

        if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) {
            return false;
        }

        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK) {
            return false;
        }

        strm.next_in = (Bytef*)src;
        strm.avail_in = stride_size;
        strm.next_out = (Bytef*)tmp_buf;
        strm.avail_out = sizeof(tmp_buf);

        while (strm.avail_in != 0) {
            if (strm.avail_out == 0) {
                if (tig_file_fwrite(tmp_buf, sizeof(tmp_buf), 1, stream) != 1) {
                    deflateEnd(&strm);
                    return false;
                }

                strm.next_out = tmp_buf;
                strm.avail_out = sizeof(tmp_buf);
            }

            if (deflate(&strm, Z_NO_FLUSH) != Z_OK) {
                deflateEnd(&strm);
                return false;
            }

            if (strm.total_in == stride_size) {
                while (strm.avail_out == 0) {
                    if (tig_file_fwrite(tmp_buf, sizeof(tmp_buf), 1, stream) != 1) {
                        deflateEnd(&strm);
                        return false;
                    }

                    strm.next_out = tmp_buf;
                    strm.avail_out = sizeof(tmp_buf);

                    rc = deflate(&strm, Z_FINISH);
                    if (rc == Z_STREAM_END) {
                        break;
                    }

                    if (rc != Z_OK) {
                        deflateEnd(&strm);
                        return false;
                    }
                }

                if (strm.avail_out != sizeof(tmp_buf)) {
                    if (tig_file_fwrite(tmp_buf, sizeof(tmp_buf) - strm.avail_out, 1, stream) != 1) {
                        deflateEnd(&strm);
                        return false;
                    }
                }

                if (deflateEnd(&strm) != Z_OK) {
                    return false;
                }

                size = tig_file_ftell(stream) - pos - sizeof(size);

                if (tig_file_fseek(stream, pos, SEEK_SET) != 0) {
                    return false;
                }

                // NOTE: The original writes the 4-byte size back here
                // (0x4E9857: push 0x4); the port wrote sizeof(stream), which
                // is 8 bytes on LP64 and would corrupt the header it just
                // skipped back to patch. Editor-only path (terrain_flush is
                // behind terrain_editor), fixed but deliberately not numbered.
                if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) {
                    return false;
                }

                if (tig_file_fseek(stream, size, SEEK_CUR) != 0) {
                    return false;
                }

                src += terrain_header.width;
                break;
            }
        }
    }

    return true;
}
