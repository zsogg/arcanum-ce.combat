#include "game/wall.h"

#include "game/a_name.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/obj_private.h"
#include "game/object.h"

static void wall_draw_lines(GameDrawInfo* draw_info);
static void wall_draw_grid(GameDrawInfo* draw_info);
static void wall_reconnect_location(int64_t loc, int variation, int p_piece);
static void wall_reconnect_portal(tig_art_id_t art_id, int64_t loc);
static void wall_reconnect_remove_portals(int64_t loc);
static void wall_reconnect_pieces_9_20(int64_t obj);
static void wall_reconnect_pieces_21_33(int64_t obj);
static void wall_reconnect_pieces_34_45(int64_t obj);

// 0x603420
static TigRect wall_iso_content_rect;

// 0x603430
static bool wall_editor;

// 0x603434
static int wall_grid_color;

// 0x603438
static IsoInvalidateRectFunc* wall_iso_window_invalidate_rect;

// 0x60343C
static tig_window_handle_t wall_iso_window_handle;

// 0x603440
static ViewOptions wall_view_options;

// 0x603448
static bool wall_enabled;

// 0x4DF390
bool wall_init(GameInitInfo* init_info)
{
    TigWindowData window_data;

    wall_iso_window_handle = init_info->iso_window_handle;
    wall_iso_window_invalidate_rect = init_info->invalidate_rect_func;

    if (tig_window_data(init_info->iso_window_handle, &window_data) != TIG_OK) {
        return false;
    }

    wall_iso_content_rect.x = 0;
    wall_iso_content_rect.y = 0;
    wall_iso_content_rect.width = window_data.rect.width;
    wall_iso_content_rect.height = window_data.rect.height;

    wall_view_options.type = VIEW_TYPE_ISOMETRIC;
    wall_editor = init_info->editor;
    wall_enabled = true;
    wall_grid_color = tig_color_make(255, 0, 100);

    return true;
}

// 0x4DF460
void wall_exit(void)
{
    wall_iso_window_handle = TIG_WINDOW_HANDLE_INVALID;
    wall_iso_window_invalidate_rect = NULL;
}

// 0x4DF480
void wall_resize(GameResizeInfo* resize_info)
{
    wall_iso_window_handle = resize_info->window_handle;
    wall_iso_content_rect = resize_info->content_rect;
}

// 0x4DF4C0
void wall_update_view(ViewOptions* view_options)
{
    wall_view_options = *view_options;
}

// 0x4DF4E0
void wall_toggle(void)
{
    wall_enabled = !wall_enabled;
    wall_iso_window_invalidate_rect(NULL);
}

// 0x4DF500
void wall_draw(GameDrawInfo* draw_info)
{
    if (wall_view_options.type != VIEW_TYPE_TOP_DOWN) {
        return;
    }

    if (!wall_enabled) {
        return;
    }

    wall_draw_lines(draw_info);
    wall_draw_grid(draw_info);
}

// 0x4E1490
// The wall branch of object_bust: the editor-only body repairs the busted
// wall's art (a_name_wall_aid_damage_set, rotation renumber, neighbour walk)
// in the original at 0x4E1490; untranscribed because it cannot execute when
// editor is off.
void wall_bust(int64_t obj, int64_t triggerer_obj)
{
    (void)obj;
    (void)triggerer_obj;

    if (!wall_editor) {
        return;
    }

    // TODO: Incomplete.
}

// 0x4E18F0
void wall_delete(int64_t obj, bool a2)
{
    tig_art_id_t art_id;
    int p_piece;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    p_piece = tig_art_wall_id_p_piece_get(art_id);

    if (p_piece >= 9 && p_piece <= 20) {
        wall_reconnect_pieces_9_20(obj);
    } else if (p_piece >= 21 && p_piece <= 33) {
        wall_reconnect_pieces_21_33(obj);
    } else if (p_piece >= 34 && p_piece <= 45) {
        wall_reconnect_pieces_34_45(obj);
    }

    if (a2) {
        if (obj_handle_is_valid(obj)) {
            object_remove_from_sector(obj);
            object_delete(obj);
        }
    }
}

// 0x4E1C00
void wall_draw_lines(GameDrawInfo* draw_info)
{
    LocRect* loc_rect;
    int64_t x;
    int64_t y;
    int64_t loc;
    ObjectList walls;
    ObjectNode* node;
    int64_t loc_x;
    int64_t loc_y;
    tig_art_id_t aid;
    int rotation;
    TigRect rect;

    loc_rect = draw_info->loc_rect;
    for (y = loc_rect->y1; y <= loc_rect->y2; y++) {
        for (x = loc_rect->x1; x <= loc_rect->x2; x++) {
            loc = location_make(x, y);
            object_list_location(loc, OBJ_TM_WALL, &walls);
            node = walls.head;
            while (node != NULL) {
                location_xy(loc, &loc_x, &loc_y);
                if (loc_x >= -wall_view_options.zoom
                    && loc_x <= wall_iso_content_rect.width
                    && loc_y >= -wall_view_options.zoom
                    && loc_y <= wall_iso_content_rect.height) {
                    aid = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
                    rotation = tig_art_id_rotation_get(aid);
                    if ((rotation & 1) != 0) {
                        rotation--;
                    }

                    switch (rotation) {
                    case 0:
                        rect.x = (int)loc_x + wall_view_options.zoom;
                        rect.y = (int)loc_y;
                        rect.width = 2;
                        rect.height = wall_view_options.zoom + 1;
                        break;
                    case 2:
                        rect.x = (int)loc_x;
                        rect.y = (int)loc_y + wall_view_options.zoom;
                        rect.width = wall_view_options.zoom + 1;
                        rect.height = 2;
                        break;
                    case 4:
                        rect.x = (int)loc_x;
                        rect.y = (int)loc_y;
                        rect.width = 2;
                        rect.height = wall_view_options.zoom + 1;
                        break;
                    default:
                        rect.x = (int)loc_x;
                        rect.y = (int)loc_y;
                        rect.width = wall_view_options.zoom + 1;
                        rect.height = 2;
                        break;
                    }

                    // FIX (porting bug #320): the wall lines are green
                    // (0x4e1dc9-0x4e1dd3 index the green table at 0xff); CE drew
                    // them red. Editor only.
                    tig_window_fill(wall_iso_window_handle, &rect, tig_color_make(0, 255, 0));
                }
                node = node->next;
            }
            object_list_destroy(&walls);
        }
    }
}

// 0x4E1EB0
void wall_draw_grid(GameDrawInfo* draw_info)
{
    LocRect loc_rect;
    TigRect rect;
    int size;
    int64_t x;
    int64_t y;
    int64_t loc_x;
    int64_t loc_y;
    int center;
    TigRectListNode* rect_node;
    TigRect tmp_rect;

    loc_rect = *draw_info->loc_rect;

    if (loc_rect.x1 % 4 >= 3) {
        loc_rect.x1 += 3;
    } else {
        loc_rect.x1 += 2 - loc_rect.x1 % 4;
    }

    if (loc_rect.y1 % 4 >= 3) {
        loc_rect.y1 += 3;
    } else {
        loc_rect.y1 += 2 - loc_rect.y1 % 4;
    }

    size = wall_view_options.zoom / 4;
    rect.width = size;
    rect.height = size;
    if (size < 4) {
        size = 4;
        rect.width = size;
        rect.height = size;
    }
    if (size > wall_view_options.zoom) {
        size = wall_view_options.zoom;
        rect.width = size;
        rect.height = size;
    }

    center = (wall_view_options.zoom - size) / 2;
    for (y = loc_rect.y1; y <= loc_rect.y2; y += 4) {
        for (x = loc_rect.x1; x <= loc_rect.x2; x += 4) {
            location_xy(LOCATION_MAKE(x, y), &loc_x, &loc_y);
            rect.x = center + (int)loc_x;
            rect.y = center + (int)loc_y;

            rect_node = *draw_info->rects;
            while (rect_node != NULL) {
                if (tig_rect_intersection(&rect, &(rect_node->rect), &tmp_rect) == TIG_OK) {
                    tig_window_fill(wall_iso_window_handle, &tmp_rect, wall_grid_color);
                }
                rect_node = rect_node->next;
            }
        }
    }
}

// Renumbers the walls at one tile of a run and deletes its portals: the
// neighbour step the two functions below repeat.
static void wall_reconnect_location(int64_t loc, int variation, int p_piece)
{
    ObjectList objects;
    ObjectNode* node;
    tig_art_id_t art_id;

    object_list_location(loc, OBJ_TM_WALL | OBJ_TM_PORTAL, &objects);
    node = objects.head;
    while (node != NULL) {
        if (obj_field_int32_get(node->obj, OBJ_F_TYPE) == OBJ_TYPE_PORTAL) {
            object_remove_from_sector(node->obj);
            object_delete(node->obj);
        } else {
            art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
            art_id = tig_art_wall_id_variation_set(art_id, variation);
            art_id = tig_art_wall_id_p_piece_set(art_id, p_piece);
            object_set_current_aid(node->obj, art_id);
        }
        node = node->next;
    }
    object_list_destroy(&objects);
}

// Puts back the door or window a renumbered piece carries.
static void wall_reconnect_portal(tig_art_id_t art_id, int64_t loc)
{
    ObjectID oid;
    tig_art_id_t portal_art_id;
    int64_t portal_obj;

    portal_art_id = a_name_portal_aid_from_wall_aid(art_id, &oid);
    if (portal_art_id != TIG_ART_ID_INVALID) {
        object_create(obj_pool_perm_lookup(oid), loc, &portal_obj);
        object_set_current_aid(portal_obj, portal_art_id);
    }
}

// Deletes the portals on the removed piece's own tile.
static void wall_reconnect_remove_portals(int64_t loc)
{
    ObjectList portals;
    ObjectNode* node;

    object_list_location(loc, OBJ_TM_PORTAL, &portals);
    node = portals.head;
    while (node != NULL) {
        object_remove_from_sector(node->obj);
        object_delete(node->obj);
        node = node->next;
    }
    object_list_destroy(&portals);
}

// FIX (porting bug #323): removing a piece of a 3-, 4- or 5-piece run
// (pieces 9-20) renumbers the rest of the run, deletes the portals on it and
// recreates the window each renumbered piece carries (0x4e20a0: the piece table
// at 0x4e2100, the own-tile portal sweep at 0x4e227e, the clockwise walk at
// 0x4e22ea and the counter-clockwise one at 0x4e243f); CE left a TODO. Editor
// only: `wall_delete` runs from `object_destroy`'s editor branch and
// `portal_delete`.
//
// 0x4E20A0
void wall_reconnect_pieces_9_20(int64_t obj)
{
    tig_art_id_t art_id;
    int p_piece;
    int variation;
    int64_t loc;
    int cw_size;
    int ccw_size;
    int base_p_piece;
    int rot;
    int cw_rot;
    int ccw_rot;
    int64_t adjacent_loc;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    p_piece = tig_art_wall_id_p_piece_get(art_id);
    variation = tig_art_wall_id_variation_get(art_id);
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    base_p_piece = 0;

    switch (p_piece) {
    case 9:
        cw_size = 0;
        ccw_size = 2;
        variation = 0;
        break;
    case 10:
        cw_size = 1;
        ccw_size = 1;
        variation = 0;
        break;
    case 11:
        cw_size = 2;
        ccw_size = 0;
        variation = 0;
        break;
    case 12:
        cw_size = 0;
        ccw_size = 3;
        base_p_piece = 9;
        break;
    case 13:
        cw_size = 1;
        ccw_size = 2;
        variation = 0;
        break;
    case 14:
        cw_size = 2;
        ccw_size = 1;
        variation = 0;
        break;
    case 15:
        cw_size = 3;
        ccw_size = 0;
        base_p_piece = 11;
        break;
    case 16:
        cw_size = 0;
        ccw_size = 4;
        base_p_piece = 12;
        break;
    case 17:
        cw_size = 1;
        ccw_size = 3;
        variation = 0;
        break;
    case 18:
        cw_size = 2;
        ccw_size = 2;
        variation = 0;
        break;
    case 19:
        cw_size = 3;
        ccw_size = 1;
        variation = 0;
        break;
    case 20:
        cw_size = 4;
        ccw_size = 0;
        base_p_piece = 15;
        break;
    default:
        return;
    }

    rot = tig_art_id_rotation_get(art_id);
    if ((rot & 1) == 0) {
        rot++;
    }

    if (rot == 1 || rot == 3) {
        cw_rot = (rot + 6) % 8;
        ccw_rot = (rot + 2) % 8;
    } else {
        cw_rot = (rot + 2) % 8;
        ccw_rot = (rot + 6) % 8;
    }

    art_id = tig_art_wall_id_variation_set(art_id, variation);
    art_id = tig_art_wall_id_p_piece_set(art_id, 0);
    object_set_current_aid(obj, art_id);
    wall_reconnect_remove_portals(loc);

    // The portal art is derived from the removed piece's art, renumbered.
    p_piece = base_p_piece;
    adjacent_loc = loc;
    while (cw_size > 0) {
        location_in_dir(adjacent_loc, cw_rot, &adjacent_loc);
        wall_reconnect_location(adjacent_loc, variation, p_piece);
        wall_reconnect_portal(tig_art_wall_id_p_piece_set(tig_art_wall_id_variation_set(art_id, variation), p_piece), adjacent_loc);

        if (p_piece != 0) {
            p_piece--;
        }

        cw_size--;
    }

    p_piece = base_p_piece;
    adjacent_loc = loc;
    while (ccw_size > 0) {
        location_in_dir(adjacent_loc, ccw_rot, &adjacent_loc);
        wall_reconnect_location(adjacent_loc, variation, p_piece);
        wall_reconnect_portal(tig_art_wall_id_p_piece_set(tig_art_wall_id_variation_set(art_id, variation), p_piece), adjacent_loc);

        if (p_piece != 0) {
            p_piece++;
        }

        ccw_size--;
    }
}

// FIX (porting bug #324): the same for 3-, 4- and 6-piece runs with doors
// (pieces 21-33), where an end piece of the six leaves a four-piece run and a
// single piece one step further (0x4e25b0: the piece table at 0x4e2673, the
// extra steps at 0x4e2940 and 0x4e2b58); CE left a TODO. Editor only.
//
// 0x4E25B0
void wall_reconnect_pieces_21_33(int64_t obj)
{
    tig_art_id_t art_id;
    int p_piece;
    int variation;
    int64_t loc;
    int cw_size;
    int ccw_size;
    int base_p_piece;
    bool cw_extra;
    bool ccw_extra;
    int rot;
    int cw_rot;
    int ccw_rot;
    int64_t adjacent_loc;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    p_piece = tig_art_wall_id_p_piece_get(art_id);
    variation = tig_art_wall_id_variation_get(art_id);
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);

    rot = tig_art_id_rotation_get(art_id);
    if ((rot & 1) == 0) {
        rot++;
    }

    if (rot == 1 || rot == 3) {
        cw_rot = (rot + 6) % 8;
        ccw_rot = (rot + 2) % 8;
    } else {
        cw_rot = (rot + 2) % 8;
        ccw_rot = (rot + 6) % 8;
    }

    base_p_piece = 0;
    ccw_extra = false;
    cw_extra = false;

    // A six-piece run (28-33) losing an end piece leaves a four-piece run and
    // one single piece past it.
    switch (p_piece) {
    case 21:
        cw_size = 0;
        ccw_size = 2;
        variation = 0;
        break;
    case 22:
        cw_size = 1;
        ccw_size = 1;
        variation = 0;
        break;
    case 23:
        cw_size = 2;
        ccw_size = 0;
        variation = 0;
        break;
    case 24:
        cw_size = 0;
        ccw_size = 3;
        base_p_piece = 21;
        break;
    case 25:
        cw_size = 1;
        ccw_size = 2;
        variation = 0;
        break;
    case 26:
        cw_size = 2;
        ccw_size = 1;
        variation = 0;
        break;
    case 27:
        cw_size = 3;
        ccw_size = 0;
        base_p_piece = 23;
        break;
    case 28:
        cw_size = 0;
        ccw_size = 4;
        base_p_piece = 24;
        ccw_extra = true;
        break;
    case 29:
        cw_size = 1;
        ccw_size = 4;
        variation = 0;
        break;
    case 30:
        cw_size = 2;
        ccw_size = 3;
        variation = 0;
        break;
    case 31:
        cw_size = 3;
        ccw_size = 2;
        variation = 0;
        break;
    case 32:
        cw_size = 4;
        ccw_size = 1;
        variation = 0;
        break;
    case 33:
        cw_size = 4;
        ccw_size = 0;
        base_p_piece = 27;
        cw_extra = true;
        break;
    default:
        return;
    }

    // NOTE: Unlike its two siblings, the original clears the removed piece's
    // own variation even where the run keeps it (24, 27, 28, 33): 0x4E276F
    // pushes a register zeroed before the switch.
    art_id = tig_art_wall_id_variation_set(art_id, 0);
    art_id = tig_art_wall_id_p_piece_set(art_id, 0);
    object_set_current_aid(obj, art_id);
    wall_reconnect_remove_portals(loc);

    p_piece = base_p_piece;
    adjacent_loc = loc;
    while (cw_size > 0) {
        location_in_dir(adjacent_loc, cw_rot, &adjacent_loc);
        wall_reconnect_location(adjacent_loc, variation, p_piece);
        wall_reconnect_portal(tig_art_wall_id_p_piece_set(tig_art_wall_id_variation_set(art_id, variation), p_piece), adjacent_loc);

        if (p_piece != 0) {
            p_piece--;
        }

        cw_size--;
    }

    if (cw_extra) {
        location_in_dir(adjacent_loc, cw_rot, &adjacent_loc);
        wall_reconnect_location(adjacent_loc, 0, 0);
    }

    p_piece = base_p_piece;
    adjacent_loc = loc;
    while (ccw_size > 0) {
        location_in_dir(adjacent_loc, ccw_rot, &adjacent_loc);
        wall_reconnect_location(adjacent_loc, variation, p_piece);
        wall_reconnect_portal(tig_art_wall_id_p_piece_set(tig_art_wall_id_variation_set(art_id, variation), p_piece), adjacent_loc);

        if (p_piece != 0) {
            p_piece++;
        }

        ccw_size--;
    }

    if (ccw_extra) {
        location_in_dir(adjacent_loc, ccw_rot, &adjacent_loc);
        wall_reconnect_location(adjacent_loc, 0, 0);
    }
}

// 0x4E2C50
void wall_reconnect_pieces_34_45(int64_t obj)
{
    tig_art_id_t art_id;
    int p_piece;
    int variation;
    int64_t loc;
    int cw_size;
    int ccw_size;
    int base_p_piece;
    int rot;
    int cw_rot;
    int ccw_rot;
    ObjectList walls;
    ObjectNode* node;
    int64_t adjacent_loc;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    p_piece = tig_art_wall_id_p_piece_get(art_id);
    variation = tig_art_wall_id_variation_get(art_id);
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    base_p_piece = 0;

    switch (p_piece) {
    case 34:
        cw_size = 0;
        ccw_size = 2;
        variation = 0;
        break;
    case 35:
        cw_size = 1;
        variation = 0;
        ccw_size = 1;
        break;
    case 36:
        ccw_size = 0;
        cw_size = 2;
        variation = 0;
        break;
    case 37:
        cw_size = 0;
        ccw_size = 3;
        base_p_piece = 34;
        break;
    case 38:
        cw_size = 1;
        ccw_size = 2;
        variation = 0;
        break;
    case 39:
        cw_size = 2;
        ccw_size = 1;
        variation = 0;
        break;
    case 40:
        cw_size = 3;
        ccw_size = 0;
        base_p_piece = 36;
        break;
    case 41:
        cw_size = 0;
        ccw_size = 4;
        base_p_piece = 37;
        break;
    case 42:
        cw_size = 1;
        ccw_size = 3;
        variation = 0;
        break;
    case 43:
        cw_size = 2;
        variation = 0;
        ccw_size = 2;
        break;
    case 44:
        cw_size = 3;
        ccw_size = 1;
        variation = 0;
        break;
    case 45:
        cw_size = 4;
        ccw_size = 0;
        base_p_piece = 40;
        break;
    default:
        return;
    }

    rot = tig_art_id_rotation_get(art_id);
    if ((rot & 1) == 0) {
        rot++;
    }

    // NOTE: Verified against 0x4E2D6B-0x4E2DB1 - rotation arithmetic faithful.
    if (rot == 1 || rot == 3) {
        cw_rot = (rot + 6) % 8;
        ccw_rot = (rot + 2) % 8;
        if (ccw_rot < 0) {
            ccw_rot = 8 - ccw_rot;
        }
    } else {
        cw_rot = (rot + 2) % 8;
        ccw_rot = (rot + 6) % 8;
        if (ccw_rot < 0) {
            ccw_rot = 8 - ccw_rot;
        }
    }

    art_id = tig_art_wall_id_variation_set(art_id, variation);
    art_id = tig_art_wall_id_p_piece_set(art_id, 0);
    object_set_current_aid(obj, art_id);

    p_piece = base_p_piece;
    adjacent_loc = loc;
    while (cw_size > 0) {
        location_in_dir(adjacent_loc, cw_rot, &adjacent_loc);
        object_list_location(adjacent_loc, OBJ_TM_WALL, &walls);
        node = walls.head;
        while (node != NULL) {
            if (obj_field_int32_get(node->obj, OBJ_F_TYPE) == OBJ_TYPE_WALL) {
                art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
                art_id = tig_art_wall_id_variation_set(art_id, variation);
                art_id = tig_art_wall_id_p_piece_set(art_id, p_piece);
                object_set_current_aid(node->obj, art_id);
            }
            node = node->next;
        }
        object_list_destroy(&walls);

        if (p_piece != 0) {
            p_piece--;
        }

        cw_size--;
    }

    p_piece = base_p_piece;
    adjacent_loc = loc;
    while (ccw_size > 0) {
        location_in_dir(adjacent_loc, ccw_rot, &adjacent_loc);
        object_list_location(adjacent_loc, OBJ_TM_WALL, &walls);
        node = walls.head;
        while (node != NULL) {
            if (obj_field_int32_get(node->obj, OBJ_F_TYPE) == OBJ_TYPE_WALL) {
                art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
                art_id = tig_art_wall_id_variation_set(art_id, variation);
                art_id = tig_art_wall_id_p_piece_set(art_id, p_piece);
                object_set_current_aid(node->obj, art_id);
            }
            node = node->next;
        }
        object_list_destroy(&walls);

        if (p_piece != 0) {
            p_piece++;
        }

        ccw_size--;
    }
}
