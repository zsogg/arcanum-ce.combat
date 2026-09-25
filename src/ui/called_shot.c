#include "ui/called_shot.h"

#include <tig/tig.h>

#include "game/combat.h"
#include "game/feature.h"
#include "game/hrp.h"
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

bool called_shot_throws_enabled(void)
{
    // Its own row, deliberately not `called shot ui`. The selector is a way to
    // choose an aim the original already accepted from the keyboard; an aimed
    // throw is an aim the original never accepted at all. Tying them together
    // would mean switching on a HUD control silently added a mechanic.
    return feature_enabled(FEATURE_CALLED_SHOT_THROWS);
}

int called_shot_throw_aim_for(bool enabled, int committed_aim)
{
    if (!enabled) {
        // The shipped behaviour, and not "aim at the torso": `combat_attack`
        // reads `HIT_LOC_TORSO` as unaimed and rolls the location at
        // 70/15/10/5, which is what a thrown weapon has always done.
        return HIT_LOC_TORSO;
    }

    if (committed_aim <= HIT_LOC_TORSO || committed_aim >= HIT_LOC_COUNT) {
        return HIT_LOC_TORSO;
    }

    return committed_aim;
}

int called_shot_throw_aim(int committed_aim)
{
    return called_shot_throw_aim_for(called_shot_throws_enabled(), committed_aim);
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

// -1 when no replayed aim is in force. Not a bool plus a value, because the
// two can disagree and then the question "is an aim forced" has two answers.
static int called_shot_replay_aim = -1;

void called_shot_replay_aim_set(int hit_loc)
{
    if (hit_loc < 0 || hit_loc >= HIT_LOC_COUNT) {
        // A recording that names a limb this build has no table entry for is
        // refused rather than clamped: replaying it as a different aim would
        // be a silently wrong reproduction, which is worse than a loud one.
        tig_debug_printf("called_shot: replay aim %d is not a hit location\n",
            hit_loc);
        called_shot_replay_aim = -1;
        return;
    }

    called_shot_replay_aim = hit_loc;
}

void called_shot_replay_aim_clear(void)
{
    called_shot_replay_aim = -1;
}

int called_shot_aim_now(void)
{
    // The recording wins while it is in force. The panel and the keyboard
    // describe what a player wants next; a replay is reproducing what they
    // wanted then.
    if (called_shot_replay_aim >= 0) {
        return called_shot_replay_aim;
    }

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
// because the shipped behaviour around it is what every save depends on. The
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

// The cursor art is 32x37. The controls are sized to it rather than to a round
// number, so a click anywhere on a control is a click on its picture.
#define CALLED_SHOT_CONTROL_WIDTH 32
#define CALLED_SHOT_CONTROL_HEIGHT 37
#define CALLED_SHOT_CONTROL_GAP 2
#define CALLED_SHOT_PANEL_WIDTH \
    ((CALLED_SHOT_CONTROL_WIDTH + CALLED_SHOT_CONTROL_GAP) * HIT_LOC_COUNT)

static const int called_shot_art[HIT_LOC_COUNT] = {
    /* HIT_LOC_TORSO */ CALLED_SHOT_ART_BODY,
    /*  HIT_LOC_HEAD */ CALLED_SHOT_ART_HEAD,
    /*   HIT_LOC_ARM */ CALLED_SHOT_ART_ARM,
    /*   HIT_LOC_LEG */ CALLED_SHOT_ART_LEG,
};

// The combat bar is not merely offset between layouts, it is relocated:
// `combat_ui` creates it at { 222, 411 } normally and { 617, 577 } compact. The
// panel sits to its left in both, so the frame is chosen per layout rather than
// derived once. Both are authored in the 800x600 design space and mapped by
// `hrp_apply`, exactly as the bar is.
static TigRect called_shot_frame_normal = { 222 - CALLED_SHOT_PANEL_WIDTH - 4,
    411, CALLED_SHOT_PANEL_WIDTH, CALLED_SHOT_CONTROL_HEIGHT };
static TigRect called_shot_frame_compact = { 617 - CALLED_SHOT_PANEL_WIDTH - 4,
    577, CALLED_SHOT_PANEL_WIDTH, CALLED_SHOT_CONTROL_HEIGHT };

static tig_window_handle_t called_shot_window = TIG_WINDOW_HANDLE_INVALID;
static TigRect called_shot_placed;
static bool called_shot_ui_created;
static bool called_shot_ui_shown;

static TigRect* called_shot_frame(void)
{
    return intgame_is_compact_interface()
        ? &called_shot_frame_compact
        : &called_shot_frame_normal;
}

// One control's box, in window coordinates.
static void called_shot_control_rect(int index, TigRect* rect)
{
    rect->x = index * (CALLED_SHOT_CONTROL_WIDTH + CALLED_SHOT_CONTROL_GAP);
    rect->y = 0;
    rect->width = CALLED_SHOT_CONTROL_WIDTH;
    rect->height = CALLED_SHOT_CONTROL_HEIGHT;
}

int called_shot_art_for(int hit_loc)
{
    if (hit_loc < 0 || hit_loc >= HIT_LOC_COUNT) {
        return -1;
    }

    return called_shot_art[hit_loc];
}

/**
 * Which control a screen position falls in, or -1.
 *
 * **Rectangles, not art pixels.** `tig_button` hit-tests by sampling the
 * button's art and rejecting a colour-key pixel, which is right for a drawn
 * button and wrong for these: the limb pictures are cursors and are mostly
 * transparent, so most of each control was a hole that swallowed the click.
 * That is why the first panel accepted no clicks at all while drawing
 * perfectly.
 */
int called_shot_control_index_at(const TigRect* panel, int x, int y)
{
    TigRect rect;
    int index;

    if (panel == NULL) {
        return -1;
    }

    for (index = 0; index < HIT_LOC_COUNT; index++) {
        called_shot_control_rect(index, &rect);
        if (x >= panel->x + rect.x
            && x < panel->x + rect.x + rect.width
            && y >= panel->y + rect.y
            && y < panel->y + rect.y + rect.height) {
            return index;
        }
    }

    return -1;
}

bool called_shot_control_point(int hit_loc, int* x, int* y)
{
    TigRect rect;

    if (!called_shot_ui_created || !called_shot_ui_shown
        || hit_loc < 0 || hit_loc >= HIT_LOC_COUNT) {
        return false;
    }

    called_shot_control_rect(hit_loc, &rect);
    *x = called_shot_placed.x + rect.x + rect.width / 2;
    *y = called_shot_placed.y + rect.y + rect.height / 2;
    return true;
}

static int called_shot_control_at(int x, int y)
{
    return called_shot_control_index_at(&called_shot_placed, x, y);
}

static void called_shot_ui_draw(void)
{
    TigArtBlitInfo blit_info;
    TigArtFrameData frame_data;
    tig_art_id_t art_id;
    TigRect rect;
    TigRect src;
    TigRect whole;
    int index;

    if (!called_shot_ui_created) {
        return;
    }

    whole.x = 0;
    whole.y = 0;
    whole.width = called_shot_placed.width;
    whole.height = called_shot_placed.height;
    tig_window_fill(called_shot_window, &whole, 0);

    for (index = 0; index < HIT_LOC_COUNT; index++) {
        called_shot_control_rect(index, &rect);

        if (tig_art_interface_id_create(called_shot_art[index], 0, 0, 0, &art_id) == TIG_OK
            && tig_art_frame_data(art_id, &frame_data) == TIG_OK) {
            src.x = 0;
            src.y = 0;
            src.width = frame_data.width < rect.width ? frame_data.width : rect.width;
            src.height = frame_data.height < rect.height ? frame_data.height : rect.height;

            blit_info.flags = 0;
            blit_info.art_id = art_id;
            blit_info.src_rect = &src;
            blit_info.dst_rect = &rect;
            tig_window_blit_art(called_shot_window, &blit_info);
        }

        // The selected control is boxed. Not colour alone: session 1 asked for
        // an indicator a player can see without relying on it.
        if (index == called_shot_selection()) {
            tig_window_box(called_shot_window, &rect, tig_color_make(255, 255, 255));
        }
    }
}

static bool called_shot_ui_message_filter(TigMessage* msg)
{
    int index;

    // Every visible window with a filter sees every message, so this has to
    // decide for itself whether a position is its own.
    if (msg->type != TIG_MESSAGE_MOUSE || !called_shot_ui_created
        || !called_shot_ui_shown) {
        return false;
    }

    if (msg->data.mouse.event != TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN
        && msg->data.mouse.event != TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP) {
        return false;
    }

    index = called_shot_control_at(msg->data.mouse.x, msg->data.mouse.y);
    if (index < 0) {
        return false;
    }

    // Both halves of the click are consumed. Claiming only the release would
    // leave the press to reach the map, which orders a move or an attack -
    // exactly what session 1 said selecting must never do.
    if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP) {
        called_shot_select(index);
        called_shot_ui_draw();
        tig_debug_printf("called_shot: aiming at %d\n", index);
    }

    return true;
}

bool called_shot_ui_create(void)
{
    TigWindowData window_data;
    TigRect* frame;

    if (!called_shot_enabled() || called_shot_ui_created) {
        return called_shot_ui_created;
    }

    frame = called_shot_frame();
    called_shot_placed = *frame;

    // **The design space is 800x600 and the screen is not.** `combat_ui` maps
    // its bar with the same call, and the first build of this panel omitted it:
    // the bar moved to the centre and the panel stayed at its raw coordinates,
    // against the left edge two thirds of the way down.
    hrp_apply(&called_shot_placed, GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);

    window_data.flags = TIG_WINDOW_MESSAGE_FILTER;
    window_data.rect = called_shot_placed;
    window_data.background_color = 0;
    window_data.color_key = 0;
    window_data.message_filter = called_shot_ui_message_filter;

    if (tig_window_create(&window_data, &called_shot_window) != TIG_OK) {
        tig_debug_printf("called_shot: could not create the panel window\n");
        return false;
    }

    called_shot_ui_created = true;
    tig_debug_printf("called_shot: panel at %d,%d %dx%d (authored %d,%d)\n",
        called_shot_placed.x, called_shot_placed.y, called_shot_placed.width,
        called_shot_placed.height, frame->x, frame->y);

    called_shot_ui_draw();
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
    called_shot_ui_shown = false;
}

void called_shot_ui_refresh(void)
{
    int64_t pc_obj;
    bool was_shown = called_shot_ui_shown;

    // Real time (2026-09-25): the panel's window is built with the turn-based
    // combat bar (`combat_ui_recreate`) and torn down with it
    // (`combat_ui_close`), so in real-time combat mode it may not exist. It is
    // built here when it should be shown; `called_shot_ui_create` refreshes
    // again once it has.
    if (!called_shot_ui_created) {
        pc_obj = player_get_local_pc_obj();
        if (called_shot_enabled()
            && pc_obj != OBJ_HANDLE_NULL
            && combat_critter_is_combat_mode_active(pc_obj)) {
            called_shot_ui_create();
        }
        return;
    }

    // Shown while the local player is in combat mode, and hidden outright
    // elsewhere rather than shown disabled: an inert control is worse than
    // none. Session 1 showed it only in turn-based combat, deferring real time.
    // Real time was added on 2026-09-25 at zsogg's request. A real-time attack
    // commits its aim through `intgame_perform_action_on_target_inner`, the
    // same route as a turn-based one, and the comma, period and slash keys it
    // mirrors have always worked there, so the panel was all that was missing.
    // The combat-mode change that shows or hides it reaches
    // `combat_ui_combat_mode_changed` in both modes.
    pc_obj = player_get_local_pc_obj();
    if (pc_obj != OBJ_HANDLE_NULL
        && combat_critter_is_combat_mode_active(pc_obj)) {
        tig_window_show(called_shot_window);
        called_shot_ui_shown = true;
    } else {
        tig_window_hide(called_shot_window);
        called_shot_ui_shown = false;
    }

    if (called_shot_ui_shown != was_shown) {
        if (called_shot_ui_shown) {
            called_shot_ui_draw();
        }
        tig_debug_printf("called_shot: panel %s\n",
            called_shot_ui_shown ? "shown" : "hidden");
    }
}

bool called_shot_ui_handle_button(tig_button_handle_t button_handle)
{
    // Kept so the header's contract still holds for a caller that routes
    // button messages here. The panel no longer uses `tig_button`: its art is
    // cursor art and `tig_button_get_at_position` rejects a colour-key pixel,
    // which made most of every control a hole.
    (void)button_handle;
    return false;
}
