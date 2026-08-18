#ifndef ARCANUM_GAME_MULTIPLAYER_H_
#define ARCANUM_GAME_MULTIPLAYER_H_

#include "game/context.h"
#include "game/obj.h"
#include "game/timeevent.h"

#define MULTIPLAYER_ALWAYS_RUN 0x0100u
#define MULTIPLAYER_AUTO_ATTACK 0x0200u
#define MULTIPLAYER_AUTO_SWITCH_WEAPONS 0x0400u
#define MULTIPLAYER_FOLLOWER_SKILLS 0x0800u

typedef void (*Func5F0E08)(void);
typedef bool (*Func5F0DF8)(tig_button_handle_t button_handle);

bool multiplayer_init(GameInitInfo* init_info);
void multiplayer_exit(void);
void multiplayer_reset(void);
bool multiplayer_save(TigFile* stream);
bool mutliplayer_load(GameLoadInfo* load_info);
bool multiplayer_mod_load(void);
void multiplayer_mod_unload(void);
bool multiplayer_start(void);
bool multiplayer_end(void);
void multiplayer_start_server(void);
bool multiplayer_host_game(const char* a1, const char* a2);
bool multiplayer_timeevent_process(TimeEvent* timeevent);
bool multiplayer_map_open_by_name(const char* name);
void multiplayer_player_joined_callback_set(Func5F0E08 func);
int multiplayer_find_slot_from_obj(int64_t obj);
int64_t multiplayer_active_player_obj_get(int player);
bool multiplayer_is_locked(void);
void multiplayer_lock(void);
void multiplayer_unlock(void);
int64_t multiplayer_player_find_first(void);
int64_t multiplayer_player_find_next(void);
void multiplayer_item_pickup_request(ObjectID oid, bool (*success_func)(void*), void* success_info, bool (*failure_func)(void*), void* failure_info);
void multiplayer_empty_hook(void);
int multiplayer_xfer_extra_seconds_get(void);
bool multiplayer_leave_game(bool (*func)(tig_button_handle_t), tig_button_handle_t button_handle);
void multiplayer_leave_callback_set(Func5F0DF8 func, tig_button_handle_t button_handle);
bool multiplayer_free_loaded_chars(bool a1);
bool multiplayer_load_chars(int64_t** objs_ptr, int* cnt_ptr);
void multiplayer_player_data_set(int player, ObjectID oid, int level, void* data, int size);
void* multiplayer_player_data_get(int player);
int multiplayer_player_data_size_get(int player);
ObjectID multiplayer_player_data_oid_get(int player);
void* multiplayer_player_data_entry_get(int player);
int multiplayer_player_data_total_size_get(int player);
void multiplayer_reset_lock(void);
void multiplayer_reset_unlock(void);
bool multiplayer_save_local_player_char(void);
bool multiplayer_notify_level_scheme_changed(int64_t obj);
bool multiplayer_level_scheme_rule(int64_t obj, char* str);
bool multiplayer_level_scheme_name(int64_t obj, char* str);
bool multiplayer_portrait_path(int64_t obj, int size, char* path);
bool multiplayer_item_pickup_complete(int64_t pc_obj, int64_t item_obj);
bool multiplayer_item_reveal(int64_t pc_obj, int64_t item_obj);
int multiplayer_substitute_inventory_count(int64_t a1);
void multiplayer_ping(tig_timestamp_t timestamp);
void multiplayer_substitute_inventory_set(int64_t a1, int64_t a2);
void multiplayer_flags_set(int client_id, unsigned int flags);
void multiplayer_flags_unset(int client_id, unsigned int flags);
unsigned int multiplayer_flags_get(int client_id);
int multiplayer_max_players_get(void);
void multiplayer_max_players_set(int a1);
void multiplayer_gameplay_settings_apply(void);
bool multiplayer_combat_action_disallowed(int64_t a1, int64_t a2, int64_t a3, int64_t a4);
bool multiplayer_export_char(int64_t pc_obj);

#endif /* ARCANUM_GAME_MULTIPLAYER_H_ */
