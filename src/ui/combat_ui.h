#ifndef ARCANUM_UI_COMBAT_UI_H_
#define ARCANUM_UI_COMBAT_UI_H_

#include "game/context.h"

bool combat_ui_init(GameInitInfo* init_info);
void combat_ui_exit(void);
void combat_ui_reset(void);
void combat_ui_resize(GameResizeInfo* resize_info);
void combat_ui_refresh(void);

// CE (card O2): the end-turn action the button, the `E` key and a replay all
// go through. Returns whether the engine took it. See the definition for why
// the three conditions live in `combat_observer` as a pure judgement.
bool combat_ui_end_turn_action(void);

// CE (card O2): is this critter free to act right now? A standing critter is
// busy with its fidget goal, so this is not `!anim_is_busy`. Shared with the
// replay so the two cannot disagree about when a command may be submitted.
bool combat_ui_actor_ready(int64_t obj);

#endif /* ARCANUM_UI_COMBAT_UI_H_ */
