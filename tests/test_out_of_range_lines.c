// QA1 phase 2, item 4: the shipped dialogue lines whose number is outside
// what the engine allocates (arcanum-work/scripts/quest_domains_allow.tsv),
// run through CE's own `dialog_perform_actions` and `dialog_check_conditions`.
//
// Data defects the original shares, not porting bugs. Each case says what the
// original does with the line; the addresses are `Arcanum.exe`'s.
//
// - `qu10361 ` (01140, {125}) and `qu10961 ` (01481, {181}): state 0 for a
//   quest that does not exist. Nothing is indexed by the number.
// - `rp 2`, `rp -2` (01395, 01396) and `rp -1` (01408): reputations are
//   1000-1999. Adding and removing only search the PC's list, but once 2 is
//   on it, every reaction reads record 2 - 1000 = -998 of the reputation
//   table, 75,848 bytes before it, with no bound in either engine.
// - `ru 1143, rq - 1143` (01906, {114}): `atoi("- 1143")` is 0, so the
//   condition reads rumour 0's quelled bit, byte -125 of the bit array.
//
// The table reads are pinned against a buffer this file owns, laid out so the
// out-of-range record is inside it: that pins which byte CE reads without the
// case itself overrunning. The same lines against the engine's own allocation
// run in child processes ("probes"), which record what happens and check
// nothing: the outcome depends on what the heap holds there.
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

// A reputation table with REP_UNDER records in front of it, the first of them
// carrying one effect worth `adj` with no origin or faction (matches anyone).
static unsigned char* rep_buffer_install(int adj)
{
    unsigned char* buf = calloc(REP_UNDER + 1000, REP_RECORD);
    int* rec = (int*)buf;

    rec[0] = 1; // num_effects
    rec[4] = adj; // effects[0].adj, origin and faction 0 (0x4C1B86-0x4C1BA6)
    reputations = buf + REP_UNDER * REP_RECORD;
    return buf;
}

TEST(reputation_two_lands_on_the_pc_and_reads_record_minus_998)
{
    unsigned char* buf;
    void* saved = reputations;

    if (!setup()) {
        CHECK(false);
        return;
    }
    buf = rep_buffer_install(7);

    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 0);

    // 01395 {24} (and {23}, a leading space). Original: `rp` (0x416445)
    // calls reputation_add (0x4C1D20), which searches the PC's list
    // (0x4C1CB0) and appends 2 (0x4075B0); no table index. The `re +10` after
    // it reads the reaction, whose modifiers (0x4C1500) call
    // reputation_reaction_adj (0x4C1AC0): record 2 - 1000, `sub eax,0x3e8`
    // at 0x4C1B66, unchecked.
    CHECK(act("lf0 1,rp 2 , re +10"));
    CHECK_INT(rep_count(), 1);
    CHECK_INT(rep_at(0), 2);
    CHECK_INT(reputation_reaction_adj(state.pc_obj, state.npc_obj), 7);

    // {23}, and {17}, {18} without their `$$ -10` (a gold transfer creates a
    // gold object on the map, which the harness has not got): the same
    // reputation, already held, is not added again.
    CHECK(act(" lf0 1,rp 2 , re +10"));
    CHECK(act("lf0 1, rp 2 , re +10"));
    CHECK_INT(rep_count(), 1);

    // The faction test reads the same record (0x4C222C), factions at +4.
    CHECK(!reputation_check_faction(state.pc_obj, 5));
    ((int*)buf)[1] = 5;
    CHECK(reputation_check_faction(state.pc_obj, 5));

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

TEST(rumour_zero_reads_the_byte_125_before_the_bit_array)
{
    unsigned char buf[RUMOR_UNDER + RUMOR_BYTES];
    uint8_t* saved = rumor_qstate;

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
    // unchecked (0x4C5674), bit -1000 mod 8 = 0 (0x4C566E-0x4C5680).
    CHECK(!cond(" ru 1143, rq - 1143"));
    pc_knows_rumour_1143();
    CHECK(cond(" ru 1143"));
    CHECK(!cond(" ru 1143, rq - 1143"));

    // Quelling 1143, the line's own action, does not change what it reads.
    CHECK(act(" rq 1143"));
    CHECK(!cond(" ru 1143, rq - 1143"));

    // The byte it does read.
    buf[0] = 0x01;
    CHECK(cond(" ru 1143, rq - 1143"));
    buf[0] = 0xFE;
    CHECK(!cond(" ru 1143, rq - 1143"));

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
// and prints how the child ended. Under the sanitizer preset an overrun ends it
// with ASan's report (in `log`, in the working directory) and exit 1. Checks
// nothing: what lies before the allocation is the heap's business.
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
        return;
    }
    if (WIFEXITED(status)) {
        printf("  probe %s: exit %d (%s)\n", name, WEXITSTATUS(status), log);
    } else {
        printf("  probe %s: signal %d (%s)\n", name, WIFSIGNALED(status) ? WTERMSIG(status) : 0, log);
    }
}

static void probe_reputation_body(void)
{
    if (!setup() || !reputation_init(NULL)) {
        _exit(2);
    }
    act("lf0 1,rp 2 , re +10");
    fprintf(stderr, "reputation_reaction_adj %d\n", reputation_reaction_adj(state.pc_obj, state.npc_obj));
}

static void probe_rumour_body(void)
{
    if (!setup() || !rumor_init(NULL)) {
        _exit(2);
    }
    pc_knows_rumour_1143();
    fprintf(stderr, "condition %d\n", cond(" ru 1143, rq - 1143"));
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
    RUN(reputation_two_lands_on_the_pc_and_reads_record_minus_998);
    RUN(rumour_zero_reads_the_byte_125_before_the_bit_array);
    RUN(local_flags_and_counters_past_the_width_through_the_dialogue);
#ifndef _WIN32
    RUN(probes_against_the_engines_own_tables);
#endif
}
