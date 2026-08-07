#ifndef ARCANUM_GAME_LI_H_
#define ARCANUM_GAME_LI_H_

#include "game/context.h"

bool li_init(GameInitInfo* init_info);
void li_exit(void);
void li_resize(GameResizeInfo* resize_info);
void li_disable(void);
void li_enable(void);
int li_disable_count_get(void);
void li_update(void);
void li_redraw(void);

#endif /* ARCANUM_GAME_LI_H_ */
