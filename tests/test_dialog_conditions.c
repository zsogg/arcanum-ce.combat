// Porting bugs #66, #67, #68 and #70: dialogue tests that read wrong.
//
// - #66 `$$ -N` ("the party holds at most N gold") compared the gold with the
//   still negative N, so it always failed (original: case 10 of 0x4150D0).
// - #67 `le N` compared the NEGATED level (original: STAT_LEVEL into the
//   shared compare at 0x4156E2).
// - #68 `dialog_parse_params` left a NUL where a range's dash was, in the
//   dialog file's cached string (original restores it at 0x417FD9).
// - #70 `script_local_counter_get` returned every higher counter with the one
//   asked for (original masks at 0x44500C), which `lc N V` reads.
// - #380 `fo` read inverted (original: 0x4156A0-0x4156C3, 0x41595D).
// - #381 a code letter with nothing after it: the original fails the condition
//   (0x415152) and returns from the actions before their attack (0x415C50).
//
// The condition cases go through `dialog_collect_pc_options`, which runs every
// option's conditions, so they test the dialogue as the engine reads it. The
// fixture idiom - a fresh path per load - is test_dialog_options.c's, for the
// reason given there: `dialog_load` hands back a cached parse for a path it has
// seen.

#include <stdio.h>
#include <string.h>

#include "game/critter.h"
#include "game/dialog.h"
#include "game/obj.h"
#include "game/script.h"
#include "game/stat.h"

#include "obj_harness.h"
#include "test_harness.h"

// Globalized out of dialog.c by the build (tests/CMakeLists.txt).
int dialog_collect_pc_options(DialogState* state, int* out);
int dialog_parse_params(int* values, char* str);
bool dialog_check_conditions(DialogState* a1, char* a2);
bool dialog_perform_actions(DialogState* a1, char* a2, int a3);

// `co`'s attack, counted instead of made while `attack_calls` is armed (>= 0)
// (tests/CMakeLists.txt wraps it); other suites get the real one.
static int attack_calls = -1;

void __real_ai_attack(int64_t target_obj, int64_t source_obj, int loudness, unsigned int flags);
void __wrap_ai_attack(int64_t target_obj, int64_t source_obj, int loudness, unsigned int flags);

void __wrap_ai_attack(int64_t target_obj, int64_t source_obj, int loudness, unsigned int flags)
{
    if (attack_calls < 0) {
        __real_ai_attack(target_obj, source_obj, loudness, flags);
        return;
    }
    attack_calls++;
}

#define DLG_DIR "./arcanum_test_dialog"

static int cond_seq;
static char cond_path[128];

// Offers one option, {101}, carrying `cond`, under node {100}; returns how many
// options a PC at `level` with no gold is offered (0 or 1).
static int offered(const char* cond, int level)
{
    char body[512];
    FILE* stream;
    DialogState state;
    int out[8];
    int dlg = -1;
    int count;

    snprintf(body, sizeof(body),
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{101}{An option.}{}{1}{%s}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n",
        cond);

    tig_file_mkdir(DLG_DIR);
    snprintf(cond_path, sizeof(cond_path), DLG_DIR "/%05dcond.dlg", ++cond_seq);
    stream = fopen(cond_path, "wb");
    if (stream == NULL) {
        return -1;
    }
    fwrite(body, 1, strlen(body), stream);
    fclose(stream);

    if (!dialog_load(cond_path, &dlg)) {
        remove(cond_path);
        return -1;
    }

    memset(&state, 0, sizeof(state));
    state.dlg = dlg;
    state.status_num = 100;
    state.pc_obj = obj_harness_create_proto(OBJ_TYPE_PC);
    stat_base_set(state.pc_obj, STAT_LEVEL, level);

    count = dialog_collect_pc_options(&state, out);

    obj_harness_destroy(state.pc_obj);
    dialog_unload(dlg);
    remove(cond_path);
    return count;
}

TEST(at_most_n_gold_passes_for_a_party_short_of_money)
{
    // A harness PC carries no gold.
    CHECK_INT(offered("$$ -500", 1), 1);
    CHECK_INT(offered("$$ -0", 1), 1);
    // The positive form, at least N, is the control: it was right before.
    CHECK_INT(offered("$$ 500", 1), 0);
}

TEST(le_reads_the_level_itself)
{
    CHECK_INT(offered("le 5", 5), 1);
    CHECK_INT(offered("le 6", 5), 0);
    CHECK_INT(offered("le -5", 5), 1);
    CHECK_INT(offered("le -4", 5), 0);
}

TEST(a_range_reads_the_same_the_second_time)
{
    // The shape of a shipped rumour line, "R: $0, 1175-1183".
    char str[] = "1175-1183";
    int values[100];

    CHECK_INT(dialog_parse_params(values, str), 9);
    CHECK_STR(str, "1175-1183");
    CHECK_INT(dialog_parse_params(values, str), 9);
    CHECK_INT(values[8], 1183);
}

TEST(a_list_with_a_range_is_left_as_it_was)
{
    char str[] = "1,3-5,9";
    int values[100];

    CHECK_INT(dialog_parse_params(values, str), 5);
    CHECK_STR(str, "1,3-5,9");
}

TEST(a_local_counter_is_one_byte)
{
    int64_t obj;
    Script scr;

    CHECK(obj_harness_init());
    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    // The dice game's state: counter 2 holds a roll, counter 3 the gambling
    // difference plus 20 (01620's entries 2-3).
    memset(&scr, 0, sizeof(scr));
    scr.num = 1620;
    scr.hdr.counters = (23u << 24) | (6u << 16) | (0u << 8) | 1u;
    obj_arrayfield_script_set(obj, OBJ_F_SCRIPTS_IDX, 0, &scr);

    CHECK_INT(script_local_counter_get(obj, 0, 0), 1);
    CHECK_INT(script_local_counter_get(obj, 0, 1), 0);
    CHECK_INT(script_local_counter_get(obj, 0, 2), 6);
    CHECK_INT(script_local_counter_get(obj, 0, 3), 23);

    obj_harness_destroy(obj);
}

static DialogState direct;

static bool direct_setup(void)
{
    if (!obj_harness_init()) {
        return false;
    }
    memset(&direct, 0, sizeof(direct));
    direct.pc_obj = obj_harness_create_proto(OBJ_TYPE_PC);
    direct.npc_obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    return direct.pc_obj != OBJ_HANDLE_NULL && direct.npc_obj != OBJ_HANDLE_NULL;
}

static void direct_teardown(void)
{
    obj_harness_destroy(direct.npc_obj);
    obj_harness_destroy(direct.pc_obj);
}

static bool cond(const char* line)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "%s", line);
    return dialog_check_conditions(&direct, buf);
}

static bool act(const char* line)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "%s", line);
    return dialog_perform_actions(&direct, buf, 0);
}

TEST(fo_one_passes_for_a_follower_of_the_pc)
{
    int64_t other;

    CHECK(direct_setup());
    if (direct.npc_obj == OBJ_HANDLE_NULL) {
        return;
    }
    other = obj_harness_create_proto(OBJ_TYPE_NPC);

    // No leader: `fo 0` passes, `fo 1` fails; any other value passes.
    CHECK(cond("fo 0"));
    CHECK(!cond("fo 1"));
    CHECK(cond("fo 2"));

    // Following the PC: Weldo's {294}/{301} `fo 1` is offered.
    critter_leader_set(direct.npc_obj, direct.pc_obj);
    CHECK(!cond("fo 0"));
    CHECK(cond("fo 1"));
    CHECK(cond("fo 2"));

    // Following someone else reads as not following the PC.
    critter_leader_set(direct.npc_obj, other);
    CHECK(cond("fo 0"));
    CHECK(!cond("fo 1"));

    obj_harness_destroy(other);
    direct_teardown();
}

TEST(a_lone_code_letter_fails_the_condition)
{
    CHECK(direct_setup());
    if (direct.npc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // Controls: a passing code, and trailing separators after it.
    CHECK(cond("fo 0"));
    CHECK(cond("fo 0, "));
    CHECK(cond(" 7 "));
    // One character after the last code letter: the original's 0x415156.
    CHECK(!cond("z"));
    CHECK(!cond("$"));
    CHECK(!cond("fo 0, z"));
    CHECK(!cond("fo 0 x"));

    direct_teardown();
}

TEST(a_lone_code_letter_ends_the_actions_before_the_attack)
{
    CHECK(direct_setup());
    if (direct.npc_obj == OBJ_HANDLE_NULL) {
        return;
    }
    attack_calls = 0;

    // Control: `co` at the end attacks once (0x4166FB-0x416717).
    CHECK(act("co"));
    CHECK_INT(attack_calls, 1);
    CHECK(act("co, "));
    CHECK_INT(attack_calls, 2);
    // A lone letter after it returns through 0x41671F, past the attack.
    CHECK(act("co a"));
    CHECK(act("co, $"));
    CHECK_INT(attack_calls, 2);

    attack_calls = -1;
    direct_teardown();
}

void test_dialog_conditions_suite(void)
{
    RUN(at_most_n_gold_passes_for_a_party_short_of_money);
    RUN(le_reads_the_level_itself);
    RUN(a_range_reads_the_same_the_second_time);
    RUN(a_list_with_a_range_is_left_as_it_was);
    RUN(a_local_counter_is_one_byte);
    RUN(fo_one_passes_for_a_follower_of_the_pc);
    RUN(a_lone_code_letter_fails_the_condition);
    RUN(a_lone_code_letter_ends_the_actions_before_the_attack);
}
