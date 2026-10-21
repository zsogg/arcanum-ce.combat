// Porting bug #60: `effect_parse` read the operator character as the number
// for `*N` and `/N`.
//
// The original steps past the operator before `atoi` (0x4E9E6D `inc %eax`,
// shared by both branches). CE called `atoi(tok)` on "*2" and "/2", which is
// 0: every multiplier in rules/effect.mes became x0, and Flow Reaper's
// `resistmagic /2` ({218}) became a division by zero in `effect_adjust_func`.
//
// The suite loads its own effect.mes through the real `effect_init`, gives a
// harness critter the effects directly through its effect arrays (so nothing
// else of `effect_add` runs), and reads the adjusted values back. It restores
// the harness's own fixture on the way out.

#include <stdio.h>
#include <string.h>

#include "game/effect.h"
#include "game/obj.h"
#include "game/resistance.h"
#include "game/stat.h"

#include "obj_harness.h"
#include "test_harness.h"

static bool load_effects(const char* contents)
{
    FILE* stream;
    GameInitInfo init_info;

    stream = fopen("rules/effect.mes", "wb");
    if (stream == NULL) {
        return false;
    }
    fputs(contents, stream);
    fclose(stream);

    memset(&init_info, 0, sizeof(init_info));
    effect_exit();
    return effect_init(&init_info);
}

static void give_effect(int64_t obj, int effect)
{
    int idx = obj_arrayfield_length_get(obj, OBJ_F_CRITTER_EFFECTS_IDX);

    obj_arrayfield_uint32_set(obj, OBJ_F_CRITTER_EFFECTS_IDX, idx, effect);
    obj_arrayfield_uint32_set(obj, OBJ_F_CRITTER_EFFECT_CAUSE_IDX, idx, EFFECT_CAUSE_ITEM);
}

TEST(multiply_and_divide_read_the_number_after_the_operator)
{
    int64_t obj;

    CHECK(obj_harness_init());
    // The shapes rules/effect.mes ships: {214} Serpentine Necklace
    // `poisonrate *2`, {218} Flow Reaper `resistmagic /2`.
    CHECK(load_effects("{50}{poisonrate *2}\n{51}{resistmagic /2}\n{52}{st *3, dx /4}\n"));

    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    give_effect(obj, 50);
    give_effect(obj, 51);
    give_effect(obj, 52);

    CHECK_INT(effect_adjust_stat_level(obj, STAT_POISON_RECOVERY, 10), 20);
    // Before the fix this divided by zero.
    CHECK_INT(effect_adjust_resistance(obj, RESISTANCE_TYPE_MAGIC, 40), 20);
    CHECK_INT(effect_adjust_stat_level(obj, STAT_STRENGTH, 5), 15);
    CHECK_INT(effect_adjust_stat_level(obj, STAT_DEXTERITY, 16), 4);

    obj_harness_destroy(obj);
}

TEST(restore_the_harness_effect_fixture)
{
    CHECK(load_effects("{1}{unused - see obj_harness.c}\n"));
}

void test_effect_parse_suite(void)
{
    RUN(multiply_and_divide_read_the_number_after_the_operator);
    RUN(restore_the_harness_effect_fixture);
}
