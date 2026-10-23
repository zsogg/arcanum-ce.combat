// `combat_order_summons` puts a fight's summons after their casters (magic
// milestone MG-3, `summoning sickness`). It takes nodes out of the critter
// list and links them back in - the kind of pointer surgery that, until
// 2026-10-22, no test failed on when it broke (review finding 20: dropping
// `prev->next = next` left a node in two places and the list corrupt at the
// next round). The list and the recorder of who summoned whom are `static`,
// so the build globalizes them for this suite; the nodes are the test's own.
#include <string.h>

#include "game/feature.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/object_node.h"
#include "obj_harness.h"
#include "test_harness.h"

// Globalized out of combat.c and magictech.c by the build; in no header.
extern ObjectList combat_critter_list;
void combat_order_summons(void);
void magictech_summons_record(int64_t summoner_obj, int64_t summon_obj);

static int walk(int64_t* out, int cap)
{
    ObjectNode* node;
    int count = 0;

    for (node = combat_critter_list.head; node != NULL && count < cap; node = node->next) {
        out[count++] = node->obj;
    }
    return node == NULL ? count : -1; // -1: longer than cap, or a cycle
}

static void chain(ObjectNode* nodes, int64_t* objs, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        nodes[i].obj = objs[i];
        nodes[i].next = i + 1 < count ? &nodes[i + 1] : NULL;
    }
    memset(&combat_critter_list, 0, sizeof(combat_critter_list));
    combat_critter_list.head = nodes;
}

TEST(summons_go_after_their_caster_in_the_order_made)
{
    int64_t caster = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t first = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t second = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t other = obj_harness_create_proto(OBJ_TYPE_NPC);
    // An ordinary critter ahead of the summons, so taking them out goes
    // through `prev->next = next` and not only the list head: with the
    // summons first, the mutation this case exists for was not reached
    // (measured 2026-10-23, the first version of this case missed it).
    int64_t before[4] = { other, second, first, caster };
    int64_t after[8];
    ObjectNode nodes[4];

    magictech_summons_record(caster, first);
    magictech_summons_record(caster, second);
    feature_override(FEATURE_SUMMONING_SICKNESS, true);
    chain(nodes, before, 4);

    combat_order_summons();

    CHECK_INT(walk(after, 8), 4);
    CHECK(after[0] == other);
    CHECK(after[1] == caster);
    CHECK(after[2] == first);
    CHECK(after[3] == second);

    feature_clear_overrides();
    combat_critter_list.head = NULL;
    obj_harness_destroy(caster);
    obj_harness_destroy(first);
    obj_harness_destroy(second);
    obj_harness_destroy(other);
}

TEST(a_summon_whose_caster_is_not_in_the_fight_stays_put)
{
    int64_t caster = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t summon = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t other = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t before[2] = { summon, other };
    int64_t after[8];
    ObjectNode nodes[2];

    magictech_summons_record(caster, summon);
    feature_override(FEATURE_SUMMONING_SICKNESS, true);
    chain(nodes, before, 2);

    combat_order_summons();

    CHECK_INT(walk(after, 8), 2);
    CHECK(after[0] == summon);
    CHECK(after[1] == other);

    feature_clear_overrides();
    combat_critter_list.head = NULL;
    obj_harness_destroy(caster);
    obj_harness_destroy(summon);
    obj_harness_destroy(other);
}

TEST(with_the_flag_off_the_list_is_untouched)
{
    int64_t caster = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t summon = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t before[2] = { summon, caster };
    int64_t after[8];
    ObjectNode nodes[2];

    magictech_summons_record(caster, summon);
    chain(nodes, before, 2);

    combat_order_summons();

    CHECK_INT(walk(after, 8), 2);
    CHECK(after[0] == summon);
    CHECK(after[1] == caster);

    combat_critter_list.head = NULL;
    obj_harness_destroy(caster);
    obj_harness_destroy(summon);
}

void test_combat_order_suite(void)
{
    CHECK(obj_harness_init());
    RUN(summons_go_after_their_caster_in_the_order_made);
    RUN(a_summon_whose_caster_is_not_in_the_fight_stays_put);
    RUN(with_the_flag_off_the_list_is_untouched);
}
