#ifndef ARCANUM_GAME_NAME_H_
#define ARCANUM_GAME_NAME_H_

#include "game/context.h"

bool name_init(GameInitInfo* init_info);
void name_exit(void);
tig_art_id_t name_normalize_aid(tig_art_id_t aid);
int name_resolve_path(tig_art_id_t aid, char* path, size_t maxlen);
bool name_art_browser_run(int type, int* extra, int* result);

#endif /* ARCANUM_GAME_NAME_H_ */
