#ifndef ARCANUM_GAME_RANDOM_H_
#define ARCANUM_GAME_RANDOM_H_

#include "game/context.h"

bool random_init(GameInitInfo* init_info);
void random_exit(void);
void random_seed(unsigned int value);

// CE (card O2): the generator's whole state, which is what `random_seed` sets.
// Capture it at a run's Ready barrier and restore it with `random_seed`.
unsigned int random_state_get(void);
unsigned int random_seed_generate(void);
int random_between(int lower, int upper);
int random_rand(void);

#endif /* ARCANUM_GAME_RANDOM_H_ */
