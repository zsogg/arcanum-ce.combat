// Tests for `dialog_collect_pc_options` — how the engine decides which player
// options a dialogue node offers, and the CE parent link added 2026-08-30.
//
// **Why this suite exists at all.** The parent link's whole point is a second
// collection pass, so shipping it with that pass untested would be shipping the
// interesting half on faith. `dialog_collect_pc_options` is `static`, so
// `tests/CMakeLists.txt` globalizes it with `objcopy` — the same trick the
// emudiff oracle already uses for `art.c`, and for the same reason: it needs no
// source change and no runtime behaviour change.
//
// **What the engine does, which the cases below pin.** Entries are sorted by
// line number at load (`dialog_file_load` qsorts them), so a node's options are
// the entries that follow it *numerically*, up to the next NPC line — an entry
// whose minimum-intelligence field is blank. At most five options are collected,
// and each must pass a gender test, an intelligence test and its conditions.
//
// **What CE adds.** A second pass over entries carrying an `@N` marker in their
// conditions field, naming the node they belong to. It exists because adjacency
// is a hard constraint: 48% of the 15,444 shipped nodes have no free number
// before the next node, so without it a mod cannot add a player option to half
// the game's dialogue however it is authored.

#include <stdio.h>
#include <string.h>

#include "game/dialog.h"
#include "game/obj.h"
#include "game/stat.h"

#include "obj_harness.h"
#include "test_harness.h"

// Globalized out of dialog.c by the build; not in any header, because it is
// `static` in the shipped engine and this suite does not change that.
int dialog_collect_pc_options(DialogState* state, int* out);
bool dialog_build_pc_option(int entry_num, int slot, DialogState* state);

#define DLG_DIR "./arcanum_test_dialog"

// A FRESH PATH PER FIXTURE, and it is not tidiness.
//
// `dialog_load` does not reload a file it already holds: `find_dialog` looks the
// path up in `dialog_files` and, on a hit, bumps the refcount and hands back the
// existing parse. `dialog_unload` only decrements that refcount - it does not
// free the entries. So a suite that wrote different content to one path would
// silently test its FIRST fixture over and over. Measured: seven of these ten
// cases failed that way before the counter was added, including one that
// contains no parent markers at all, which is what gave the game away.
static int dlg_seq;
static char dlg_path[128];

static void dlg_write(const char* body)
{
    FILE* stream;

    tig_file_mkdir(DLG_DIR);
    snprintf(dlg_path, sizeof(dlg_path), DLG_DIR "/%05dtest.dlg", ++dlg_seq);
    stream = fopen(dlg_path, "wb");
    if (stream != NULL) {
        fwrite(body, 1, strlen(body), stream);
        fclose(stream);
    }
}

static void dlg_remove(void)
{
    remove(dlg_path);
}

// Collect the options a node offers, into `out`. Returns the count.
//
// The PC object is a real critter from the object harness, because the engine
// reads STAT_GENDER and STAT_INTELLIGENCE off it and `critter_is_dumb` looks at
// its stats too - a fake would be testing the fake.
// The first collected option's text, as `dialog_substitute_names` left it.
static char first_option[1000];

static int collect(const char* body, int node, int* out)
{
    DialogState state;
    int dlg = -1;
    int count;

    dlg_write(body);
    if (!dialog_load(dlg_path, &dlg)) {
        return -1;
    }

    memset(&state, 0, sizeof(state));
    state.dlg = dlg;
    state.status_num = node;
    state.pc_obj = obj_harness_create_proto(OBJ_TYPE_PC);

    count = dialog_collect_pc_options(&state, out);
    // The collector stores entry numbers; the text is built one slot at a
    // time by `dialog_build_pc_option`, which is where `@...@` substitution
    // happens.
    first_option[0] = '\0';
    if (count > 0) {
        dialog_build_pc_option(out[0], 0, &state);
        strcpy(first_option, state.options[0]);
    }

    obj_harness_destroy(state.pc_obj);
    dialog_unload(dlg);
    dlg_remove();
    return count;
}

// A node with two adjacent options, then a second node. Intelligence 1 so every
// option qualifies on the IQ test.
#define BODY_ADJACENT                \
    "{100}{Well?}{}{}{}{0}{}\r\n"    \
    "{101}{First.}{}{1}{}{0}{}\r\n"  \
    "{102}{Second.}{}{1}{}{0}{}\r\n" \
    "{200}{Another node.}{}{}{}{0}{}\r\n"

TEST(adjacent_options_are_collected_in_order)
{
    int out[8];
    int count = collect(BODY_ADJACENT, 100, out);

    CHECK_INT(count, 2);
    if (count == 2) {
        CHECK_INT(out[0], 101);
        CHECK_INT(out[1], 102);
    }
}

// The block ends at the next NPC line - an entry with a blank intelligence
// field. Without that the walk would run to the end of the file.
TEST(the_next_npc_line_ends_the_block)
{
    int out[8];
    int count = collect(BODY_ADJACENT, 200, out);

    // Node 200 is last and has no options of its own.
    CHECK_INT(count, 0);
}

// **The parent link.** The option is numbered far away - nowhere near node 100 -
// and names its node with `@100` instead.
TEST(a_parent_linked_option_is_collected_though_it_is_not_adjacent)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n"
        "{20000}{Added by a mod.}{}{1}{@100}{0}{}\r\n",
        100, out);

    CHECK_INT(count, 1);
    if (count == 1) {
        CHECK_INT(out[0], 20000);
    }
}

// Adjacent options keep Troika's numbers and come first; the linked one fills a
// remaining slot. A mod can add to a node, never displace what was there.
TEST(adjacent_options_come_before_parent_linked_ones)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{101}{First.}{}{1}{}{0}{}\r\n"
        "{102}{Second.}{}{1}{}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n"
        "{20000}{Added by a mod.}{}{1}{@100}{0}{}\r\n",
        100, out);

    CHECK_INT(count, 3);
    if (count == 3) {
        CHECK_INT(out[0], 101);
        CHECK_INT(out[1], 102);
        CHECK_INT(out[2], 20000);
    }
}

// A marker naming a different node must not leak into this one.
TEST(a_parent_link_only_answers_its_own_node)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n"
        "{20000}{For the other node.}{}{1}{@200}{0}{}\r\n",
        100, out);

    CHECK_INT(count, 0);
}

// The five-slot cap is the engine's, and the parent pass must respect it rather
// than overrun the caller's array. Six linked options, five collected.
TEST(the_five_option_cap_holds_across_both_passes)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{101}{Adjacent one.}{}{1}{}{0}{}\r\n"
        "{102}{Adjacent two.}{}{1}{}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n"
        "{20000}{Mod one.}{}{1}{@100}{0}{}\r\n"
        "{20001}{Mod two.}{}{1}{@100}{0}{}\r\n"
        "{20002}{Mod three.}{}{1}{@100}{0}{}\r\n"
        "{20003}{Mod four.}{}{1}{@100}{0}{}\r\n",
        100, out);

    CHECK_INT(count, 5);
    if (count == 5) {
        // The two Troika wrote, then three of the mod's - never the reverse.
        CHECK_INT(out[0], 101);
        CHECK_INT(out[1], 102);
        CHECK_INT(out[4], 20002);
    }
}

// An NPC line carrying a marker is a mod's mistake, not an instruction. It is
// skipped rather than offered as an option the player could pick.
TEST(a_parent_marker_on_an_npc_line_is_ignored)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n"
        "{20000}{An NPC line with a marker.}{}{}{@100}{0}{}\r\n",
        100, out);

    CHECK_INT(count, 0);
}

// The marker is stripped, so what reaches the condition evaluator is the
// conditions and nothing else. A marker followed by a real condition must have
// that condition still honoured - here one that cannot pass, so the option is
// filtered out rather than offered.
TEST(the_marker_is_stripped_and_the_conditions_still_apply)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{200}{Another node.}{}{}{}{0}{}\r\n"
        // `lf1 0` is a "logic flag 1 is 0" style test; whatever it evaluates to
        // it must be EVALUATED, which a marker left in place would prevent.
        "{20000}{Conditional.}{}{1}{@100 gd1 99}{0}{}\r\n",
        100, out);

    // The condition names an impossible global-flag value, so nothing is
    // offered. What matters is that the marker did not make the string
    // unparseable, which would have produced a collected option instead.
    CHECK_INT(count, 0);
}

// A file with no markers must behave exactly as it did before the change. This
// is the regression half: the first pass was refactored from `return` to
// `break` so a second pass could follow it.
TEST(a_file_without_markers_behaves_as_before)
{
    int out[8];
    int count = collect(
        "{100}{Well?}{}{}{}{0}{}\r\n"
        "{101}{One.}{}{1}{}{0}{}\r\n"
        "{102}{Two.}{}{1}{}{0}{}\r\n"
        "{103}{Three.}{}{1}{}{0}{}\r\n"
        "{200}{Another.}{}{}{}{0}{}\r\n"
        "{201}{Not mine.}{}{1}{}{0}{}\r\n",
        100, out);

    CHECK_INT(count, 3);
    if (count == 3) {
        CHECK_INT(out[2], 103);
    }
}

// A node that is not in the file at all yields nothing, rather than reading past
// the array. The bsearch returning NULL is the guard.
// `dialog_substitute_names` walks `@...@` pairs in an option's text. Until
// 2026-10-22 a line with an odd number of `@` dereferenced the NULL that
// `strchr` returned for the missing closing one (review finding 5).
TEST(a_lone_at_sign_in_an_option_keeps_the_text_and_does_not_crash)
{
    int out[5];

    CHECK_INT(collect("{100}{Well?}{}{}{}{0}{}\r\n"
                      "{101}{Ask @pcname about it}{}{1}{}{0}{}\r\n",
                  100, out),
        1);
    CHECK_STR(first_option, "Ask @pcname about it");
}

TEST(a_paired_unknown_token_is_still_dropped)
{
    int out[5];

    // Faithful: an unrecognised `@word@` contributes nothing.
    CHECK_INT(collect("{100}{Well?}{}{}{}{0}{}\r\n"
                      "{101}{x @foo@ y}{}{1}{}{0}{}\r\n",
                  100, out),
        1);
    CHECK_STR(first_option, "x  y");
}

TEST(an_unknown_node_offers_nothing)
{
    int out[8];
    CHECK_INT(collect(BODY_ADJACENT, 999, out), 0);
}

void test_dialog_options_suite(void)
{
    RUN(adjacent_options_are_collected_in_order);
    RUN(the_next_npc_line_ends_the_block);
    RUN(a_parent_linked_option_is_collected_though_it_is_not_adjacent);
    RUN(adjacent_options_come_before_parent_linked_ones);
    RUN(a_parent_link_only_answers_its_own_node);
    RUN(the_five_option_cap_holds_across_both_passes);
    RUN(a_parent_marker_on_an_npc_line_is_ignored);
    RUN(the_marker_is_stripped_and_the_conditions_still_apply);
    RUN(a_file_without_markers_behaves_as_before);
    RUN(an_unknown_node_offers_nothing);
    RUN(a_lone_at_sign_in_an_option_keeps_the_text_and_does_not_crash);
    RUN(a_paired_unknown_token_is_still_dropped);
}
