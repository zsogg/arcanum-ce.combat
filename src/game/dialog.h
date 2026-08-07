#ifndef ARCANUM_GAME_DIALOG_H_
#define ARCANUM_GAME_DIALOG_H_

#include "game/context.h"
#include "game/obj.h"
#include "game/object.h"

/**
 * What the dialog engine has decided should happen next; consumed by the
 * dialog UI after each processed option (see the switch in
 * `dialog_ui_process_option`). The first three values are set by
 * `dialog_perform_response` from a dialog file's {R} response field, per
 * Troika's Eventscripts.doc: positive is a dialog line to continue at, zero
 * returns to the calling script, negative runs the given script line (they
 * coincide with the first three DialogResponseType values). The remaining
 * values hand control to another screen; they are produced by the
 * generated-dialog opcodes ({B:}, {L:}) and by the CE/SU/II/RI result actions.
 */
typedef enum DialogStatus {
    DIALOG_STATUS_CONTINUE, // show reply + options; status_num is the dialog line
    DIALOG_STATUS_RETURN_TO_SCRIPT, // end dialog, resume the calling script
    DIALOG_STATUS_RUN_SCRIPT_LINE, // end dialog, run script line status_num
    DIALOG_STATUS_BARTER, // open barter; status_num holds the pending response_val
    DIALOG_STATUS_FLOAT_AND_END, // float the reply as a bubble and end without opening dialog
    DIALOG_STATUS_CHAREDIT, // open the character sheet (passive)
    DIALOG_STATUS_WORLDMAP, // open the world map
    DIALOG_STATUS_SCHEMATICS, // open the schematics screen
    DIALOG_STATUS_IDENTIFY, // open the identify-items screen
    DIALOG_STATUS_REPAIR, // open the repair-items screen
} DialogStatus;

/**
 * What choosing a PC option does, dispatched by `dialog_perform_response`.
 * The first three values are what `dialog_response_val_decode` produces from a
 * dialog file's {R} response field (and deliberately coincide with the first
 * three DialogStatus values, which those responses set verbatim). The rest are
 * the steps of the generated-dialog flows (GeneratedDialog.doc): the opcode
 * parser turns {B:}/{C:}/{D:}/{H:}/{L:}/{P:}/{R:}/{T:}/{X:} options into their
 * entry steps, and each step's handler builds the options for the next one.
 */
typedef enum DialogResponseType {
    DIALOG_RESPONSE_GO_TO_LINE,
    DIALOG_RESPONSE_RETURN_TO_SCRIPT,
    DIALOG_RESPONSE_RUN_SCRIPT_LINE,
    DIALOG_RESPONSE_BARTER,
    DIALOG_RESPONSE_PAY_MONEY,
    DIALOG_RESPONSE_OFFER_TRAINING,
    DIALOG_RESPONSE_ASK_MONEY_FOR_TRAINING,
    DIALOG_RESPONSE_PERFORM_TRAINING,
    DIALOG_RESPONSE_ASK_MONEY_FOR_RUMOR,
    DIALOG_RESPONSE_TELL_RUMOR,
    DIALOG_RESPONSE_INSULT_REPLY,
    DIALOG_RESPONSE_OFFER_HEALING,
    DIALOG_RESPONSE_OFFER_MAGICKAL_HEALING,
    DIALOG_RESPONSE_OFFER_MAGICKAL_POISON_HEALING,
    DIALOG_RESPONSE_ASK_MONEY_FOR_SKILL,
    DIALOG_RESPONSE_ASK_MONEY_FOR_SPELL,
    DIALOG_RESPONSE_USE_SKILL,
    DIALOG_RESPONSE_USE_SPELL,
    DIALOG_RESPONSE_OFFER_DIRECTIONS,
    DIALOG_RESPONSE_ASK_MONEY_FOR_DIRECTIONS,
    DIALOG_RESPONSE_GIVE_DIRECTIONS,
    DIALOG_RESPONSE_OFFER_MARK_AREA,
    DIALOG_RESPONSE_ASK_MONEY_FOR_MARK_AREA,
    DIALOG_RESPONSE_MARK_AREA,
    DIALOG_RESPONSE_CHECK_STORY,
    DIALOG_RESPONSE_ASK_ABOUT_NEWSPAPERS,
    DIALOG_RESPONSE_OFFER_TODAY_NEWSPAPER,
    DIALOG_RESPONSE_OFFER_OLDER_NEWSPAPER,
    DIALOG_RESPONSE_ASK_MONEY_FOR_NEWSPAPER,
    DIALOG_RESPONSE_BUY_NEWSPAPER,
    DIALOG_RESPONSE_WORLDMAP,
} DialogResponseType;

typedef struct DialogState {
    /* 0000 */ int dlg;
    /* 0008 */ int64_t pc_obj;
    /* 0010 */ ObjectLocator field_10;
    /* 0038 */ int64_t npc_obj;
    /* 0040 */ ObjectLocator field_40;
    /* 0068 */ int num;
    /* 006C */ int script_num;
    /* 0070 */ char reply[1000];
    /* 0458 */ int speech_id;
    /* 045C */ int num_options;
    /* 0460 */ char options[5][1000];
    /* 17E8 */ int status;
    /* 17EC */ int status_num;
    /* 17F0 */ int option_response_types[5];
    /* 1804 */ int option_response_nums[5];
    /* 1818 */ int option_response_extras[5];
    /* 182C */ char* actions[5];
    /* 1840 */ int field_1840;
    /* 1844 */ unsigned int seed;
} DialogState;

bool dialog_init(GameInitInfo* init_info);
void dialog_exit(void);
bool dialog_load(const char* path, int* dlg_ptr);
void dialog_unload(int dlg);
void sub_412F60(int dlg);
bool sub_412FD0(DialogState* state);
void sub_413130(DialogState* state, int index);
void sub_413280(DialogState* state);
void sub_4132A0(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_farewell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_sell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_sell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_normally_wont_sell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_buy_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_buy_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_buy_stolen_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_normally_wont_buy_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_buy_for_less_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_not_enough_money_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_let_me_handle_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void sub_413A30(DialogState* a1, bool a2);
void dialog_copy_npc_wont_follow_msg(int64_t npc_obj, int64_t pc_obj, int reason, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_order_ok_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_order_no_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_insult_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_accidental_attack_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_fleeing_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_witness_pc_critical_msg(int64_t npc_obj, int64_t pc_obj, int type, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_gamble_msg(int64_t npc_obj, int64_t pc_obj, int type, char* buffer);
void dialog_copy_npc_warning_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_newspaper_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_warning_follow_msg(int64_t npc_obj, int64_t pc_obj, int reason, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_upset_attacking_msg(int64_t npc_obj, int64_t pc_obj, int reason, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_near_death_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_wont_identify_already_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_identify_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_repair_broken_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_repair_undamaged_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_repair_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_check(void);
void dialog_enable_numbers(void);
void dialog_speech_id_unpack(int speech_id, int* script_num_ptr, int* line_ptr);

#endif /* ARCANUM_GAME_DIALOG_H_ */
