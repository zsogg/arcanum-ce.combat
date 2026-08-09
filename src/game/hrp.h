#ifndef ARCANUM_GAME_HRP_H_
#define ARCANUM_GAME_HRP_H_

#include "game/context.h"

typedef unsigned int Gravity;

#define GRAVITY_LEFT 0x01
#define GRAVITY_RIGHT 0x02
#define GRAVITY_TOP 0x04
#define GRAVITY_BOTTOM 0x08
#define GRAVITY_CENTER_HORIZONTAL 0x10
#define GRAVITY_CENTER_VERTICAL 0x20

/**
 * The vertical gravity the `MenuPosition` setting asks for.
 *
 * Callers that place, or un-project mouse input for, one of the panels the
 * high resolution patch moves with `HeightBonus` use this in place of a
 * hardcoded `GRAVITY_CENTER_VERTICAL`. Placement and un-projection for the same
 * panel must pass the same value or clicks land off the controls, which is why
 * this is a function rather than a constant per call site.
 *
 * Panels the patch always centres - the main menu, the slide show, the pc lens,
 * the turn-based and confirmation modals - keep `GRAVITY_CENTER_VERTICAL`.
 */
Gravity hrp_menu_gravity(void);

bool hrp_init(GameInitInfo* init_info);
void hrp_resize(GameResizeInfo* resize_info);
void hrp_apply(TigRect* rect, Gravity gravity);
void hrp_center(int* x, int* y);
int hrp_iso_window_width_get(void);
int hrp_iso_window_height_get(void);

#endif /* ARCANUM_GAME_HRP_H_ */
