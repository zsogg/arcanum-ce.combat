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
// R5 extends the same fixture through compiled-record validation and the
// dynamic operation boundary. Its tests repeat the private ScriptState layout
// below so local arrays and diagnostic status are reachable. The R1 arithmetic
// cases still pass NULL deliberately: their operand kinds need no state, and
// an accidental new dependency would crash rather than produce a lucky pass.

#include "game/script.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "game/object.h"
#include "test_harness.h"

// script.c:50. Not exported, so it is pinned here: if the sentinel changes,
// these tests should say so rather than silently comparing -1 to -1.
#define SCRIPT_NEXT (-1)
#define SCRIPT_INVALID_OPERAND (-5)

// script.c:47 and script.c:48.
#define TEST_MAX_GLOBAL_VARS 2000
#define TEST_MAX_GLOBAL_FLAGS 100

typedef struct ScriptOperandError {
    int script_id;
    const char* script_name;
    const char* instruction;
    const char* branch;
    const char* domain;
    int line;
    int slot;
    int value;
    int minimum;
    int maximum;
} ScriptOperandError;

// Private VM state, repeated here because the R5 tests execute dynamic
// operand checks through the real action handler. Any layout drift is caught
// by the behavior and sanitizer runs rather than hidden behind a helper that
// bypasses the operation boundary.
typedef struct ScriptState {
    ScriptInvocation* invocation;
    int field_4;
    int loop_cnt;
    int field_C;
    int script_num;
    int field_14;
    int64_t current_loop_obj;
    int64_t objs[100];
    ObjectList objects;
    int field_398;
    int lc_vars[10];
    int field_3C4;
    int64_t lc_objs[10];
    bool invalid_operand;
    ScriptOperandError operand_error;
} ScriptState;

extern int script_execute_action(ScriptAction* action, int line, ScriptState* state);
extern int script_execute_condition(ScriptCondition* condition, int line, ScriptState* state);
extern bool script_file_validate(ScriptFile* script_file, int script_id, ScriptOperandError* error);
extern bool script_execute_file(ScriptInvocation* invocation, ScriptFile* script_file,
    ScriptFlags flags, ScriptState* state);
extern bool (*script_nested_execute_func)(ScriptInvocation* invocation);

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
static ScriptFile* nested_test_file;
static int nested_test_calls;
static bool nested_test_invalid;
static int nested_test_script_id;
static const char* nested_test_script_name;

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

static void script_state_begin(ScriptState* state,
    ScriptInvocation* invocation,
    Script* script,
    int script_num)
{
    memset(state, 0, sizeof(*state));
    memset(invocation, 0, sizeof(*invocation));
    memset(script, 0, sizeof(*script));
    script->num = script_num;
    invocation->script = script;
    state->invocation = invocation;
    state->script_num = -1;
}

static bool validate_one(ScriptCondition* condition, ScriptOperandError* error)
{
    ScriptFile file;
    memset(&file, 0, sizeof(file));
    file.num_entries = 1;
    file.max_entries = 1;
    file.entries = condition;
    memcpy(file.description, "r5-boundary", sizeof("r5-boundary"));
    return script_file_validate(&file, 4242, error);
}

static void assign_action(ScriptCondition* condition, int value_type, int value)
{
    memset(condition, 0, sizeof(*condition));
    condition->type = SCT_TRUE;
    condition->action.type = SAT_ASSIGN_NUM;
    condition->action.op_type[0] = value_type;
    condition->action.op_value[0] = value;
    condition->action.op_type[1] = SVT_NUMBER;
    condition->action.op_value[1] = 7;
    condition->els.type = SAT_DO_NOTHING;
}

static bool execute_nested_test_file(ScriptInvocation* invocation)
{
    ScriptOperandError error;
    ScriptState state;
    bool result;

    nested_test_calls++;
    memset(&state, 0, sizeof(state));
    state.invocation = invocation;
    state.script_num = -1;
    CHECK(script_file_validate(nested_test_file, invocation->script->num, &error));
    result = script_execute_file(invocation, nested_test_file,
        nested_test_file->flags, &state);
    nested_test_invalid = state.invalid_operand;
    nested_test_script_id = state.operand_error.script_id;
    nested_test_script_name = state.operand_error.script_name;
    return result;
}

// --------------------------------------------------------- R5 operand safety

TEST(compiled_records_validate_each_fixed_operand_domain)
{
    static const struct {
        int type;
        int minimum;
        int maximum;
        const char* domain;
    } cases[] = {
        { SVT_COUNTER, 0, 3, "counter index" },
        { SVT_GL_VAR, 0, TEST_MAX_GLOBAL_VARS - 1, "global variable index" },
        { SVT_LC_VAR, 0, 9, "local variable index" },
        { SVT_GL_FLAG, 0, TEST_MAX_GLOBAL_FLAGS * 32 - 1, "global flag index" },
    };
    ScriptCondition condition;
    ScriptOperandError error;

    for (unsigned int index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        assign_action(&condition, cases[index].type, cases[index].minimum);
        CHECK(validate_one(&condition, &error));
        condition.action.op_value[0] = cases[index].maximum;
        CHECK(validate_one(&condition, &error));

        condition.action.op_value[0] = cases[index].minimum - 1;
        CHECK(!validate_one(&condition, &error));
        CHECK_STR(error.domain, cases[index].domain);
        CHECK_INT(error.value, cases[index].minimum - 1);

        condition.action.op_value[0] = cases[index].maximum + 1;
        CHECK(!validate_one(&condition, &error));
        CHECK_STR(error.domain, cases[index].domain);
        CHECK_INT(error.value, cases[index].maximum + 1);
    }

    // A number is a literal, not an index. The validator must not impose an
    // invented generic integer range on it.
    assign_action(&condition, SVT_NUMBER, INT_MIN);
    CHECK(validate_one(&condition, &error));
    condition.action.op_value[0] = INT_MAX;
    CHECK(validate_one(&condition, &error));
}

TEST(compiled_records_validate_focus_opcode_and_embedded_selector_domains)
{
    ScriptCondition condition;
    ScriptOperandError error;

    memset(&condition, 0, sizeof(condition));
    condition.type = SCT_HAS_GOLD;
    condition.op_type[0] = SFO_LOCAL_OBJECT;
    condition.op_value[0] = 9;
    condition.op_type[1] = SVT_NUMBER;
    condition.action.type = SAT_DO_NOTHING;
    condition.els.type = SAT_DO_NOTHING;
    CHECK(validate_one(&condition, &error));
    condition.op_value[0] = -1;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "local object index");
    condition.op_value[0] = 10;
    CHECK(!validate_one(&condition, &error));

    condition.op_type[0] = SFO_TRIGGERER;
    condition.op_value[0] = INT_MAX; // unread for this selector
    CHECK(validate_one(&condition, &error));
    condition.op_type[0] = 255;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "focus object type");

    memset(&condition, 0, sizeof(condition));
    condition.type = SCT_EQ;
    condition.op_type[0] = SVT_PC_VAR;
    condition.op_value[0] = 0x7FFF0001;
    condition.op_type[1] = SVT_NUMBER;
    condition.action.type = SAT_DO_NOTHING;
    condition.els.type = SAT_DO_NOTHING;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "embedded focus object type");

    condition.op_type[0] = SVT_NUMBER;
    condition.type = -1;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "opcode");
    condition.type = SCT_WAITING_FOR_LEADER + 1;
    CHECK(!validate_one(&condition, &error));
}

TEST(call_script_in_validates_its_noncontiguous_operands_only)
{
    ScriptCondition condition;
    ScriptOperandError error;

    memset(&condition, 0, sizeof(condition));
    condition.type = SCT_TRUE;
    condition.action.type = SAT_CALL_SCRIPT_IN;
    condition.action.op_type[0] = SVT_NUMBER;
    condition.action.op_type[1] = SVT_NUMBER;
    condition.action.op_type[2] = 255; // serialized garbage; AT alone reads it
    condition.action.op_type[3] = 255;
    condition.action.op_type[4] = SVT_GL_VAR;
    condition.action.op_value[4] = TEST_MAX_GLOBAL_VARS - 1;
    condition.els.type = SAT_DO_NOTHING;
    CHECK(validate_one(&condition, &error));

    condition.action.op_value[4] = TEST_MAX_GLOBAL_VARS;
    CHECK(!validate_one(&condition, &error));
    CHECK_INT(error.slot, 4);
    CHECK_STR(error.instruction, "SAT_CALL_SCRIPT_IN");
}

TEST(compiled_literal_consumer_domains_are_rejected_before_execution)
{
    ScriptCondition condition;
    ScriptOperandError error;

    memset(&condition, 0, sizeof(condition));
    condition.type = SCT_TRUE;
    condition.action.type = SAT_SET_LOCAL_FLAG;
    condition.action.op_type[0] = SVT_NUMBER;
    condition.action.op_value[0] = 32;
    condition.els.type = SAT_DO_NOTHING;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "local flag bit");
    CHECK_INT(error.script_id, 4242);
    CHECK_STR(error.script_name, "r5-boundary");

    condition.action.type = SAT_SET_GLOBAL_FLAG;
    condition.action.op_value[0] = TEST_MAX_GLOBAL_FLAGS * 32;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "global flag index");

    condition.action.type = SAT_CHANGE_SCRIPT;
    condition.action.op_type[0] = SFO_TRIGGERER;
    condition.action.op_type[1] = SVT_NUMBER;
    condition.action.op_value[1] = SAP_CRITICAL_MISS + 1;
    condition.action.op_type[2] = SVT_NUMBER;
    CHECK(!validate_one(&condition, &error));
    CHECK_STR(error.domain, "attachment point");
}

TEST(validated_compiled_record_executes_through_the_condition_handler)
{
    ScriptCondition condition;
    ScriptOperandError error;
    ScriptState state;
    ScriptInvocation invocation;
    Script script;

    script_test_begin();
    script_state_begin(&state, &invocation, &script, 4242);
    assign_action(&condition, SVT_GL_VAR, TEST_MAX_GLOBAL_VARS - 1);
    CHECK(validate_one(&condition, &error));
    CHECK_INT(script_execute_condition(&condition, 0, &state), SCRIPT_NEXT);
    CHECK_INT(test_global_vars[TEST_MAX_GLOBAL_VARS - 1], 7);
    CHECK(!state.invalid_operand);
}

TEST(dynamic_flag_indices_abort_before_mutation)
{
    ScriptAction action;
    ScriptState state;
    ScriptInvocation invocation;
    Script script;

    script_test_begin();
    script_state_begin(&state, &invocation, &script, 4242);
    script.hdr.flags = 0x12345678u;
    memset(&action, 0, sizeof(action));
    action.type = SAT_SET_LOCAL_FLAG;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;

    test_global_vars[0] = -1;
    CHECK_INT(script_execute_action(&action, 17, &state), SCRIPT_INVALID_OPERAND);
    CHECK_INT(script.hdr.flags, 0x12345678u);
    CHECK(state.invalid_operand);
    CHECK_STR(state.operand_error.instruction, "SAT_SET_LOCAL_FLAG");
    CHECK_STR(state.operand_error.domain, "local flag bit");
    CHECK_INT(state.operand_error.line, 17);
    CHECK_INT(state.operand_error.value, -1);

    state.invalid_operand = false;
    test_global_vars[0] = 32;
    CHECK_INT(script_execute_action(&action, 18, &state), SCRIPT_INVALID_OPERAND);
    CHECK_INT(script.hdr.flags, 0x12345678u);

    state.invalid_operand = false;
    script.hdr.flags = 0;
    test_global_vars[0] = 31;
    CHECK_INT(script_execute_action(&action, 19, &state), SCRIPT_NEXT);
    CHECK_INT(script.hdr.flags, 0x80000000u);
    CHECK(!state.invalid_operand);

    memset(&action, 0, sizeof(action));
    action.type = SAT_SET_GLOBAL_FLAG;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;
    test_global_vars[0] = TEST_MAX_GLOBAL_FLAGS * 32;
    CHECK_INT(script_execute_action(&action, 20, &state), SCRIPT_INVALID_OPERAND);
    CHECK_INT(test_global_flags[TEST_MAX_GLOBAL_FLAGS - 1], 0);

    state.invalid_operand = false;
    test_global_vars[0] = TEST_MAX_GLOBAL_FLAGS * 32 - 1;
    CHECK_INT(script_execute_action(&action, 21, &state), SCRIPT_NEXT);
    CHECK_INT((unsigned int)test_global_flags[TEST_MAX_GLOBAL_FLAGS - 1], 0x80000000u);
}

TEST(repeated_invalid_invocations_do_not_mutate_or_accumulate_state)
{
    ScriptAction action;
    ScriptState state;
    ScriptInvocation invocation;
    Script script;

    script_test_begin();
    script_state_begin(&state, &invocation, &script, 4242);
    script.hdr.flags = 0x55AAu;
    memset(&action, 0, sizeof(action));
    action.type = SAT_SET_LOCAL_FLAG;
    action.op_type[0] = SVT_GL_VAR;
    action.op_value[0] = 0;
    test_global_vars[0] = 32;

    for (int iteration = 0; iteration < 2000; iteration++) {
        state.invalid_operand = false;
        CHECK_INT(script_execute_action(&action, 22, &state), SCRIPT_INVALID_OPERAND);
        CHECK_INT(script.hdr.flags, 0x55AAu);
    }
}

TEST(accepted_file_path_stops_after_dynamic_error_without_rollback)
{
    ScriptCondition entries[3];
    ScriptFile file;
    ScriptOperandError error;
    ScriptState state;
    ScriptInvocation invocation;
    Script script;

    script_test_begin();
    script_state_begin(&state, &invocation, &script, 4242);
    assign_action(&entries[0], SVT_GL_VAR, 0);
    entries[0].action.op_value[1] = 7;

    memset(&entries[1], 0, sizeof(entries[1]));
    entries[1].type = SCT_TRUE;
    entries[1].action.type = SAT_SET_LOCAL_FLAG;
    entries[1].action.op_type[0] = SVT_GL_VAR;
    entries[1].action.op_value[0] = 1;
    entries[1].els.type = SAT_DO_NOTHING;

    assign_action(&entries[2], SVT_GL_VAR, 2);
    entries[2].action.op_value[1] = 9;

    memset(&file, 0, sizeof(file));
    file.num_entries = 3;
    file.max_entries = 3;
    file.entries = entries;
    memcpy(file.description, "r5-full-path", sizeof("r5-full-path"));
    CHECK(script_file_validate(&file, 4242, &error));

    test_global_vars[1] = 32;
    CHECK(!script_execute_file(&invocation, &file, 0, &state));
    CHECK(state.invalid_operand);
    CHECK_INT(state.operand_error.line, 1);
    CHECK_INT(state.operand_error.script_id, 4242);
    CHECK_STR(state.operand_error.script_name, "r5-full-path");
    CHECK_INT(test_global_vars[0], 7);
    CHECK_INT(test_global_vars[2], 0);
    CHECK_INT(script.hdr.flags, 0);
}

TEST(existing_thousand_step_execution_budget_still_bounds_a_valid_loop)
{
    ScriptCondition entries[2];
    ScriptFile file;
    ScriptOperandError error;
    ScriptState state;
    ScriptInvocation invocation;
    Script script;

    script_test_begin();
    script_state_begin(&state, &invocation, &script, 4242);
    memset(entries, 0, sizeof(entries));
    entries[0].type = SCT_TRUE;
    entries[0].action.type = SAT_ADD;
    entries[0].action.op_type[0] = SVT_GL_VAR;
    entries[0].action.op_value[0] = 0;
    entries[0].action.op_type[1] = SVT_GL_VAR;
    entries[0].action.op_value[1] = 0;
    entries[0].action.op_type[2] = SVT_NUMBER;
    entries[0].action.op_value[2] = 1;
    entries[0].els.type = SAT_DO_NOTHING;
    entries[1].type = SCT_TRUE;
    entries[1].action.type = SAT_GOTO;
    entries[1].action.op_type[0] = SVT_NUMBER;
    entries[1].action.op_value[0] = 0;
    entries[1].els.type = SAT_DO_NOTHING;

    memset(&file, 0, sizeof(file));
    file.num_entries = 2;
    file.max_entries = 2;
    file.entries = entries;
    CHECK(script_file_validate(&file, 4242, &error));
    CHECK(!script_execute_file(&invocation, &file, 0, &state));
    CHECK(!state.invalid_operand);
    CHECK_INT(test_global_vars[0], 500);
}

TEST(nested_call_failure_is_bounded_to_the_child_invocation)
{
    ScriptAction action;
    ScriptCondition child_entry;
    ScriptFile child_file;
    ScriptState parent_state;
    ScriptInvocation parent_invocation;
    Script parent_script;

    script_test_begin();
    script_state_begin(&parent_state, &parent_invocation, &parent_script, 4242);
    assign_action(&child_entry, SVT_GL_VAR, 0);
    child_entry.action.type = SAT_SET_LOCAL_FLAG;
    child_entry.action.op_type[0] = SVT_GL_VAR;
    child_entry.action.op_value[0] = 1;
    memset(&child_file, 0, sizeof(child_file));
    child_file.num_entries = 1;
    child_file.max_entries = 1;
    child_file.entries = &child_entry;
    memcpy(child_file.description, "r5-nested-child", sizeof("r5-nested-child"));

    memset(&action, 0, sizeof(action));
    action.type = SAT_CALL_SCRIPT;
    action.op_type[0] = SVT_NUMBER;
    action.op_value[0] = 9001;
    action.op_type[1] = SVT_NUMBER;
    action.op_value[1] = 0;
    action.op_type[2] = SFO_TRIGGERER;
    action.op_type[3] = SFO_ATTACHEE;

    nested_test_file = &child_file;
    nested_test_calls = 0;
    nested_test_invalid = false;
    nested_test_script_id = 0;
    nested_test_script_name = NULL;
    test_global_vars[1] = 32;
    script_nested_execute_func = execute_nested_test_file;
    CHECK_INT(script_execute_action(&action, 5, &parent_state), SCRIPT_NEXT);
    script_nested_execute_func = script_execute;

    CHECK_INT(nested_test_calls, 1);
    CHECK(nested_test_invalid);
    CHECK_INT(nested_test_script_id, 9001);
    CHECK_STR(nested_test_script_name, "r5-nested-child");
    CHECK(!parent_state.invalid_operand);
    CHECK_INT(test_global_vars[0], 0);
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

TEST(add_subtract_and_multiply_wrap_instead_of_being_undefined)
{
    // The original's `add eax,ebx` (0x44745D), `sub edi,eax` (0x447484) and
    // `imul eax,ebx` (0x4474AD) set the overflow flag and nothing reads it, so
    // an overflowing statement stores the wrap and the script continues. C
    // leaves signed overflow undefined, and under the ASan/UBSan preset's
    // `-fno-sanitize-recover=undefined` these four lines terminated the
    // process before the wrap was made explicit. Divide got this treatment for
    // its one edge in porting bug #45; these are the three that kept it.
    CHECK_INT(run_binary(SAT_ADD, INT_MAX, 1), INT_MIN);
    CHECK_INT(run_binary(SAT_SUBTRACT, INT_MIN, 1), INT_MAX);
    CHECK_INT(run_binary(SAT_MULTIPLY, INT_MAX, 2), -2);
    CHECK_INT(run_binary(SAT_MULTIPLY, INT_MIN, -1), INT_MIN);
    // 46341 * 46341 is 2147488281, which is 2^31 + 4633 - the smallest square
    // that overflows, and 4633 past INT_MIN is what `imul` leaves in EAX.
    CHECK_INT(run_binary(SAT_MULTIPLY, 46341, 46341), INT_MIN + 4633);
}

TEST(wrapping_does_not_change_any_ordinary_result)
{
    // The repair must be invisible to a correctly formed script: the three
    // opcodes still answer exactly what they answered before, including for
    // negative operands and at the last value that does not overflow.
    CHECK_INT(run_binary(SAT_ADD, INT_MAX - 1, 1), INT_MAX);
    CHECK_INT(run_binary(SAT_SUBTRACT, INT_MIN + 1, 1), INT_MIN);
    CHECK_INT(run_binary(SAT_ADD, -7, -8), -15);
    CHECK_INT(run_binary(SAT_SUBTRACT, -7, -8), 1);
    CHECK_INT(run_binary(SAT_MULTIPLY, -7, -8), 56);
    CHECK_INT(run_binary(SAT_MULTIPLY, 46340, 46340), 2147395600);
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
    RUN(compiled_records_validate_each_fixed_operand_domain);
    RUN(compiled_records_validate_focus_opcode_and_embedded_selector_domains);
    RUN(call_script_in_validates_its_noncontiguous_operands_only);
    RUN(compiled_literal_consumer_domains_are_rejected_before_execution);
    RUN(validated_compiled_record_executes_through_the_condition_handler);
    RUN(dynamic_flag_indices_abort_before_mutation);
    RUN(repeated_invalid_invocations_do_not_mutate_or_accumulate_state);
    RUN(accepted_file_path_stops_after_dynamic_error_without_rollback);
    RUN(existing_thousand_step_execution_budget_still_bounds_a_valid_loop);
    RUN(nested_call_failure_is_bounded_to_the_child_invocation);
    RUN(divide_stores_the_quotient_not_the_sum);
    RUN(divide_truncates_toward_zero_like_idiv);
    RUN(divide_handles_exact_and_zero_numerators);
    RUN(divide_by_zero_leaves_the_destination_alone);
    RUN(divide_int_min_by_minus_one_is_defined);
    RUN(divide_writes_a_flag_destination_too);
    RUN(add_subtract_and_multiply_are_not_the_same_paste);
    RUN(add_subtract_and_multiply_wrap_instead_of_being_undefined);
    RUN(wrapping_does_not_change_any_ordinary_result);
    RUN(the_four_arithmetic_opcodes_disagree_with_each_other);
    RUN(assign_num_copies_its_source_operand);
    RUN(assign_num_reads_through_a_global_source);
    RUN(goto_returns_its_operand_as_the_next_line);
    RUN(arithmetic_operands_can_be_globals_not_only_literals);
    RUN(the_shipped_level_scaling_script_divides_by_five);
}
