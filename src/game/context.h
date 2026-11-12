#ifndef ARCANUM_GAME_CONTEXT_H_
#define ARCANUM_GAME_CONTEXT_H_

#include <tig/tig.h>

#include "net_compat.h"

#define GAME 0
#define EDITOR 1

typedef enum ViewType {
    VIEW_TYPE_ISOMETRIC,
    VIEW_TYPE_TOP_DOWN,
} ViewType;

typedef void(IsoInvalidateRectFunc)(TigRect* rect);
typedef bool(IsoRedrawFunc)(void);

typedef struct GameInitInfo {
    bool editor;
    tig_window_handle_t iso_window_handle;
    IsoInvalidateRectFunc* invalidate_rect_func;
    IsoRedrawFunc* draw_func;
} GameInitInfo;

typedef struct GameResizeInfo {
    tig_window_handle_t window_handle;
    TigRect window_rect;
    TigRect content_rect;
} GameResizeInfo;

typedef struct GameLoadInfo {
    int version;
    TigFile* stream;
} GameLoadInfo;

typedef struct ViewOptions {
    int type;
    int zoom;
} ViewOptions;

typedef struct LocRect LocRect;

typedef struct SectorListNode {
    /* 0000 */ int64_t sec;
    /* 0008 */ int64_t loc;
    /* 0010 */ int width;
    /* 0014 */ int height;
    /* 0018 */ struct SectorListNode* next;
} SectorListNode;

// CE (card HW1): the most sectors a draw rectangle spans each way. The
// original's 3 (`SectorRectRow`, 0x4D0090) holds a view up to about 4K; a
// view 5K wide crosses 4 sectors and wrote past these arrays. 5 holds 8K.
#define SECTOR_RECT_MAX 5

// The offsets are the original's, for its 3 columns and rows.
typedef struct SectorRectRow {
    /* 0000 */ int num_cols;
    /* 0008 */ int64_t origin_locs[SECTOR_RECT_MAX];
    /* 0020 */ int64_t sector_ids[SECTOR_RECT_MAX];
    /* 0038 */ int tile_ids[SECTOR_RECT_MAX];
    /* 0044 */ int num_hor_tiles[SECTOR_RECT_MAX];
    /* 0050 */ int num_vert_tiles;
} SectorRectRow;

typedef struct SectorRect {
    /* 0000 */ int num_rows;
    /* 0008 */ SectorRectRow rows[SECTOR_RECT_MAX];
} SectorRect;

typedef struct GameDrawInfo {
    TigRect* screen_rect;
    LocRect* loc_rect;
    SectorRect* sector_rect;
    SectorListNode* sectors;
    TigRectListNode** rects;
} GameDrawInfo;

typedef struct MapNewInfo {
    /* 0000 */ const char* base_path;
    /* 0004 */ const char* save_path;
    /* 0008 */ int base_terrain_type;
    /* 0010 */ int64_t width;
    /* 0018 */ int64_t height;
} MapNewInfo;

#endif /* ARCANUM_GAME_CONTEXT_H_ */
