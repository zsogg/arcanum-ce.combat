#ifndef ARCANUM_GAME_LOCATION_H_
#define ARCANUM_GAME_LOCATION_H_

#include "game/context.h"

typedef void (*LocationOriginSignificantChangeCallback)(int64_t loc);

typedef struct LocRect {
    int64_t x1;
    int64_t y1;
    int64_t x2;
    int64_t y2;
} LocRect;

bool location_init(GameInitInfo* init_info);
void location_exit(void);
void location_resize(GameResizeInfo* resize_info);
void location_update_view(ViewOptions* view_options);
void location_xy(int64_t loc, int64_t* sx, int64_t* sy);
bool location_at(int64_t sx, int64_t sy, int64_t* loc_ptr);
void location_calc_dist_from_screen_center(int64_t location, int64_t* dx, int64_t* dy);
void location_origin_get(int64_t* sx, int64_t* sy);
void location_origin_scroll(int64_t dx, int64_t dy);
void location_origin_set(int64_t location);
void location_origin_significant_change_callback_set(LocationOriginSignificantChangeCallback func);
int location_rot(int64_t a, int64_t b);
bool location_in_dir(int64_t loc, int dir, int64_t* new_loc_ptr);
bool location_in_range(int64_t loc, int dir, int range, int64_t* new_loc_ptr);
bool location_screen_rect_to_loc_rect(TigRect* rect, LocRect* loc_rect);
void sub_4B93F0(int a1, int a2, int* a3, int* a4);
bool location_normalize(int64_t* loc, int* offset_x, int* offset_y);
int64_t location_dist(int64_t a, int64_t b);
bool location_limits_set(int64_t x, int64_t y);
void location_limits_get(int64_t* x, int64_t* y);
int64_t location_center_get(void);
bool sub_4B99C0(int64_t from, int64_t* to);

#define LOCATION_GET_X(l) ((l) & 0xFFFFFFFF)
#define LOCATION_GET_Y(l) (((l) >> 32) & 0xFFFFFFFF)
#define LOCATION_MAKE(x, y) ((x) | (y) << 32)

static inline int64_t location_get_x(int64_t location)
{
    return location & 0xFFFFFFFF;
}

static inline int64_t location_get_y(int64_t location)
{
    return (location >> 32) & 0xFFFFFFFF;
}

static inline int64_t location_make(int64_t x, int64_t y)
{
    // The shift goes through `uint64_t` because y occupies the top 32 bits: a
    // y of 0x80000000 or above lands in bit 63, and shifting into the sign bit
    // of a signed type is undefined. In the original both halves are plain
    // 32-bit unsigned fields, so this is an artifact of CE widening them to
    // `int64_t` rather than anything Troika did. The bit pattern is unchanged -
    // this only makes the shift defined, and UBSan stops aborting on the first
    // location the game builds.
    return x | (int64_t)((uint64_t)y << 32);
}

#endif /* ARCANUM_GAME_LOCATION_H_ */
