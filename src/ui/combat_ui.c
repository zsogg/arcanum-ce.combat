#include "ui/combat_ui.h"

#include "game/anim.h"
#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/hrp.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/uap.h"
#include "ui/anim_ui.h"
#include "ui/called_shot.h"
#include "ui/intgame.h"
#include "ui/textedit_ui.h"
#include "ui/wmap_ui.h"

typedef enum ApColor {
    AP_COLOR_GREEN,
    AP_COLOR_ORANGE,
    AP_COLOR_RED,
    AP_COLOR_COUNT,
} ApColor;

#define THREE 3
#define TWENTY 20

static void combat_ui_combat_mode_changed(bool combat_ended);
static void combat_ui_recreate(void);
static void combat_ui_create(void);
static void combat_ui_close(void);
static void combat_ui_destroy(void);
static void combat_ui_draw_compact_ap_bar(int available_ap);
static void combat_ui_draw_ap_bar_segment(tig_window_handle_t window_handle, TigRect* rect, tig_color_t color);
static void combat_ui_draw_ap_bar(int action_points);
static void combat_ui_draw_compact_npc_turn_indicator(void);
static void combat_ui_turn_notify(int64_t obj);
static bool combat_ui_message_filter(TigMessage* msg);

// 0x5CAA20
static TigRect combat_ui_compact_ap_bar_frame = { 617, 577, 124, 14 };

// 0x5CAA30
static tig_art_id_t combat_ui_ap_bar_loading_indicator_art_id = TIG_ART_ID_INVALID;

// 0x5CAA38
static UiButtonInfo combat_ui_end_turn_button_info = { 320, 9, 297, TIG_BUTTON_HANDLE_INVALID };

// 0x5CAA48
static int combat_ui_knob_x[TWENTY] = {
    22,
    35,
    48,
    61,
    74,
    93,
    106,
    119,
    132,
    145,
    164,
    177,
    190,
    203,
    216,
    235,
    248,
    261,
    274,
    287,
};

// 0x5CAA18
static tig_window_handle_t combat_ui_ap_bar_window_handle = TIG_WINDOW_HANDLE_INVALID;

// 0x5CAA1C
static tig_window_handle_t combat_ui_compact_ap_bar_window_handle = TIG_WINDOW_HANDLE_INVALID;

// 0x5CAA98
static int combat_ui_ap_icon_nums[AP_COLOR_COUNT] = {
    /*  GREEN */ 295, // ap_green.art
    /* ORANGE */ 296, // ap_orange.art
    /*    RED */ 315, // ap_red.art
};

// 0x680EA8
static tig_art_id_t combat_ui_ap_icon_art_ids[AP_COLOR_COUNT];

// 0x680EB4
static int dword_680EB4;

// 0x680EB8
static bool combat_ui_initialized;

// 0x680EBC
static bool combat_ui_created;

// 0x680EC0
static TigRect combat_ui_ap_bar_loading_indicator_frame;

// 0x56EA90
bool combat_ui_init(GameInitInfo* init_info)
{
    CombatCallbacks callbacks;
    int index;
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigWindowData window_data;

    callbacks.field_0 = combat_ui_combat_mode_changed;
    callbacks.field_4 = combat_ui_recreate;
    callbacks.field_8 = combat_ui_close;
    callbacks.field_C = combat_ui_draw_ap_bar;
    callbacks.field_10 = combat_ui_turn_notify;

    if (!combat_set_callbacks(&callbacks)) {
        return false;
    }

    for (index = 0; index < THREE; index++) {
        if (tig_art_interface_id_create(combat_ui_ap_icon_nums[index], 0, 0, 0, &(combat_ui_ap_icon_art_ids[index])) != TIG_OK) {
            return false;
        }
    }

    tig_art_interface_id_create(601, 0, 0, 0, &art_id);

    if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        return false;
    }

    if (tig_window_data(init_info->iso_window_handle, &window_data) != TIG_OK) {
        return false;
    }

    combat_ui_ap_bar_loading_indicator_frame.x = window_data.rect.x + window_data.rect.width - art_frame_data.width - 2;
    combat_ui_ap_bar_loading_indicator_frame.y = window_data.rect.y + art_frame_data.height + 2;

    tig_art_interface_id_create(601, 0, 0, 0, &combat_ui_ap_bar_loading_indicator_art_id);

    if (tig_art_frame_data(combat_ui_ap_bar_loading_indicator_art_id, &art_frame_data) != TIG_OK) {
        return false;
    }

    combat_ui_ap_bar_loading_indicator_frame.width = art_frame_data.width;
    combat_ui_ap_bar_loading_indicator_frame.height = art_frame_data.height;

    combat_ui_initialized = true;

    return true;
}

// 0x56EBE0
void combat_ui_exit(void)
{
    combat_ui_initialized = false;
}

// 0x56EBF0
void combat_ui_reset(void)
{
    combat_ui_close();
}

// 0x56EC00
void combat_ui_resize(GameResizeInfo* resize_info)
{
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;

    tig_art_interface_id_create(601, 0, 0, 0, &art_id);

    if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        tig_debug_printf("combat_ui_resize: ERROR: tig_art_frame_data failed!\n");
        return;
    }

    combat_ui_ap_bar_loading_indicator_frame.x = resize_info->window_rect.x + resize_info->window_rect.width - art_frame_data.width - 2;
    combat_ui_ap_bar_loading_indicator_frame.y = resize_info->window_rect.y + art_frame_data.height + 4;

    if (combat_ui_created) {
        if (intgame_is_compact_interface()) {
            tig_window_hide(combat_ui_ap_bar_window_handle);
        } else {
            tig_window_show(combat_ui_ap_bar_window_handle);
        }

        if (combat_ui_created) {
            if (intgame_is_compact_interface()) {
                tig_window_show(combat_ui_compact_ap_bar_window_handle);
            } else {
                tig_window_hide(combat_ui_compact_ap_bar_window_handle);
            }
        }
    }
}

// 0x56ECE0
// NOTE: Called through the CombatCallbacks table when a critter enters or
// leaves combat mode: arg 0 on entering, 1 on leaving (faithful to the
// original's call sites in 0x4B3F80). The wmap close on the *exit* branch
// is Troika's own choice, verified at 0x56ECE0.
void combat_ui_combat_mode_changed(bool combat_ended)
{
    intgame_refresh_cursor();
    if (combat_ended) {
        wmap_ui_close();

        // CE (card S1): the aim returns to an ordinary attack when the fight
        // does, so a head shot chosen for one fight is not still chosen for the
        // next. Here rather than on the UI's own toggle because this callback
        // is reached from `combat_critter_toggle_combat_mode` for **both**
        // ways out - the player pressing R, and the fight simply ending when
        // the last opponent falls. Session 6 could only cover the first.
        //
        // Already local-PC only: the call site guards it with `if (is_pc)`.
        called_shot_reset();
    }

    // Shown during the local player's standard turn-based combat and hidden
    // everywhere else, on both edges of the transition.
    called_shot_ui_refresh();
}

// 0x56ED00
void combat_ui_recreate(void)
{
    if (combat_ui_created) {
        combat_ui_close();
    }
    combat_ui_create();
    combat_ui_draw_ap_bar(0);

    // CE (card S1): the panel sits beside this bar and is relocated by the
    // same layout change, so it is rebuilt with it rather than tracking the
    // layout on its own.
    called_shot_ui_destroy();
    called_shot_ui_create();
}

// 0x56ED20
void combat_ui_create(void)
{
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigRect rect;
    TigWindowData window_data;
    TigArtBlitInfo blit_info;

    if (tig_art_interface_id_create(294, 0, 0, 0, &art_id) != TIG_OK) {
        return;
    }

    if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        return;
    }

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        return;
    }

    rect.x = 222;

    // UAP (0x56EDA7): the combat bar sits one pixel higher, y 412 -> 411.
    rect.y = uap_fixes_enabled() ? 411 : 412;
    rect.width = art_frame_data.width;
    rect.height = art_frame_data.height;

    window_data.flags = TIG_WINDOW_MESSAGE_FILTER;
    window_data.rect = rect;
    window_data.background_color = art_anim_data.color_key;
    window_data.color_key = art_anim_data.color_key;
    window_data.message_filter = combat_ui_message_filter;
    hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);

    if (tig_window_create(&window_data, &combat_ui_ap_bar_window_handle) != TIG_OK) {
        return;
    }

    rect.x = 0;
    rect.y = 0;

    blit_info.flags = 0;
    blit_info.art_id = art_id;
    blit_info.src_rect = &rect;
    blit_info.dst_rect = &rect;
    tig_window_blit_art(combat_ui_ap_bar_window_handle, &blit_info);

    if (!intgame_button_create_ex(combat_ui_ap_bar_window_handle, &rect, &combat_ui_end_turn_button_info, TIG_BUTTON_MOMENTARY)) {
        return;
    }

    combat_ui_created = true;
    dword_680EB4 = 0;

    combat_ui_refresh();
    combat_ui_draw_ap_bar(0);

    if (intgame_is_compact_interface()) {
        tig_window_hide(combat_ui_ap_bar_window_handle);
    }

    window_data.flags = TIG_WINDOW_TRANSPARENT | TIG_WINDOW_MESSAGE_FILTER;
    window_data.rect = combat_ui_compact_ap_bar_frame;
    window_data.background_color = art_anim_data.color_key;
    window_data.color_key = tig_color_make(0, 0, 255);
    window_data.message_filter = combat_ui_message_filter;
    hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);

    if (tig_window_create(&window_data, &combat_ui_compact_ap_bar_window_handle) != TIG_OK) {
        return;
    }

    combat_ui_draw_compact_ap_bar(0);

    if (!intgame_is_compact_interface()) {
        tig_window_hide(combat_ui_compact_ap_bar_window_handle);
    }
}

// 0x56EF40
void combat_ui_close(void)
{
    called_shot_ui_destroy();
    combat_ui_destroy();
}

// 0x56EF50
void combat_ui_destroy(void)
{
    if (!combat_ui_created) {
        return;
    }

    if (tig_window_destroy(combat_ui_ap_bar_window_handle) == TIG_OK) {
        combat_ui_ap_bar_window_handle = TIG_WINDOW_HANDLE_INVALID;
    }

    if (combat_ui_compact_ap_bar_window_handle != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_destroy(combat_ui_compact_ap_bar_window_handle);
        combat_ui_compact_ap_bar_window_handle = TIG_WINDOW_HANDLE_INVALID;
    }

    combat_ui_created = false;
}

// 0x56EFA0
void combat_ui_draw_compact_ap_bar(int available_ap)
{
    int red_ap = 0;
    int orange_ap;
    int green_ap;
    TigRect rect;

    if (combat_ui_compact_ap_bar_window_handle == TIG_WINDOW_HANDLE_INVALID) {
        return;
    }

    if (!player_is_local_pc_obj(combat_turn_based_whos_turn_get())) {
        return;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = combat_ui_compact_ap_bar_frame.width;
    rect.height = combat_ui_compact_ap_bar_frame.height;
    tig_window_fill(combat_ui_compact_ap_bar_window_handle,
        &rect,
        tig_color_make(0, 0, 0));

    rect.x += 1;
    rect.y += 1;
    rect.width -= 2;
    rect.height -= 2;
    tig_window_box(combat_ui_compact_ap_bar_window_handle,
        &rect,
        tig_color_make(0, 0, 255));

    rect.x = 0;
    rect.y = 0;
    rect.width = combat_ui_compact_ap_bar_frame.width;
    rect.height = combat_ui_compact_ap_bar_frame.height;

    orange_ap = combat_required_action_points_get();
    green_ap = available_ap - orange_ap;
    if (green_ap >= 25) {
        red_ap = 0;
        orange_ap = 0;
        green_ap = 25;
    } else if (orange_ap > available_ap) {
        red_ap = orange_ap - available_ap;
        orange_ap = available_ap;
        green_ap = 0;
    }

    if (orange_ap + green_ap > 25) {
        orange_ap = 25 - green_ap;
    }

    if (red_ap + orange_ap + green_ap > 25) {
        red_ap = 25 - orange_ap - green_ap;
    }

    rect.x = 2;
    rect.width = 120 * green_ap / 25;
    if (rect.width > 0) {
        combat_ui_draw_ap_bar_segment(combat_ui_compact_ap_bar_window_handle,
            &rect,
            tig_color_make(0, 255, 0));
    }

    rect.x += rect.width;
    rect.width = 120 * orange_ap / 25;
    if (rect.width > 0) {
        combat_ui_draw_ap_bar_segment(combat_ui_compact_ap_bar_window_handle,
            &rect,
            tig_color_make(255, 128, 0));
    }

    rect.x += rect.width;
    rect.width = 120 * red_ap / 25;
    if (rect.width > 0) {
        combat_ui_draw_ap_bar_segment(combat_ui_compact_ap_bar_window_handle,
            &rect,
            tig_color_make(255, 0, 0));
    }
}

// 0x56F2F0
void combat_ui_draw_ap_bar_segment(tig_window_handle_t window_handle, TigRect* rect, tig_color_t color)
{
    bool odd = false;
    int cnt;
    TigRect line_rect;
    tig_color_t color1;
    tig_color_t color2;

    line_rect.x = rect->x;
    line_rect.y = 2;
    line_rect.width = 1;
    line_rect.height = rect->height - 4;

    cnt = rect->width / 2;

    if ((rect->width & 1) != 0) {
        odd = true;
    }

    if ((rect->x & 1) != 0) {
        color1 = color;
        color2 = tig_color_make(0, 0, 0);
    } else {
        color1 = tig_color_make(0, 0, 0);
        color2 = color;
    }

    line_rect.x = rect->x;

    while (cnt > 0) {
        tig_window_fill(window_handle, &line_rect, color1);
        line_rect.x++;
        tig_window_fill(window_handle, &line_rect, color2);
        line_rect.x++;
        cnt--;
    }

    if (odd) {
        tig_window_fill(window_handle, &line_rect, color1);
    }
}

// 0x56F430
void combat_ui_draw_ap_bar(int ap_total)
{
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    int action_points;
    int green = 0;
    int orange = 0;
    int red = 0;
    int knob = 0;

    if (!combat_ui_created) {
        return;
    }

    combat_ui_draw_compact_ap_bar(ap_total);

    if (tig_art_interface_id_create(294, 0, 0, 0, &art_id) == TIG_OK
        && tig_art_frame_data(art_id, &art_frame_data) == TIG_OK
        && tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.height;

        art_blit_info.flags = 0;
        art_blit_info.art_id = art_id;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &src_rect;
        tig_window_blit_art(combat_ui_ap_bar_window_handle, &art_blit_info);
    }

    if (ap_total != 0) {
        action_points = combat_required_action_points_get();
        orange = action_points;
        green = ap_total - action_points;
        if (green >= TWENTY) {
            green = TWENTY;
            orange = 0;
            red = 0;
        } else if (action_points > ap_total) {
            orange = ap_total;
            red = action_points - ap_total;
            green = 0;
        }

        if (orange + green > TWENTY) {
            orange = TWENTY - green;
        }

        if (red + orange + green > TWENTY) {
            red = TWENTY - orange - green;
        }
    }

    if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.height;

        dst_rect.x = 0;
        dst_rect.y = 10;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.height;

        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;

        art_blit_info.art_id = combat_ui_ap_icon_art_ids[AP_COLOR_GREEN];
        while (green > 0) {
            dst_rect.x = combat_ui_knob_x[knob];
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &art_blit_info);
            green--;
            knob++;
        }

        art_blit_info.art_id = combat_ui_ap_icon_art_ids[AP_COLOR_ORANGE];
        while (orange > 0) {
            dst_rect.x = combat_ui_knob_x[knob];
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &art_blit_info);
            orange--;
            knob++;
        }

        art_blit_info.art_id = combat_ui_ap_icon_art_ids[AP_COLOR_RED];
        while (red > 0) {
            dst_rect.x = combat_ui_knob_x[knob];
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &art_blit_info);
            red--;
            knob++;
        }
    }
}

// 0x56F660
void combat_ui_refresh(void)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigRect rect;
    TigRect src_rect;
    TigRect dst_rect;
    TigArtBlitInfo blit_info;

    if (!combat_ui_created) {
        return;
    }

    combat_ui_draw_compact_npc_turn_indicator();

    obj = combat_turn_based_whos_turn_get();
    if (!player_is_local_pc_obj(obj)) {
        if (dword_680EB4 >= TWENTY) {
            dword_680EB4 = 0;
        }

        if (tig_art_interface_id_create(294, 0, 0, 0, &art_id) == TIG_OK
            && tig_art_frame_data(art_id, &art_frame_data) == TIG_OK
            && tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            rect.x = 0;
            rect.y = 0;
            rect.width = art_frame_data.width;
            rect.height = art_frame_data.height;

            blit_info.flags = 0;
            blit_info.art_id = art_id;
            blit_info.src_rect = &rect;
            blit_info.dst_rect = &rect;
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &blit_info);
        }

        if (tig_art_frame_data(art_id, &art_frame_data)) {
            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = combat_ui_knob_x[dword_680EB4];
            dst_rect.y = 10;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.height;

            blit_info.flags = 0;
            blit_info.art_id = combat_ui_ap_icon_art_ids[AP_COLOR_GREEN];
            blit_info.src_rect = &src_rect;
            blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &blit_info);

            dst_rect.x = combat_ui_knob_x[dword_680EB4 - 7 < 0 ? dword_680EB4 + TWENTY - 7 : dword_680EB4 - 7];
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &blit_info);

            dst_rect.x = combat_ui_knob_x[dword_680EB4 + 7 >= TWENTY ? dword_680EB4 - TWENTY + 7 : dword_680EB4 + 7];
            tig_window_blit_art(combat_ui_ap_bar_window_handle, &blit_info);
        }

        dword_680EB4++;
    }

    if (combat_ui_created) {
        anim_ui_event_add_delay(ANIM_UI_EVENT_TYPE_REFRESH_COMBAT_UI, -1, 100);
    }
}

// 0x56F840
void combat_ui_draw_compact_npc_turn_indicator(void)
{
    int64_t obj;
    TigRect rect;

    if (combat_ui_compact_ap_bar_window_handle == TIG_WINDOW_HANDLE_INVALID) {
        return;
    }

    obj = combat_turn_based_whos_turn_get();
    if (player_is_local_pc_obj(obj)) {
        return;
    }

    rect.x = 2;
    rect.y = 2;
    rect.width = combat_ui_compact_ap_bar_frame.width - 4;
    rect.height = combat_ui_compact_ap_bar_frame.height - 4;
    tig_window_fill(combat_ui_compact_ap_bar_window_handle, &rect, tig_color_make(0, 0, 0));

    rect.x = 120 * dword_680EB4 / 25;
    rect.width = 4;
    rect.height += 4;
    combat_ui_draw_ap_bar_segment(combat_ui_compact_ap_bar_window_handle, &rect, tig_color_make(0, 255, 0));
}

// 0x56F990
// NOTE: Unreferenced - stored in the CombatCallbacks table but never invoked
// in CE or the original. The original 0x56F990 is a conditional tail-jump to
// player_get_local_pc_obj (0x40DA50) whose result the caller would discard.
void combat_ui_turn_notify(int64_t obj)
{
    if (obj != OBJ_HANDLE_NULL) {
        player_get_local_pc_obj();
    }

    // CE (card S1): the panel's visibility is refreshed here as well as on the
    // mode change, because entering combat mode runs whole turn-based turns
    // before it returns (MODDING-NOTES quirk 47). A refresh that happens while
    // the mode is still settling can read `combat_turn_based_is_active` as
    // false and hide a panel that nothing would refresh again. A turn
    // notification only arrives once turn-based combat is genuinely running.
    called_shot_ui_refresh();
}

// CE (card O2): is the actor free to act?
//
// Not `!anim_is_busy`: a critter standing still is busy with its fidget goal,
// so reading busy alone would make ending a turn depend on animation timing.
// Lifted verbatim out of the two copies below, and exported because a replay
// needs the same notion of free - see `replay_ui_tick`, which used to read
// `anim_is_busy` directly and could therefore wait forever for a state the
// engine already considered actionable.
bool combat_ui_actor_ready(int64_t obj)
{
    AnimID curr_anim_id;
    AnimID fidget_anim_id;

    if (!anim_is_busy(obj, &curr_anim_id)) {
        return true;
    }

    return anim_is_current_goal_type(obj, AG_ANIM_FIDGET, &fidget_anim_id)
        && anim_id_is_equal(&curr_anim_id, &fidget_anim_id);
}

// CE (card O2): ending the PC's turn, in one place.
//
// The end-turn button and the `E` key carried byte-identical copies of this
// test, and the command recorder - whose boundary is
// `intgame_perform_action_on_target` - could see neither. Measured 2026-09-14:
// a replay that had learned to enter combat and attack then froze in turn 4
// holding thirteen action points, because nothing in the record could end a
// turn. Both inputs now go through here, and so does a replay's `end-turn`
// row, so the recorder sees a replayed end-turn exactly as it saw the original.
//
// Returns whether the engine took it, for the replay's benefit. The conditions
// are the judgement and they live in `combat_observer_end_turn_accepted`, where
// they can be tested without a UI; this function only gathers them.
bool combat_ui_end_turn_action(void)
{
    int64_t obj;
    bool accepted;
    char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

    obj = combat_turn_based_whos_turn_get();
    accepted = combat_observer_end_turn_accepted(obj != OBJ_HANDLE_NULL,
        obj != OBJ_HANDLE_NULL && player_is_local_pc_obj(obj),
        obj != OBJ_HANDLE_NULL && combat_ui_actor_ready(obj));

    if (!combat_observer_is_enabled()) {
        if (accepted) {
            combat_turn_based_next_subturn();
        }
        return accepted;
    }

    // The local PC, not `whos_turn_get`: the player is who pressed the key, and
    // on a refusal the turn belongs to somebody else or to nobody. Naming the
    // turn's owner would record a refused press as an NPC ending its own turn.
    // This is also what the mode events name, so a replay reads one actor field
    // across every recorded player action.
    combat_observer_object_id(player_get_local_pc_obj(), actor);
    combat_observer_end_turn_requested(actor, combat_action_points_get());

    if (accepted) {
        combat_turn_based_next_subturn();
    }

    obj = combat_turn_based_whos_turn_get();
    combat_observer_end_turn_resolved(actor, accepted,
        combat_action_points_get(),
        obj != OBJ_HANDLE_NULL && player_is_local_pc_obj(obj));
    return accepted;
}

// 0x56F9B0
bool combat_ui_message_filter(TigMessage* msg)
{
    switch (msg->type) {
    case TIG_MESSAGE_BUTTON:
        if (msg->data.button.state == TIG_BUTTON_STATE_RELEASED
            && msg->data.button.button_handle == combat_ui_end_turn_button_info.button_handle) {
            combat_ui_end_turn_action();
            return true;
        }
        break;
    case TIG_MESSAGE_KEYBOARD:
        if (!textedit_ui_is_focused()
            && !msg->data.keyboard.pressed
            && msg->data.keyboard.scancode == SDL_SCANCODE_E) {
            combat_ui_end_turn_action();
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}
