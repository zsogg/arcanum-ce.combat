// The AI's per-fight memo tables forget a critter that is gone (review
// 2026-10-22, finding 6).
//
// `ai_desperate_now` remembers each NPC caster's once-a-turn 40% roll in a
// 64-entry table keyed by object handle. Until 2026-10-22 an entry was
// never released, so after 64 distinct casters in a session the table was
// full for good and every later caster used slot 0 - two of them in one
// fight then evicted each other and re-rolled on every AI pass. The case
// below fills the table with casters that are then destroyed, and shows two
// new casters keeping their own rolls. Against the previous code the second
// caster took the first one's slot and `ai_desperation_rolled` answered
// false for a caster that had just rolled desperate.
//
// The armour-aware and restorative tables take the same rule through the
// same helper; they are reached only from inside an AI pass, so they have
// no case here.
#include "game/feature.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "obj_harness.h"
#include "test_harness.h"

// Public in ai.c, declared in no header: the observer reads them by name.
bool ai_desperate_now(int64_t obj);
bool ai_desperation_rolled(int64_t obj);

// A caster in a fight: `ai_desperate_now` rolls only for an NPC that is
// fighting, which the flag says without a combat.
static int64_t fighting_npc(void)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);

    obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, ONF_FIGHTING);
    return obj;
}

TEST(dead_casters_free_their_desperation_slots)
{
    int64_t fillers[64];
    int64_t first = OBJ_HANDLE_NULL;
    int64_t second;
    int index;
    int tries;

    feature_override(FEATURE_AI_SPELLCASTING, true);

    for (index = 0; index < 64; index++) {
        fillers[index] = fighting_npc();
        ai_desperate_now(fillers[index]);
    }
    for (index = 0; index < 64; index++) {
        obj_harness_destroy(fillers[index]);
    }

    // A caster whose roll came up desperate, so the memo has something to
    // lose. 40% a try; 200 tries never all miss.
    for (tries = 0; tries < 200; tries++) {
        first = fighting_npc();
        if (ai_desperate_now(first)) {
            break;
        }
        obj_harness_destroy(first);
        first = OBJ_HANDLE_NULL;
    }
    CHECK(first != OBJ_HANDLE_NULL);

    second = fighting_npc();
    ai_desperate_now(second);

    // Still remembered: the second caster took a dead filler's slot, not
    // the first caster's.
    CHECK(ai_desperation_rolled(first));

    obj_harness_destroy(first);
    obj_harness_destroy(second);
    feature_clear_overrides();
}

void test_ai_memo_suite(void)
{
    CHECK(obj_harness_init());
    RUN(dead_casters_free_their_desperation_slots);
}
