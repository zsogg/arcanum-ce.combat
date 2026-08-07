#ifndef ARCANUM_GAME_PROTO_H_
#define ARCANUM_GAME_PROTO_H_

#include "game/context.h"
#include "game/obj.h"

bool proto_init(GameInitInfo* init_info);
void proto_exit(void);
int64_t proto_get_first_by_type(ObjectType object_type);
int64_t proto_get_by_description(int description);
bool proto_is_valid(int description);
int proto_get_description(int64_t obj);

#endif /* ARCANUM_GAME_PROTO_H_ */
