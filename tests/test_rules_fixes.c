// Porting bugs #96, #107 and #118: three rules that read the wrong number.
//
// - #96 `reputation_parse` skipped a number's digits but not its sign, so a
//   signed effect (" +15 0 0", the shape of every shipped rules/gamerep.mes
//   effect) read its adjustment as origin and faction too. The original's
//   four skip loops accept `-` and `+` (0x4C183F, 0x4C18AD, 0x4C195C,
//   0x4C19E6).
// - #107 `item_weight` divided a stack's quantity by 4 before multiplying;
//   the original divides the product (0x4613ED).
// - #118 `object_is_busted` tested trap bit 0x1; `object_bust` sets 0x2, and
//   the original tests 0x2 (0x43F6BD).

#include <stdio.h>

#include "game/item.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/reputation.h"

#include "obj_harness.h"
#include "test_harness.h"

TEST(a_signed_reputation_effect_applies_to_any_origin)
{
    FILE* stream;
    int64_t pc;
    int64_t npc;

    CHECK(obj_harness_init());
    tig_file_mkdir("rules");
    stream = fopen("rules/gamerep.mes", "wb");
    CHECK(stream != NULL);
    if (stream == NULL) {
        return;
    }
    // Factions, then two effects: +15 for anyone, -20 for origin 7 only.
    fputs("{1000}{0 0 0, +15 0 0, -20 7 0}\n", stream);
    fclose(stream);

    CHECK(reputation_init(NULL));
    CHECK(reputation_mod_load());

    pc = obj_harness_create_proto(OBJ_TYPE_PC);
    npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_arrayfield_uint32_set(pc, OBJ_F_PC_REPUTATION_IDX, 0, 1000);

    // A harness NPC has origin 0 and faction 0: the first effect applies,
    // the second does not.
    CHECK_INT(reputation_reaction_adj(pc, npc), 15);

    obj_harness_destroy(npc);
    obj_harness_destroy(pc);
    reputation_mod_unload();
    reputation_exit();
    remove("rules/gamerep.mes");
}

TEST(a_stack_weighs_a_quarter_of_the_product)
{
    int64_t ammo;

    CHECK(obj_harness_init());
    ammo = obj_harness_create_proto(OBJ_TYPE_AMMO);
    CHECK(ammo != OBJ_HANDLE_NULL);
    if (ammo == OBJ_HANDLE_NULL) {
        return;
    }

    obj_field_int32_set(ammo, OBJ_F_ITEM_WEIGHT, 3);
    obj_field_int32_set(ammo, OBJ_F_AMMO_QUANTITY, 2);
    // (3 * 2) / 4 = 1; dividing the quantity first gave 3 * (2 / 4) = 0.
    CHECK_INT(item_weight(ammo, OBJ_HANDLE_NULL), 1);

    obj_field_int32_set(ammo, OBJ_F_AMMO_QUANTITY, 10);
    CHECK_INT(item_weight(ammo, OBJ_HANDLE_NULL), 7);

    obj_harness_destroy(ammo);
}

TEST(a_busted_trap_reads_as_busted)
{
    int64_t trap;

    CHECK(obj_harness_init());
    trap = obj_harness_create_proto(OBJ_TYPE_TRAP);
    CHECK(trap != OBJ_HANDLE_NULL);
    if (trap == OBJ_HANDLE_NULL) {
        return;
    }

    obj_field_int32_set(trap, OBJ_F_TRAP_FLAGS, 0);
    CHECK(!object_is_busted(trap));
    // The bit `object_bust` sets for a trap.
    obj_field_int32_set(trap, OBJ_F_TRAP_FLAGS, 0x2);
    CHECK(object_is_busted(trap));

    obj_harness_destroy(trap);
}

void test_rules_fixes_suite(void)
{
    RUN(a_signed_reputation_effect_applies_to_any_origin);
    RUN(a_stack_weighs_a_quarter_of_the_product);
    RUN(a_busted_trap_reads_as_busted);
}
