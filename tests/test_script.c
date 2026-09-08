// Script VM arithmetic opcodes, executed through the real action handler.
//
// Card R1 / review finding F1: `SAT_DIVIDE` stored `value1 + value2`. It is a
// copy of `SAT_ADD`'s body with a zero guard bolted on, which is what a
// copy/paste error looks like once you have both in front of you - so these
// tests cover the neighbouring arithmetic opcodes too, on the theory that the
// paste could have landed twice.
//
// The original settles every question here, at `0x4474CE`
// (`arcanum-work/audits/review-20260907/original-division.txt`):
//
//     test ecx,ecx ; je 0x44adb0    <- divisor 0: jump away, destination
//                                      is never written
//     mov eax,ebx ; cdq ; idiv ecx  <- signed division, truncating toward
//                                      zero, exactly C's `/`
//
// So this is a transcription error and not a gameplay decision, and fixing it
// *restores* `script.c`'s Faithful regime rather than departing from it.
//
// Reaching the handler: `script_execute_action` is `static`, and the build
// globalizes it into a private copy of the object rather than changing the
// source - the same objcopy technique already used for `dialog.c` and
// `anim.c`. See `tests/CMakeLists.txt`.
//
// What these tests do NOT cover, stated rather than implied: `ScriptState` is
// file-local to `script.c`, so the destination kinds that live in that struct
// - SVT_LC_VAR, SVT_COUNTER, SVT_PC_VAR, SVT_PC_FLAG - are not reachable from
// here. Destinations are exercised as SVT_GL_VAR and SVT_GL_FLAG, which route
// through `script_global_var_set` / `script_global_flag_set` and never touch
// `state`. Every case below therefore passes NULL for `state`, and would
// crash rather than mislead if some opcode started reading it.

#include "game/script.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "test_harness.h"

// script.c:50. Not exported, so it is pinned here: if the sentinel changes,
// these tests should say so rather than silently comparing -1 to -1.
#define SCRIPT_NEXT (-1)

// script.c:47 and script.c:48.
#define TEST_MAX_GLOBAL_VARS 2000
#define TEST_MAX_GLOBAL_FLAGS 100

// Opaque on purpose - see the header comment. Only ever passed as NULL.
typedef struct ScriptState ScriptState;

extern int script_execute_action(ScriptAction* action, int line, ScriptState* state);

// `static int* script_global_vars` in script.c, globalized alongside the
// handler so a test can supply storage without running `script_init`, which
// would drag in file I/O and the rest of game startup.
extern int* script_global_vars;

// script.c:127, and a separate allocation from the vars - a NULL here is a
// segfault in `script_global_flag_get`, not a wrong answer, which is how this
// one announced itself.
extern int* script_global_flags;

static int test_global_vars[TEST_MAX_GLOBAL_VARS];
static int test_global_flags[TEST_MAX_GLOBAL_FLAGS];

// A value no case below computes, so "the destination was not written" is
// distinguishable from "the destination was written with the right answer".
#define UNWRITTEN 0x5EED

static void script_test_begin(void)
{
    memset(test_global_vars, 0, sizeof(test_global_vars));
    memset(test_global_flags, 0, sizeof(test_global_flags));
    script_global_vars = test_global_vars;
    script_global_flags = test_global_flags;
}

// Builds `dest = op1 <type> op2` with both operands as literals. SVT_NUMBER
// returns the operand value itself, so no VM state is involved.
static int run_binary(int type, int value1, int value2)
{
    ScriptAction action;
    int rc;

    script_test_begin();
    test_global_vars[0] = UNWRITTEN;

    memset(&action, 0, sizeof(action));
    action.type = type;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;
    action.op_type[1] = SVT_NUMBER;
    action.op_value[1] = value1;
    action.op_type[2] = SVT_NUMBER;
    action.op_value[2] = value2;

    rc = script_execute_action(&action, 0, NULL);
    CHECK_INT(rc, SCRIPT_NEXT);

    return test_global_vars[0];
}

// ---------------------------------------------------------------- division

TEST(divide_stores_the_quotient_not_the_sum)
{
    // The whole finding, in one line: this wrote 15 before the fix.
    CHECK_INT(run_binary(SAT_DIVIDE, 12, 3), 4);
}

TEST(divide_truncates_toward_zero_like_idiv)
{
    // `idiv` truncates toward zero, and so does C's `/`. A quotient rounded
    // the other way for negatives would be a different bug wearing the same
    // answer for the positive cases.
    CHECK_INT(run_binary(SAT_DIVIDE, 7, 2), 3);
    CHECK_INT(run_binary(SAT_DIVIDE, -7, 2), -3);
    CHECK_INT(run_binary(SAT_DIVIDE, 7, -2), -3);
    CHECK_INT(run_binary(SAT_DIVIDE, -7, -2), 3);
}

TEST(divide_handles_exact_and_zero_numerators)
{
    CHECK_INT(run_binary(SAT_DIVIDE, 100, 10), 10);
    CHECK_INT(run_binary(SAT_DIVIDE, 0, 5), 0);
    CHECK_INT(run_binary(SAT_DIVIDE, -0, 5), 0);
    CHECK_INT(run_binary(SAT_DIVIDE, 5, 1), 5);
    CHECK_INT(run_binary(SAT_DIVIDE, 3, 5), 0);
}

TEST(divide_by_zero_leaves_the_destination_alone)
{
    // `test ecx,ecx ; je` at 0x4474F6. The original does not write, does not
    // zero the destination, and does not fault - it skips to the next
    // statement. A script relying on that keeps its old value.
    CHECK_INT(run_binary(SAT_DIVIDE, 12, 0), UNWRITTEN);
    CHECK_INT(run_binary(SAT_DIVIDE, 0, 0), UNWRITTEN);
    CHECK_INT(run_binary(SAT_DIVIDE, -1, 0), UNWRITTEN);
}

TEST(divide_int_min_by_minus_one_is_defined)
{
    // The one case with no faithful answer to copy: x86 `idiv` raises #DE
    // here, so the original traps. CE must not invoke C undefined behaviour
    // to reproduce a crash, so it defines the result as INT_MIN - the
    // two's-complement wrap the quotient cannot represent - and keeps
    // running. Recorded in PORTING-BUGS.md; see MODDING-NOTES quirk on
    // script arithmetic.
    CHECK_INT(run_binary(SAT_DIVIDE, INT_MIN, -1), INT_MIN);
}

TEST(divide_writes_a_flag_destination_too)
{
    // A second destination kind, to show the fix is in the operation and not
    // in one setter. SVT_GL_FLAG stores a single bit.
    ScriptAction action;

    script_test_begin();

    memset(&action, 0, sizeof(action));
    action.type = SAT_DIVIDE;
    action.op_type[0] = SVT_GL_FLAG;
    action.op_value[0] = 3;
    action.op_type[1] = SVT_NUMBER;
    action.op_value[1] = 9;
    action.op_type[2] = SVT_NUMBER;
    action.op_value[2] = 9;

    CHECK_INT(script_execute_action(&action, 0, NULL), SCRIPT_NEXT);
    CHECK_INT(script_global_flag_get(3), 1);
}

// ------------------------------------------------ the neighbouring opcodes

TEST(add_subtract_and_multiply_are_not_the_same_paste)
{
    CHECK_INT(run_binary(SAT_ADD, 12, 3), 15);
    CHECK_INT(run_binary(SAT_ADD, -4, 4), 0);

    CHECK_INT(run_binary(SAT_SUBTRACT, 12, 3), 9);
    CHECK_INT(run_binary(SAT_SUBTRACT, 3, 12), -9);

    CHECK_INT(run_binary(SAT_MULTIPLY, 12, 3), 36);
    CHECK_INT(run_binary(SAT_MULTIPLY, -12, 3), -36);
    CHECK_INT(run_binary(SAT_MULTIPLY, 7, 0), 0);
}

TEST(the_four_arithmetic_opcodes_disagree_with_each_other)
{
    // The property the copy/paste broke: four opcodes over one pair of
    // operands must produce four answers. Chosen so no two coincide.
    int sum = run_binary(SAT_ADD, 12, 3);
    int difference = run_binary(SAT_SUBTRACT, 12, 3);
    int product = run_binary(SAT_MULTIPLY, 12, 3);
    int quotient = run_binary(SAT_DIVIDE, 12, 3);

    CHECK(sum != difference);
    CHECK(sum != product);
    CHECK(sum != quotient);
    CHECK(difference != product);
    CHECK(difference != quotient);
    CHECK(product != quotient);
}

TEST(assign_num_copies_its_source_operand)
{
    ScriptAction action;

    script_test_begin();
    test_global_vars[0] = UNWRITTEN;

    memset(&action, 0, sizeof(action));
    action.type = SAT_ASSIGN_NUM;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;
    action.op_type[1] = SVT_NUMBER;
    action.op_value[1] = 42;

    CHECK_INT(script_execute_action(&action, 0, NULL), SCRIPT_NEXT);
    CHECK_INT(test_global_vars[0], 42);
}

TEST(assign_num_reads_through_a_global_source)
{
    // Not a literal this time: the source operand is itself a global, so the
    // getter is exercised as well as the setter.
    ScriptAction action;

    script_test_begin();
    test_global_vars[7] = 1234;

    memset(&action, 0, sizeof(action));
    action.type = SAT_ASSIGN_NUM;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;
    action.op_type[1] = SVT_GL_VAR;
    action.op_value[1] = 7;

    CHECK_INT(script_execute_action(&action, 0, NULL), SCRIPT_NEXT);
    CHECK_INT(test_global_vars[0], 1234);
}

TEST(goto_returns_its_operand_as_the_next_line)
{
    // The jump opcode: control flow rather than a store, so it is the one
    // neighbour whose result is the return value.
    ScriptAction action;

    script_test_begin();

    memset(&action, 0, sizeof(action));
    action.type = SAT_GOTO;
    action.op_type[0] = SVT_NUMBER;
    action.op_value[0] = 17;

    CHECK_INT(script_execute_action(&action, 0, NULL), 17);

    // And through a global, so a jump destination can be computed.
    test_global_vars[2] = 23;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 2;
    CHECK_INT(script_execute_action(&action, 0, NULL), 23);
}

TEST(arithmetic_operands_can_be_globals_not_only_literals)
{
    // The shipped scripts compute on globals; literal-only coverage would
    // leave the getter path untested for the case that actually ships.
    ScriptAction action;

    script_test_begin();
    test_global_vars[10] = 84;
    test_global_vars[11] = 4;
    test_global_vars[0] = UNWRITTEN;

    memset(&action, 0, sizeof(action));
    action.type = SAT_DIVIDE;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;
    action.op_type[1] = SVT_GL_VAR;
    action.op_value[1] = 10;
    action.op_type[2] = SVT_GL_VAR;
    action.op_value[2] = 11;

    CHECK_INT(script_execute_action(&action, 0, NULL), SCRIPT_NEXT);
    CHECK_INT(test_global_vars[0], 21);
}

// ------------------------------------------------------- the shipped reach

TEST(the_shipped_level_scaling_script_divides_by_five)
{
    // Corpus presence is not reach, so this is one real site rather than a
    // count. `scr/60001adjust_level_to_player.scr` scales an NPC to the
    // player, and it is the script that makes level-scaled encounters scale:
    //
    //   [ 4] SAT_GET_STAT(number:17, player,   counter:0)   <- player level
    //   [ 5] SAT_GET_STAT(number:17, attachee, counter:1)   <- NPC level
    //   [ 7] if SCT_LE(counter:0, counter:1) -> return       <- no scaling down
    //   [ 8] SAT_DIVIDE(counter:1, counter:1, number:5)
    //   [ 9] SAT_DIVIDE(lc_var:1,  counter:0, number:5)
    //
    // Both divisions sit under SCT_TRUE, so they run whenever the script
    // runs - there is no branch to be lucky about. Before the fix line 8 gave
    // `level + 5` where the script wanted `level / 5`, so a level 20 NPC
    // scaled as though the fifth of its level were 25.
    //
    // The shipped operands write a *counter*, which this suite cannot reach
    // (see the header comment); the arithmetic below is the same handler and
    // the same operand values, differing only in the destination setter.
    CHECK_INT(run_binary(SAT_DIVIDE, 20, 5), 4);
    CHECK_INT(run_binary(SAT_DIVIDE, 12, 5), 2);
    CHECK_INT(run_binary(SAT_DIVIDE, 5, 5), 1);
    CHECK_INT(run_binary(SAT_DIVIDE, 4, 5), 0);
    CHECK_INT(run_binary(SAT_DIVIDE, 1, 5), 0);

    // The item timers - eight of the fifteen files - all divide by 3 from a
    // local variable, e.g. `scr/02679venom_robes_timer.scr` line 7:
    // SAT_DIVIDE(lc_var:0, lc_var:9, number:3).
    CHECK_INT(run_binary(SAT_DIVIDE, 30, 3), 10);
    CHECK_INT(run_binary(SAT_DIVIDE, 10, 3), 3);
}

void test_script_suite(void)
{
    RUN(divide_stores_the_quotient_not_the_sum);
    RUN(divide_truncates_toward_zero_like_idiv);
    RUN(divide_handles_exact_and_zero_numerators);
    RUN(divide_by_zero_leaves_the_destination_alone);
    RUN(divide_int_min_by_minus_one_is_defined);
    RUN(divide_writes_a_flag_destination_too);
    RUN(add_subtract_and_multiply_are_not_the_same_paste);
    RUN(the_four_arithmetic_opcodes_disagree_with_each_other);
    RUN(assign_num_copies_its_source_operand);
    RUN(assign_num_reads_through_a_global_source);
    RUN(goto_returns_its_operand_as_the_next_line);
    RUN(arithmetic_operands_can_be_globals_not_only_literals);
    RUN(the_shipped_level_scaling_script_divides_by_five);
}
