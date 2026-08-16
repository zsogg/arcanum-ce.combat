#ifndef ARCANUM_UI_SKILL_UI_H_
#define ARCANUM_UI_SKILL_UI_H_

#include "game/context.h"
#include "game/target.h"

bool skill_ui_init(GameInitInfo* init_info);
void skill_ui_reset(void);
void skill_ui_exit(void);
int skill_ui_button_art_num(int index);
int skill_ui_icon_art_num(int index);
int skill_ui_current_type_get(void);
void skill_ui_begin(int64_t obj, int type);
void skill_ui_preprocess(int64_t obj, int type);
void skill_ui_cancel(void);
void skill_ui_apply(TargetDescriptor* td);
void skill_ui_activate(TargetDescriptor* td, int64_t obj, int a3);
int skill_ui_skill(int index);
int skill_ui_skill_for_action(int a1, TargetDescriptor* td);
char* skill_ui_name(int index);
void skill_ui_adjust_skill(int64_t obj, int skill, int action);
void skill_ui_set_training(int64_t obj, int skill, int training);
void skill_ui_inc_skill(int64_t obj, int skill);
void skill_ui_dec_skill(int64_t obj, int skill);

#endif /* ARCANUM_UI_SKILL_UI_H_ */
