#include "ui/called_shot.h"

#include <tig/tig.h>

#include "game/feature.h"

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
