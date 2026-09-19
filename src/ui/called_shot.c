#include "ui/called_shot.h"

#include <tig/tig.h>

#include "game/combat.h"
#include "game/feature.h"
#include "game/player.h"
#include "ui/intgame.h"

// The kept choice. `HIT_LOC_TORSO` is the shipped behaviour: an ordinary
// attack whose limb `combat_random_hit_loc` rolls.
static int called_shot_selected = HIT_LOC_TORSO;

bool called_shot_enabled(void)
{
    return feature_enabled(FEATURE_CALLED_SHOT_UI);
}

int called_shot_selection(void)
{
    // Reading through the flag rather than only writing through it, so a
    // selection kept from a session where the feature was on cannot leak into
    // one where it is off.
    return called_shot_enabled() ? called_shot_selected : HIT_LOC_TORSO;
}

void called_shot_select(int hit_loc)
{
    if (hit_loc < 0 || hit_loc >= HIT_LOC_COUNT) {
        // Refused, not clamped. This value reaches `hit_loc_penalties[]`,
        // which has exactly `HIT_LOC_COUNT` entries, and clamping would turn a
        // caller's mistake into a silently different aim.
        tig_debug_printf("called_shot: refusing hit location %d\n", hit_loc);
        return;
    }

    called_shot_selected = hit_loc;
}

void called_shot_reset(void)
{
    called_shot_selected = HIT_LOC_TORSO;
}

int called_shot_effective_aim(int selection, bool comma, bool period, bool slash)
{
    // The original's order, preserved exactly: comma, then period, then slash.
    // `intgame_perform_action_on_target_inner` tests them in that sequence and
    // two keys down has always meant the first of them. A selector that
    // reordered this would change a mechanic while claiming to add a control.
    if (comma) {
        return HIT_LOC_HEAD;
    }

    if (period) {
        return HIT_LOC_ARM;
    }

    if (slash) {
        return HIT_LOC_LEG;
    }

    // No key: the kept choice, which is `HIT_LOC_TORSO` unless one was made.
    if (selection < 0 || selection >= HIT_LOC_COUNT) {
        return HIT_LOC_TORSO;
    }

    return selection;
}

int called_shot_aim_now(void)
{
    return called_shot_effective_aim(called_shot_selection(),
        tig_kb_is_key_pressed(SDL_SCANCODE_COMMA),
        tig_kb_is_key_pressed(SDL_SCANCODE_PERIOD),
        tig_kb_is_key_pressed(SDL_SCANCODE_SLASH));
}

/* -- the panel ---------------------------------------------------------- */

// One control per limb, in the order a player reads them.
//
// **The art is chosen by what it depicts.** `cursor-called-leg` is art 819 and
// `cursor-called-arm` is 820, while the key dispatch raises 819 for an ARM shot
// and 820 for a LEG one - Troika's own crossing, recorded as quirk 48 and kept
// because every shipped map and save depends on the behaviour around it. The
// panel is the one place a player checks what they are aiming at, so it maps
// the art to the limb it shows and ignores which key raises it.
//
// `battlecur` is Body: it is the cursor an ordinary attack already uses, and
// Body *is* an ordinary attack - the location is rolled, not aimed at the
// torso.
#define CALLED_SHOT_ART_BODY 353
#define CALLED_SHOT_ART_HEAD 818
#define CALLED_SHOT_ART_LEG 819
#define CALLED_SHOT_ART_ARM 820

#define CALLED_SHOT_CONTROL_SIZE 32
#define CALLED_SHOT_CONTROL_GAP 2

static const int called_shot_art[HIT_LOC_COUNT] = {
    /* HIT_LOC_TORSO */ CALLED_SHOT_ART_BODY,
    /*  HIT_LOC_HEAD */ CALLED_SHOT_ART_HEAD,
    /*   HIT_LOC_ARM */ CALLED_SHOT_ART_ARM,
    /*   HIT_LOC_LEG */ CALLED_SHOT_ART_LEG,
};

// The combat bar is not merely offset between layouts, it is relocated:
// `combat_ui` creates it at { 222, 411 } normally and { 617, 577, 124, 14 }
// compact. The panel sits to its left in both, so the frame is chosen per
// layout rather than derived once.
//
// The normal y is the flag-dependent one: the bar is at 411 with `uap fixes`
// and 412 without, so it is read from `combat_ui` rather than written here.
static TigRect called_shot_frame_normal = { 222 - (CALLED_SHOT_CONTROL_SIZE + CALLED_SHOT_CONTROL_GAP) * HIT_LOC_COUNT - 4, 411,
    (CALLED_SHOT_CONTROL_SIZE + CALLED_SHOT_CONTROL_GAP) * HIT_LOC_COUNT, CALLED_SHOT_CONTROL_SIZE };
static TigRect called_shot_frame_compact = { 617 - (CALLED_SHOT_CONTROL_SIZE + CALLED_SHOT_CONTROL_GAP) * HIT_LOC_COUNT - 4, 577,
    (CALLED_SHOT_CONTROL_SIZE + CALLED_SHOT_CONTROL_GAP) * HIT_LOC_COUNT, CALLED_SHOT_CONTROL_SIZE };

static tig_window_handle_t called_shot_window = TIG_WINDOW_HANDLE_INVALID;
static UiButtonInfo called_shot_buttons[HIT_LOC_COUNT];
static bool called_shot_ui_created;

int called_shot_art_for(int hit_loc)
{
    if (hit_loc < 0 || hit_loc >= HIT_LOC_COUNT) {
        return -1;
    }

    return called_shot_art[hit_loc];
}

static bool called_shot_ui_message_filter(TigMessage* msg)
{
    // Consuming the release rather than the press, which is what every other
    // HUD control here does, and returning true so the click stops at the
    // panel. A click that fell through to the map would order a move or an
    // attack, which is exactly what session 1 said selecting must never do.
    if (msg->type == TIG_MESSAGE_BUTTON
        && msg->data.button.state == TIG_BUTTON_STATE_RELEASED) {
        return called_shot_ui_handle_button(msg->data.button.button_handle);
    }

    return false;
}

static TigRect* called_shot_frame(void)
{
    return intgame_is_compact_interface()
        ? &called_shot_frame_compact
        : &called_shot_frame_normal;
}

bool called_shot_ui_create(void)
{
    TigWindowData window_data;
    TigRect* frame;
    int index;

    if (!called_shot_enabled() || called_shot_ui_created) {
        return called_shot_ui_created;
    }

    frame = called_shot_frame();

    window_data.flags = TIG_WINDOW_MESSAGE_FILTER;
    window_data.rect = *frame;
    window_data.background_color = 0;
    window_data.color_key = 0;
    window_data.message_filter = called_shot_ui_message_filter;

    if (tig_window_create(&window_data, &called_shot_window) != TIG_OK) {
        tig_debug_printf("called_shot: could not create the panel window\n");
        return false;
    }

    for (index = 0; index < HIT_LOC_COUNT; index++) {
        called_shot_buttons[index].x = frame->x
            + index * (CALLED_SHOT_CONTROL_SIZE + CALLED_SHOT_CONTROL_GAP);
        called_shot_buttons[index].y = frame->y;
        called_shot_buttons[index].art_num = called_shot_art[index];
        called_shot_buttons[index].button_handle = TIG_BUTTON_HANDLE_INVALID;

        if (!intgame_button_create_ex(called_shot_window, frame,
                &(called_shot_buttons[index]), 0)) {
            tig_debug_printf("called_shot: could not create control %d\n", index);
            tig_window_destroy(called_shot_window);
            called_shot_window = TIG_WINDOW_HANDLE_INVALID;
            return false;
        }
    }

    called_shot_ui_created = true;
    called_shot_ui_refresh();
    return true;
}

void called_shot_ui_destroy(void)
{
    if (!called_shot_ui_created) {
        return;
    }

    tig_window_destroy(called_shot_window);
    called_shot_window = TIG_WINDOW_HANDLE_INVALID;
    called_shot_ui_created = false;
}

void called_shot_ui_refresh(void)
{
    int64_t pc_obj;

    if (!called_shot_ui_created) {
        return;
    }

    // Session 1: shown only during the local player's standard turn-based
    // combat, and hidden outright elsewhere rather than shown disabled. Real
    // time and fast turn-based support are deferred, and an inert control is
    // worse than none.
    pc_obj = player_get_local_pc_obj();
    if (pc_obj != OBJ_HANDLE_NULL
        && combat_critter_is_combat_mode_active(pc_obj)
        && combat_turn_based_is_active()) {
        tig_window_show(called_shot_window);
    } else {
        tig_window_hide(called_shot_window);
    }
}

bool called_shot_ui_handle_button(tig_button_handle_t button_handle)
{
    int index;

    if (!called_shot_ui_created) {
        return false;
    }

    for (index = 0; index < HIT_LOC_COUNT; index++) {
        if (called_shot_buttons[index].button_handle == button_handle) {
            // Selecting costs nothing: no attack, no action points, no
            // ammunition, no randomness. Session 1's contract, and the reason
            // this is a click rather than a command.
            called_shot_select(index);
            return true;
        }
    }

    return false;
}
