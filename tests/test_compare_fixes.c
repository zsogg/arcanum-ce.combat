// Porting bugs found by the function-by-function comparisons of 2026-10-20 to
// 2026-10-24, each pinned where it is reachable without a map or a renderer.
//
// - #90 `skill_invocation_difficulty` (0x4C8430) let Heal be used
//   without a healing item. The original adds 1,000,000 on every miss of its
//   item test (0x4C8D20/0x4C8D34/0x4C8D47 -> 0x4C8D72).
// - #128 `anim_goal_turn_on_stop_at_blockers` (0x436CB0) set the stop flag
//   0x400 only when networked; the original sets it when not.
// - #129 `anim_knockback_path_blocked` (0x42FD70) had its bit 0x1 test
//   inverted: set is blocked outright (0x42FD7B), clear runs the checks.
// - #178 `object_lighting_changed` (0x444690) stored the setting only when
//   it was out of range, so object lighting stayed off.
// - #186 `convert_screen_to_world_coords` (0x4B98B0) halved the screen
//   offset by truncating division; the original floors it (0x4B98D7
//   `__allshr`), as `location_at`'s `>> 1` does.
// - #262 `inven_ui_gamble_check` (0x579840) tested the NPC's gambling
//   training at the worn-item gate (needs Expert) and the won't-sell gate
//   (needs Master). The original tests the PC's (0x5799c4-0x5799d3 pushes
//   0x6814f8, the PC handle).
// - #302 `target_context_build_list` (0x4F40B0) gave a wall spell rotation
//   1's step for rotation 2 and stopped one short of its radius. The
//   original steps rotation 2 diagonally (jump table 0x4f4da8 entry 2,
//   0x4f4628) and reaches the radius on each side (0x4f47ff jle), so a
//   Radius 2 wall is 5 tiles long, not 3.
// - #315 `animfx_add` (0x4CCF70) counted copies of the still-invalid art id
//   against the per-art cap, so no slot ever matched and capped eye candy
//   (blood splotches, weapon effects) stacked without bound. The original
//   counts copies of the entry's own art (0x4cd0ee).

#include <string.h>

#include "game/anim_private.h"
#include "game/animfx.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/settings.h"
#include "game/skill.h"
#include "game/target.h"

#include "tig/art.h"

#include "obj_harness.h"
#include "test_harness.h"

// `static` in inven_ui.c, reached through inven_ui_globalized.o (see
// tests/CMakeLists.txt).
extern int64_t inven_ui_drag_source_obj;
extern int64_t inven_ui_pc_obj;
extern int64_t inven_ui_target_obj;
bool inven_ui_gamble_check(int64_t obj, bool a2);

// `static` in anim.c, object.c and location.c, reached the same way.
void anim_goal_turn_on_stop_at_blockers(AnimID anim_id);
bool anim_knockback_path_blocked(AnimRunInfo* run_info, int64_t obj, AnimPath* path, int64_t from, int64_t to);
void object_lighting_changed(void);
void convert_screen_to_world_coords(int64_t sx, int64_t sy, int64_t* wx, int64_t* wy);
extern bool object_lighting;
extern IsoInvalidateRectFunc* object_iso_invalidate_rect;

// The wall's step for each rotation, from the original's jump table at
// 0x4f4da8; rotation 0 is the switch's default.
static const int wall_steps[8][2] = {
    { 1, -1 },
    { 0, -1 },
    { -1, -1 },
    { -1, 0 },
    { -1, 1 },
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
};

static bool target_list_has_loc(const TargetList* list, int64_t loc)
{
    int idx;

    for (idx = 0; idx < list->cnt; idx++) {
        if (list->entries[idx].loc == loc) {
            return true;
        }
    }

    return false;
}

TEST(a_radius_2_wall_is_5_tiles_along_its_rotation)
{
    static TargetList list;
    int64_t origin = location_make(100, 100);
    int rot;

    for (rot = 0; rot < 8; rot++) {
        TargetContext ctx;
        TargetParams params;
        int64_t source = 0;
        int dx;
        int dy;
        int k;

        // A neighbouring tile the caster faces the wall's centre from.
        for (dy = -1; dy <= 1 && source == 0; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int64_t loc = location_make(100 + dx, 100 + dy);

                if ((dx != 0 || dy != 0) && location_rot(loc, origin) == rot) {
                    source = loc;
                    break;
                }
            }
        }
        CHECK(source != 0);

        target_context_init(&ctx, &params, OBJ_HANDLE_NULL);
        params.tgt = TGT_TILE_RADIUS_WALL;
        params.radius = 2;
        ctx.source_loc = source;
        ctx.orig_target_loc = origin;
        ctx.targets = &list;
        target_context_build_list(&ctx);

        CHECK_INT(list.cnt, 5);
        for (k = -2; k <= 2; k++) {
            CHECK(target_list_has_loc(&list,
                location_make(100 + k * wall_steps[rot][0], 100 + k * wall_steps[rot][1])));
        }
    }
}

// Fills the first `copies` foreground slots with `art_id` and clears the rest.
static void set_fore_overlays(int64_t obj, tig_art_id_t art_id, int copies)
{
    int index;

    for (index = 0; index < 7; index++) {
        obj_arrayfield_uint32_set(obj, OBJ_F_OVERLAY_FORE, index, index < copies ? art_id : TIG_ART_ID_INVALID);
    }
}

TEST(capped_eye_candy_stops_stacking_past_its_cap)
{
    AnimFxListEntry entry = { 0 };
    AnimFxList list = { 0 };
    AnimFxNode node;
    int64_t obj;

    CHECK(obj_harness_init());
    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    CHECK_INT(tig_art_eye_candy_id_create(5, 0, 0, 0, 0, 0, 0, &entry.eye_candy_art_id), TIG_OK);
    entry.flags = ANIMFX_LIST_ENTRY_FOREGROUND_OVERLAY;
    entry.sound = -1;
    entry.light_art_id = TIG_ART_ID_INVALID;
    list.num_effects = 1;
    list.entries = &entry;

    // The art is not on disk here, so `animfx_add` places nothing; the
    // copies already on the object are what the cap is counted against.
    // The original refuses once more than the cap are present.
    set_fore_overlays(obj, entry.eye_candy_art_id, 2);
    animfx_node_init(&list, &node, obj, -1, 0);
    node.max_simultaneous_effects = 2;
    CHECK(animfx_add(&node));

    set_fore_overlays(obj, entry.eye_candy_art_id, 3);
    animfx_node_init(&list, &node, obj, -1, 0);
    node.max_simultaneous_effects = 2;
    CHECK(!animfx_add(&node));

    obj_harness_destroy(obj);
}

static void set_gambling(int64_t obj, int points, int training)
{
    obj_arrayfield_int32_set(obj, OBJ_F_CRITTER_BASIC_SKILL_IDX, BASIC_SKILL_GAMBLING, points | (training << 6));
}

TEST(the_gamble_gates_read_the_pcs_training)
{
    int64_t pc;
    int64_t npc;
    int64_t item;

    CHECK(obj_harness_init());
    pc = obj_harness_create_proto(OBJ_TYPE_PC);
    npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    // Gold costs nothing to gamble for, so the cost and purse gates pass and
    // only the training gates decide.
    item = obj_harness_create_proto(OBJ_TYPE_GOLD);
    CHECK(pc != OBJ_HANDLE_NULL && npc != OBJ_HANDLE_NULL && item != OBJ_HANDLE_NULL);
    if (pc == OBJ_HANDLE_NULL || npc == OBJ_HANDLE_NULL || item == OBJ_HANDLE_NULL) {
        return;
    }

    inven_ui_pc_obj = pc;
    inven_ui_target_obj = npc;
    inven_ui_drag_source_obj = npc;

    // Worn: needs Expert.
    obj_field_int32_set(item, OBJ_F_ITEM_INV_LOCATION, FIRST_WEAR_INV_LOC);
    obj_field_int32_set(item, OBJ_F_ITEM_FLAGS, 0);
    set_gambling(pc, 0, TRAINING_EXPERT);
    set_gambling(npc, 1, TRAINING_NONE);
    CHECK(inven_ui_gamble_check(item, true));
    set_gambling(pc, 0, TRAINING_NONE);
    set_gambling(npc, 1, TRAINING_EXPERT);
    CHECK(!inven_ui_gamble_check(item, true));

    // Won't sell: needs Master.
    obj_field_int32_set(item, OBJ_F_ITEM_INV_LOCATION, 0);
    obj_field_int32_set(item, OBJ_F_ITEM_FLAGS, OIF_WONT_SELL);
    set_gambling(pc, 0, TRAINING_MASTER);
    set_gambling(npc, 1, TRAINING_NONE);
    CHECK(inven_ui_gamble_check(item, true));
    set_gambling(pc, 0, TRAINING_NONE);
    set_gambling(npc, 1, TRAINING_MASTER);
    CHECK(!inven_ui_gamble_check(item, true));

    inven_ui_pc_obj = OBJ_HANDLE_NULL;
    inven_ui_target_obj = OBJ_HANDLE_NULL;
    inven_ui_drag_source_obj = OBJ_HANDLE_NULL;
    obj_harness_destroy(item);
    obj_harness_destroy(npc);
    obj_harness_destroy(pc);
}

TEST(heal_without_a_healing_item_is_out_of_reach)
{
    SkillInvocation skill_invocation;
    int64_t healer;
    int64_t kit;
    int with_kit;
    int without_kit;

    CHECK(obj_harness_init());
    healer = obj_harness_create_proto(OBJ_TYPE_NPC);
    kit = obj_harness_create_proto(OBJ_TYPE_GENERIC);
    CHECK(healer != OBJ_HANDLE_NULL && kit != OBJ_HANDLE_NULL);
    if (healer == OBJ_HANDLE_NULL || kit == OBJ_HANDLE_NULL) {
        return;
    }

    obj_field_int32_set(kit, OBJ_F_GENERIC_FLAGS, OGF_IS_HEALING_ITEM);
    obj_field_int32_set(kit, OBJ_F_GENERIC_USAGE_BONUS, 10);

    skill_invocation_init(&skill_invocation);
    skill_invocation.skill = SKILL_HEAL;
    skill_invocation.source.obj = healer;
    skill_invocation.item.obj = kit;
    with_kit = skill_invocation_difficulty(&skill_invocation);

    skill_invocation.item.obj = OBJ_HANDLE_NULL;
    without_kit = skill_invocation_difficulty(&skill_invocation);

    // Everything else is the same, so the two differ by the kit's bonus and
    // the 1,000,000 a missing kit costs.
    CHECK_INT(without_kit - with_kit, 1000000 + 10);

    obj_harness_destroy(kit);
    obj_harness_destroy(healer);
}

TEST(a_single_player_move_stops_at_blockers)
{
    AnimRunInfo saved = anim_run_info[0];
    AnimID id = { 0, 0x7E57, 0 };

    memset(&anim_run_info[0], 0, sizeof(anim_run_info[0]));
    anim_run_info[0].id = id;

    anim_goal_turn_on_stop_at_blockers(id);
    CHECK((anim_run_info[0].flags & 0x400) != 0);

    anim_run_info[0] = saved;
}

TEST(a_knockback_path_is_blocked_only_by_its_bit)
{
    AnimRunInfo run_info;
    AnimPath path;
    int64_t from = location_make(0, 0);

    memset(&run_info, 0, sizeof(run_info));
    memset(&path, 0, sizeof(path));

    // Direction 0 from (0, 0) leaves the map, so the checks find nothing to
    // stop at: no map is needed to see which way the bit is read.
    path.baseRot = 0;
    path.flags = 0;
    CHECK(!anim_knockback_path_blocked(&run_info, OBJ_HANDLE_NULL, &path, from, 0));

    path.flags = 0x01;
    CHECK(anim_knockback_path_blocked(&run_info, OBJ_HANDLE_NULL, &path, from, 0));
}

static void invalidate_nothing(TigRect* rect)
{
    (void)rect;
}

TEST(the_object_lighting_setting_is_stored)
{
    IsoInvalidateRectFunc* saved = object_iso_invalidate_rect;

    settings_init(&settings, "unused");
    settings_register(&settings, OBJECT_LIGHTING_KEY, "1", NULL);
    object_iso_invalidate_rect = invalidate_nothing;

    object_lighting = false;
    object_lighting_changed();
    CHECK(object_lighting);

    settings_set_value(&settings, OBJECT_LIGHTING_KEY, 0);
    object_lighting_changed();
    CHECK(!object_lighting);

    // Clamped: anything above 1 is on.
    settings_set_value(&settings, OBJECT_LIGHTING_KEY, 5);
    object_lighting_changed();
    CHECK(object_lighting);

    object_lighting = false;
    object_iso_invalidate_rect = saved;
    settings_exit(&settings);
}

TEST(an_odd_negative_screen_offset_floors)
{
    int64_t ox;
    int64_t oy;
    int64_t wx;
    int64_t wy;

    // The suite never builds a view, so it is the default isometric one.
    location_origin_get(&ox, &oy);

    // dx = -1 >> 1 = -1 (truncation gave 0), dy = 39: x = (39 + 1) / 40.
    convert_screen_to_world_coords(ox - 1, oy + 39, &wx, &wy);
    CHECK_INT(wx, 1);
    CHECK_INT(wy, 0);
}

void test_compare_fixes_suite(void)
{
    RUN(a_radius_2_wall_is_5_tiles_along_its_rotation);
    RUN(capped_eye_candy_stops_stacking_past_its_cap);
    RUN(the_gamble_gates_read_the_pcs_training);
    RUN(heal_without_a_healing_item_is_out_of_reach);
    RUN(a_single_player_move_stops_at_blockers);
    RUN(a_knockback_path_is_blocked_only_by_its_bit);
    RUN(the_object_lighting_setting_is_stored);
    RUN(an_odd_negative_screen_offset_floors);
}
