// QA1 phase 2, item 4: the shipped dialogue lines whose number is outside
// what the engine allocates (arcanum-work/scripts/quest_domains_allow.tsv),
// run through CE's own `dialog_perform_actions` and `dialog_check_conditions`.
//
// Data defects the original shares, not porting bugs. Each case says what the
// original does with the line; the addresses are `Arcanum.exe`'s. Where the
// original reads outside its table, CE is bounded (`FIX:` in reputation.c and
// rumor.c): a number outside the table contributes nothing.
//
// - `qu10361 ` (01140, {125}) and `qu10961 ` (01481, {181}): state 0 for a
//   quest that does not exist. Nothing is indexed by the number.
// - `rp 2`, `rp -2` (01395, 01396) and `rp -1` (01408): reputations are
//   1000-1999. Adding and removing only search the PC's list, but once 2 is
//   on it, every reaction in the original reads record 2 - 1000 = -998 of
//   the reputation table, 75,848 bytes before it, unbounded. CE keeps 2 on
//   the list and reads no record for it.
// - `ru 1143, rq - 1143` (01906, {114}): `atoi("- 1143")` is 0, so the
//   condition reads rumour 0's quelled bit, byte -125 of the original's bit
//   array. In CE rumour 0 is not quelled.
//
// The tables are pointed at a buffer this file owns, with a planted record or
// byte before (and after) the table, so a read outside it would show without
// the case itself overrunning. The same lines against the engine's own
// allocation run in child processes ("probes"), which must end cleanly: under
// the sanitizer preset an overrun ends the child with ASan's report.
//
// The masked-shift tokens of the same gate (`lc4`, `lc21`, `lf61`, `lf241`)
// are here too, through the dialogue functions, so the sanitizer preset sees
// porting bug #378's masks on that path.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "game/dialog.h"
#include "game/obj.h"
#include "game/quest.h"
#include "game/reaction.h"
#include "game/reputation.h"
#include "game/rumor.h"
#include "game/script.h"

#include "obj_harness.h"
#include "test_harness.h"

// Globalized out of dialog.c, reputation.c and rumor.c by the build
// (tests/CMakeLists.txt).
bool dialog_check_conditions(DialogState* a1, char* a2);
bool dialog_perform_actions(DialogState* a1, char* a2, int a3);
extern void* reputations;
extern uint8_t* rumor_qstate;

// sizeof(ReputationInfo), private to reputation.c: 0x4C in the original's
// `calloc(1000, 0x4C)` (0x4C15D0) and its `19 * 4` stride (0x4C1B6E-0x4C1B79).
// The record pin below fails if CE's differs.
#define REP_RECORD 76
#define REP_UNDER 998
// RUMOR_QSTATE_SIZE: `calloc(0xFB, 1)` (0x4C5500).
#define RUMOR_BYTES 251
#define RUMOR_UNDER 125

static DialogState state;

static bool setup(void)
{
    Script scr;

    if (!obj_harness_init()) {
        return false;
    }

    memset(&state, 0, sizeof(state));
    state.pc_obj = obj_harness_create_proto(OBJ_TYPE_PC);
    state.npc_obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    if (state.pc_obj == OBJ_HANDLE_NULL || state.npc_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    // A dialogue script, so the local flags and counters are live.
    memset(&scr, 0, sizeof(scr));
    scr.num = 1074;
    obj_arrayfield_script_set(state.npc_obj, OBJ_F_SCRIPTS_IDX, SAP_DIALOG, &scr);
    return true;
}

static void teardown(void)
{
    obj_harness_destroy(state.npc_obj);
    obj_harness_destroy(state.pc_obj);
}

static bool act(const char* line)
{
    char buf[256];

    snprintf(buf, sizeof(buf), "%s", line);
    return dialog_perform_actions(&state, buf, 0);
}

static bool cond(const char* line)
{
    char buf[256];

    snprintf(buf, sizeof(buf), "%s", line);
    return dialog_check_conditions(&state, buf);
}

static int rep_count(void)
{
    return obj_arrayfield_length_get(state.pc_obj, OBJ_F_PC_REPUTATION_IDX);
}

static int rep_at(int index)
{
    return (int)obj_arrayfield_uint32_get(state.pc_obj, OBJ_F_PC_REPUTATION_IDX, index);
}

TEST(a_quest_number_past_the_table_changes_nothing)
{
    // Original: `qu` (0x415E9A) calls quest_state_set (0x4C4D20), which reads
    // the PC's quest 10361 through the sparse array (0x407900, bounded by its
    // bitset), gets 0, and returns at 0x4C4D5C because 0 >= the new state 0.
    // The global table (`quest_gstate`, 0x5FF418) and the quest records
    // (0x5FF41C) are not reached. Neither is allocated here, so a touch would
    // fault.
    if (!setup()) {
        CHECK(false);
        return;
    }

    CHECK(act("qu10361 "));
    CHECK(act("qu10961 "));
    CHECK_INT(quest_state_get(state.pc_obj, 10361), 0);
    CHECK_INT(quest_state_get(state.pc_obj, 10961), 0);
    CHECK_INT(obj_arrayfield_length_get(state.pc_obj, OBJ_F_PC_QUEST_IDX), 0);

    teardown();
}

// One record of a reputation table: one effect worth `adj` with no origin or
// faction (matches anyone), in faction `faction`.
static void rep_record_plant(unsigned char* rec_bytes, int adj, int faction)
{
    int* rec = (int*)rec_bytes;

    rec[0] = 1; // num_effects
    rec[1] = faction; // factions[0] (0x4C2242)
    rec[4] = adj; // effects[0].adj, origin and faction 0 (0x4C1B86-0x4C1BA6)
}

// A 1000-record reputation table with REP_UNDER records in front of it and one
// after it. Record -998 (reputation 2) is worth 7 in faction 5, record 1000
// (reputation 2000, past the end) 11 in faction 6, and record 0 (reputation
// 1000, in the table) 3 in faction 4.
static unsigned char* rep_buffer_install(void)
{
    unsigned char* buf = calloc(REP_UNDER + 1000 + 1, REP_RECORD);

    rep_record_plant(buf, 7, 5);
    rep_record_plant(buf + (REP_UNDER + 1000) * REP_RECORD, 11, 6);
    rep_record_plant(buf + REP_UNDER * REP_RECORD, 3, 4);
    reputations = buf + REP_UNDER * REP_RECORD;
    return buf;
}

TEST(reputation_two_lands_on_the_pc_and_reads_no_record)
{
    unsigned char* buf;
    void* saved = reputations;

    if (!setup()) {
        CHECK(false);
        return;
    }
    buf = rep_buffer_install();

    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 0);

    // 01395 {24} (and {23}, a leading space). Original: `rp` (0x416445)
    // calls reputation_add (0x4C1D20), which searches the PC's list
    // (0x4C1CB0) and appends 2 (0x4075B0); no table index. The `re +10` after
    // it reads the reaction, whose modifiers (0x4C1500) call
    // reputation_reaction_adj (0x4C1AC0): record 2 - 1000, `sub eax,0x3e8`
    // at 0x4C1B66, unchecked. CE stores 2 as the original does (saves stay
    // the same) and reads nothing for it.
    CHECK(act("lf0 1,rp 2 , re +10"));
    CHECK_INT(rep_count(), 1);
    CHECK_INT(rep_at(0), 2);
    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 0);
    CHECK_INT(reputation_pick(state.pc_obj, state.npc_obj), 0);

    // {23}, and {17}, {18} without their `$$ -10` (a gold transfer creates a
    // gold object on the map, which the harness has not got): the same
    // reputation, already held, is not added again.
    CHECK(act(" lf0 1,rp 2 , re +10"));
    CHECK(act("lf0 1, rp 2 , re +10"));
    CHECK_INT(rep_count(), 1);

    // The faction test reads the same record in the original (0x4C222C).
    CHECK(!reputation_check_faction(state.pc_obj, 5));

    // Past the end too (record 1000), and a number in the table still reads
    // its record: the control.
    CHECK(act("rp 2000"));
    CHECK_INT(rep_count(), 2);
    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 0);
    CHECK(!reputation_check_faction(state.pc_obj, 6));
    CHECK(act("rp 1000"));
    CHECK_INT(rep_count(), 3);
    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 3);
    CHECK(reputation_check_faction(state.pc_obj, 4));
    CHECK(act("rp -1000, rp -2000"));
    CHECK_INT(rep_count(), 1);

    // 01408 {7} and the rest: `rp -1` removes reputation 1 (0x4C1E10), which
    // no line adds. A search, no index; 2 stays.
    CHECK(act("rp -1 , re -5"));
    CHECK(act("rp -1 , re -10"));
    CHECK_INT(rep_count(), 1);

    // 01395 {4}, {7}; 01396 {3}, {6}: `rp -2` takes 2 off again, and the
    // reaction stops reading the record.
    CHECK(act("re -5 , rp -2"));
    CHECK_INT(rep_count(), 0);
    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 0);

    reputations = saved;
    free(buf);
    teardown();
}

// rumor_known_set stamps the game time, which is 0 here, and 0 reads as not
// known; so the stamp is written directly (rumor_set_known_internal, 0x4C58A0).
static void pc_knows_rumour_1143(void)
{
    obj_arrayfield_int64_set(state.pc_obj, OBJ_F_PC_RUMOR_IDX, 143, 1);
}

TEST(rumour_zero_is_not_quelled_and_reads_nothing)
{
    unsigned char buf[RUMOR_UNDER + RUMOR_BYTES + 1];
    uint8_t* saved = rumor_qstate;
    int i;
    int set = 0;
    int known;

    if (!setup()) {
        CHECK(false);
        return;
    }
    memset(buf, 0, sizeof(buf));
    rumor_qstate = buf + RUMOR_UNDER;

    // 01906 {114}, `ru 1143, rq - 1143`. Original: the condition's value is
    // `atoi` (0x415168, 0x589CAD) of "- 1143", 0; `rq` with 0 >= 0 calls
    // rumor_qstate_get(0) (0x415689), which takes 0 - 1000 = -1000, divides by
    // 8 rounding to zero (0x4C5663-0x4C566B, -125) and reads that byte
    // unchecked (0x4C5674), bit -1000 mod 8 = 0 (0x4C566E-0x4C5680). CE reads
    // no byte for it: rumour 0 is not quelled.
    buf[0] = 0xFF;
    buf[RUMOR_UNDER + RUMOR_BYTES] = 0xFF;
    CHECK(!cond(" ru 1143, rq - 1143"));
    pc_knows_rumour_1143();
    CHECK(cond(" ru 1143"));
    CHECK(!cond(" ru 1143, rq - 1143"));

    // Quelling 1143, the line's own action, does not change what it reads.
    CHECK(act(" rq 1143"));
    CHECK(!cond(" ru 1143, rq - 1143"));

    CHECK(cond(" ru 1143, rq 1143"));

    // Neither the byte before nor the one after the array is read or written:
    // rumour 3008 is the first past `calloc(0xFB, 1)`'s 2,008 bits.
    CHECK(!cond("rq 3008"));
    CHECK(cond("rq -3008"));
    buf[RUMOR_UNDER + RUMOR_BYTES - 1] = 0x80;
    CHECK(cond("rq 3007"));
    memset(buf, 0, sizeof(buf));
    CHECK(act("rq 0"));
    CHECK(act("rq 999"));
    CHECK(act("rq 3008"));
    for (i = 0; i < (int)sizeof(buf); i++) {
        set += buf[i] != 0;
    }
    CHECK_INT(set, 0);
    CHECK(!cond("rq 0"));
    CHECK(!cond("rq 999"));

    // A rumour below 1000 is not learnt: its key would be negative in the
    // PC's sparse array, whose bitset both engines index unchecked below 0.
    known = obj_arrayfield_length_get(state.pc_obj, OBJ_F_PC_RUMOR_IDX);
    CHECK(act("ru 999"));
    CHECK_INT(obj_arrayfield_length_get(state.pc_obj, OBJ_F_PC_RUMOR_IDX), known);
    CHECK(!rumor_known_get(state.pc_obj, 999));
    CHECK(!cond("ru 999"));

    rumor_qstate = saved;
    teardown();
}

TEST(local_flags_and_counters_past_the_width_through_the_dialogue)
{
    Script scr;

    if (!setup()) {
        CHECK(false);
        return;
    }

    // Original: the getters and setters shift unchecked (0x444F48,
    // 0x444F98, 0x444FAD, 0x44500A, 0x44505C, 0x44506B) and x86 takes the
    // count mod 32; CE masks (#378). The tokens are the gate's masked-shift
    // lines': Raven's `lc4` (01074 {545}, {711}), Schuyler's `lc21` (01049
    // {60}), Thorvald's `lf61` (01066 {844}), the mages' `lf61` (01970-01972
    // {95}) and Drake's `lf241` (02098 {333}).
    CHECK(act("lc4 1"));
    CHECK(cond("lc4 1"));
    CHECK(cond("lc0 1"));

    obj_arrayfield_script_get(state.npc_obj, OBJ_F_SCRIPTS_IDX, SAP_DIALOG, &scr);
    scr.hdr.counters = 0x00002200u;
    scr.hdr.flags = 1u << 29 | 1u << 17;
    obj_arrayfield_script_set(state.npc_obj, OBJ_F_SCRIPTS_IDX, SAP_DIALOG, &scr);

    // `lc21` with no value sets counter 21 (counter 1) to 0.
    CHECK(act("lc21"));
    CHECK(cond("lc1 0"));
    // `lf61` reads flag 29, `lf241` flag 17.
    CHECK(!cond("lf61"));
    CHECK(cond("lf241 1"));
    // `lf61` as an action (no value) clears flag 29.
    CHECK(act("lf61"));
    CHECK(cond("lf61"));
    CHECK(cond("lf29 0"));

    teardown();
}

#ifndef _WIN32
// Runs `line` in a child, against the engine's own allocation of the table,
// prints how the child ended and checks that it exited 0. Under the sanitizer
// preset an overrun ends it with ASan's report (in `log`, in the working
// directory) and exit 1.
static void probe(const char* name, void (*body)(void))
{
    char log[64];
    pid_t child;
    int status = 0;

    snprintf(log, sizeof(log), "oor_probe_%s.log", name);
    fflush(NULL);
    child = fork();
    if (child == 0) {
        alarm(10);
        if (freopen(log, "w", stderr) == NULL) {
            _exit(3);
        }
        body();
        fflush(stderr);
        _exit(0);
    }
    if (child < 0 || waitpid(child, &status, 0) != child) {
        printf("  probe %s: did not run\n", name);
        CHECK(false);
        return;
    }
    if (WIFEXITED(status)) {
        printf("  probe %s: exit %d (%s)\n", name, WEXITSTATUS(status), log);
    } else {
        printf("  probe %s: signal %d (%s)\n", name, WIFSIGNALED(status) ? WTERMSIG(status) : 0, log);
    }
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void probe_reputation_body(void)
{
    if (!setup() || !reputation_init(NULL)) {
        _exit(2);
    }
    act("lf0 1,rp 2 , re +10");
    fprintf(stderr, "reputation_reaction_adj %d\n", reputation_reaction_adj(state.pc_obj, state.npc_obj));
    if (reputation_reaction_adj(state.pc_obj, state.npc_obj) != 0) {
        _exit(4);
    }
}

static void probe_rumour_body(void)
{
    if (!setup() || !rumor_init(NULL)) {
        _exit(2);
    }
    pc_knows_rumour_1143();
    fprintf(stderr, "condition %d\n", cond(" ru 1143, rq - 1143"));
    if (cond(" ru 1143, rq - 1143")) {
        _exit(4);
    }
}

TEST(probes_against_the_engines_own_tables)
{
    probe("rp2", probe_reputation_body);
    probe("rq0", probe_rumour_body);
}
#endif

void test_out_of_range_lines_suite(void)
{
    RUN(a_quest_number_past_the_table_changes_nothing);
    RUN(reputation_two_lands_on_the_pc_and_reads_no_record);
    RUN(rumour_zero_is_not_quelled_and_reads_nothing);
    RUN(local_flags_and_counters_past_the_width_through_the_dialogue);
#ifndef _WIN32
    RUN(probes_against_the_engines_own_tables);
#endif
}
