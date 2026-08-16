#ifndef ARCANUM_GAME_CI_H_
#define ARCANUM_GAME_CI_H_

#include "game/context.h"

bool ci_init(GameInitInfo* init_info);
void ci_exit(void);
void ci_lock(void);
void ci_unlock(void);
int ci_lock_count(void);
void ci_update(void);
void ci_redraw(void);

#endif /* ARCANUM_GAME_CI_H_ */
