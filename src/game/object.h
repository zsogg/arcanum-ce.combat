#ifndef ARCANUM_GAME_OBJECT_H_
#define ARCANUM_GAME_OBJECT_H_

#include "game/context.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/timeevent.h"

#define OBJ_TM_WALL 0x00000001
#define OBJ_TM_PORTAL 0x00000002
#define OBJ_TM_CONTAINER 0x00000004
#define OBJ_TM_SCENERY 0x00000008
#define OBJ_TM_PROJECTILE 0x00000010
#define OBJ_TM_WEAPON 0x00000020
#define OBJ_TM_AMMO 0x00000040
#define OBJ_TM_ARMOR 0x00000080
#define OBJ_TM_GOLD 0x00000100
#define OBJ_TM_FOOD 0x00000200
#define OBJ_TM_SCROLL 0x00000400
#define OBJ_TM_KEY 0x00000800
#define OBJ_TM_KEY_RING 0x00001000
#define OBJ_TM_WRITTEN 0x00002000
#define OBJ_TM_GENERIC 0x00004000
#define OBJ_TM_PC 0x00008000
#define OBJ_TM_NPC 0x00010000
#define OBJ_TM_TRAP 0x00020000

#define OBJ_TM_ITEM (OBJ_TM_WEAPON \
    | OBJ_TM_AMMO                  \
    | OBJ_TM_ARMOR                 \
    | OBJ_TM_GOLD                  \
    | OBJ_TM_FOOD                  \
    | OBJ_TM_SCROLL                \
    | OBJ_TM_KEY                   \
    | OBJ_TM_KEY_RING              \
    | OBJ_TM_WRITTEN               \
    | OBJ_TM_GENERIC)

#define OBJ_TM_CRITTER (OBJ_TM_PC | OBJ_TM_NPC)

#define OBJ_TM_ALL (OBJ_TM_WALL \
    | OBJ_TM_PORTAL             \
    | OBJ_TM_CONTAINER          \
    | OBJ_TM_SCENERY            \
    | OBJ_TM_PROJECTILE         \
    | OBJ_TM_ITEM               \
    | OBJ_TM_PC                 \
    | OBJ_TM_NPC                \
    | OBJ_TM_TRAP)

#define OBJ_TRAVERSAL_PORTAL_BLOCKS 0x01u
#define OBJ_TRAVERSAL_WINDOW_BLOCKS 0x02u
#define OBJ_TRAVERSAL_IGNORE_CRITTERS 0x04u
#define OBJ_TRAVERSAL_PROJECTILE 0x08u
#define OBJ_TRAVERSAL_SKIP_OBJECTS 0x10u
#define OBJ_TRAVERSAL_SOUND 0x20u

// TODO: Better name.
// Durable identity of an object, good across handle invalidation, sector
// unloads and save/load - unlike a bare object handle, which is only valid
// while the object stays in memory.
//
// `location` and `map` are only set for static objects; they let
// `object_locator_resolve` bring the containing sector back in before looking
// the object up. Destroyed objects are recorded as a null id.
typedef struct ObjectLocator {
    /* 0000 */ ObjectID objid;
    /* 0018 */ int64_t location;
    /* 0020 */ int map;
    /* 0024 */ int padding_24;
} ObjectLocator;

// Serializeable.
static_assert(sizeof(ObjectLocator) == 0x28, "wrong size");

// An object handle paired with the locator needed to recover it.
//
// Used wherever a reference has to outlive the handle: spell source/parent/
// target, skill invocations, hotkeys, the follower list. Call
// `object_ref_validate` before trusting `obj`.
typedef struct ObjectRef {
    /* 0000 */ int64_t obj;
    /* 0008 */ ObjectLocator locator;
} ObjectRef;

// Serializeable.
static_assert(sizeof(ObjectRef) == 0x30, "wrong size");

typedef struct ObjectList {
    /* 0000 */ int num_sectors;
    /* 0008 */ int64_t sectors[9];
    /* 0050 */ ObjectNode* head;
} ObjectList;

extern int object_hover_overlay_frame;
extern int object_hover_underlay_frame;
extern bool object_hover_animating;
extern tig_art_id_t object_hover_underlay_art_id;
extern tig_color_t object_hover_color;
extern tig_art_id_t object_hover_overlay_art_id;
extern ObjectLocator object_hover_locator;
extern int64_t object_hover_obj;

bool object_init(GameInitInfo* init_info);
void object_resize(GameResizeInfo* resize_info);
void object_reset(void);
void object_exit(void);
void object_ping(tig_timestamp_t timestamp);
void object_update_view(ViewOptions* view_options);
void object_map_close(void);
bool object_type_is_enabled(int obj_type);
void object_type_toggle(int obj_type);
void object_draw(GameDrawInfo* draw_info);
void object_hover_obj_set(int64_t obj);
int64_t object_hover_obj_get(void);
void sub_43C690(GameDrawInfo* draw_info);
void object_invalidate_rect(TigRect* rect);
void object_flush(void);
bool object_create(int64_t proto_obj, int64_t loc, int64_t* obj_ptr);
bool object_create_ex(int64_t proto_obj, int64_t loc, ObjectID oid, int64_t* obj_ptr);
bool object_duplicate(int64_t proto_obj, int64_t loc, int64_t* obj_ptr);
bool object_duplicate_ex(int64_t proto_obj, int64_t loc, ObjectID* oids, int64_t* obj_ptr);
void object_destroy(int64_t obj);
void object_remove_from_sector(int64_t obj);
void object_delete(int64_t obj);
void object_flags_set(int64_t obj, unsigned int flags);
void object_flags_unset(int64_t obj, unsigned int flags);
int object_hp_pts_get(int64_t obj);
int object_hp_pts_set(int64_t obj, int value);
int object_hp_adj_get(int64_t obj);
int object_hp_adj_set(int64_t obj, int value);
int object_hp_damage_get(int64_t obj);
int object_hp_damage_set(int64_t obj, int value);
int object_hp_max(int64_t obj);
int object_hp_current(int64_t obj);
int object_get_resistance(int64_t obj, int resistance_type, bool a2);
int object_get_ac(int64_t obj, bool a2);
bool sub_43D940(int64_t obj);
bool object_is_static(int64_t obj);
bool sub_43D9F0(int x, int y, int64_t* obj_ptr, unsigned int flags);
void object_get_rect(int64_t obj, unsigned int flags, TigRect* rect);
void object_move_to(int64_t obj, int64_t loc, int offset_x, int offset_y);
bool object_teleported_timeevent_process(TimeEvent* timeevent);
void object_set_offset(int64_t obj, int offset_x, int offset_y);
void object_set_current_aid(int64_t obj, tig_art_id_t aid);
void object_set_light(int64_t obj, unsigned int flags, tig_art_id_t aid, tig_color_t color);
void object_overlay_set(int64_t obj, int fld, int index, tig_art_id_t aid);
bool object_scenery_respawn_timeevent_process(TimeEvent* timeevent);
void object_set_overlay_light(int64_t obj, int index, unsigned int flags, tig_art_id_t aid, tig_color_t color);
void object_set_blit_scale(int64_t obj, int value);
void object_add_flags(int64_t obj, unsigned int flags);
void object_remove_flags(int64_t obj, unsigned int flags);
void sub_43F030(int64_t obj);
void object_cycle_palette(int64_t obj);
void object_bust(int64_t obj, int64_t triggerer_obj);
bool object_is_busted(int64_t obj);
void sub_43F710(int64_t obj);
void object_inc_current_aid(int64_t obj);
void object_dec_current_aid(int64_t obj);
void object_eye_candy_aid_inc(int64_t obj, int fld, int index);
void object_eye_candy_aid_dec(int64_t obj, int fld, int index);
void object_overlay_light_frame_set_first(int64_t obj, int index);
void object_overlay_light_frame_set_last(int64_t obj, int index);
void object_overlay_light_frame_inc(int64_t obj, int index);
void object_overlay_light_frame_dec(int64_t obj, int index);
void object_cycle_rotation(int64_t obj);
bool object_traversal_is_blocked(int64_t obj, int64_t loc, int rot, unsigned int flags, bool* is_window_ptr);
int object_calc_traversal_cost(int64_t obj, int64_t loc, int rot, unsigned int flags, int64_t* block_obj_ptr, int* block_obj_type_ptr, bool* is_window_ptr);
bool object_traversal_check_blocking_door(int64_t obj, int64_t loc, int rot, unsigned int flags, int64_t* block_obj_ptr);
void object_list_location(int64_t loc, unsigned int flags, ObjectList* objects);
void object_list_rect(LocRect* loc_rect, unsigned int flags, ObjectList* objects);
void object_list_vicinity(int64_t obj, unsigned int flags, ObjectList* objects);
void object_list_destroy(ObjectList* objects);
bool object_list_remove(ObjectList* objects, int64_t obj);
void object_list_followers(int64_t obj, ObjectList* objects);
void object_list_all_followers(int64_t obj, ObjectList* objects);
void object_list_party(int64_t obj, ObjectList* objects);
void object_list_team(int64_t obj, ObjectList* objects);
void object_list_copy(ObjectList* dst, ObjectList* src);
void object_drop(int64_t obj, int64_t loc);
void object_pickup(int64_t item_obj, int64_t parent_obj);
bool object_script_execute(int64_t triggerer_obj, int64_t attachee_obj, int64_t extra_obj, int a4, int a5);
int64_t object_dist(int64_t a, int64_t b);
int object_rot(int64_t a, int64_t b);
void object_examine(int64_t obj, int64_t pc_obj, char* buffer);
void object_set_gender_and_race(int64_t obj, int body_type, int gender, int race);
bool object_is_lockable(int64_t obj);
bool object_locked_get(int64_t obj);
bool object_locked_set(int64_t obj, bool locked);
bool object_lock_timeevent_process(TimeEvent* timeevent);
bool object_jammed_set(int64_t obj, bool jammed);
void object_npc_ai_start(int64_t obj, int first_heartbeat);
void object_blit_flags_set(unsigned int flags);
int object_blit_flags_get(void);
void object_write_mem(uint8_t** data_ptr, int* size_ptr, int64_t obj);
bool object_read_mem(uint8_t* data, int64_t* obj_ptr, int64_t loc);
void sub_442520(int64_t obj);
void object_render_clear(int64_t obj);
bool object_hover_get_rect(TigRect* rect);
bool object_save_obj_handle_safe(int64_t* obj_ptr, ObjectLocator* a2, TigFile* stream);
bool object_load_obj_handle_safe(int64_t* obj_ptr, ObjectLocator* a2, TigFile* stream);
void object_locator_init(int64_t obj, ObjectLocator* a2);
bool object_locator_resolve(int64_t* obj_ptr, ObjectLocator* a2);
bool object_locator_validate(int64_t* obj_ptr, ObjectLocator* a2);
void object_ref_init(int64_t obj, ObjectRef* a2);
bool object_ref_resolve(ObjectRef* a1);
bool object_ref_validate(ObjectRef* a1);
int64_t get_fire_at_location(int64_t loc);
void sub_4445A0(int64_t a1, int64_t a2);

void object_highlight_mode_set(bool enabled);

#endif /* ARCANUM_GAME_OBJECT_H_ */
