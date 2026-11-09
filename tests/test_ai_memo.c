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
#include <stdlib.h>

#include "game/feature.h"
#include "game/magictech.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/random.h"
#include "game/spell.h"
#include "game/stat.h"
#include "obj_harness.h"
#include "test_harness.h"

// Public in ai.c, declared in no header: the observer reads them by name.
bool ai_desperate_now(int64_t obj);
bool ai_desperation_rolled(int64_t obj);
int ai_check_use_spell(int64_t attacker_obj, int64_t item_obj, int64_t target_obj, int spell);
bool ai_in_fight(int64_t obj);

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

// Review 2026-11-08, healing-1: an NPC is in combat mode only after its own
// first swing, so the restorative allowance, given back "out of combat mode",
// came back on every pass of a caster's fight. A fighting NPC is in a fight.
TEST(an_npc_whose_ai_is_fighting_is_in_a_fight_before_its_first_swing)
{
    int64_t idle = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t fighting = fighting_npc();

    obj_field_int32_set(idle, OBJ_F_NPC_FLAGS, 0);
    CHECK(!ai_in_fight(idle));
    CHECK(ai_in_fight(fighting));

    obj_harness_destroy(idle);
    obj_harness_destroy(fighting);
}

// A spell the estimate flickers on. The harness loads no spell list, so the
// case gives `magictech_spells` a table of its own in which this spell resists
// on Strength at +0 with no damage flag: the estimate then answers 100 when
// the target's throw holds, else the target's magic resistance, 0, and
// against Strength 10 the answer is a coin. The stock single ask refuses one
// decision in two; the case checks that premise first, so a changed estimate
// fails here rather than passing on nothing.
#define FLICKERING_SPELL 22
#define FLICKERING_TRIES 400

static int refusals(int64_t caster, int64_t target)
{
    int count = 0;
    int tries;

    random_seed(20261030);
    for (tries = 0; tries < FLICKERING_TRIES; tries++) {
        if (ai_check_use_spell(caster, OBJ_HANDLE_NULL, target, FLICKERING_SPELL) == 8) {
            count++;
        }
    }
    return count;
}

TEST(a_likely_to_fail_refusal_takes_two_bad_estimates_under_spellcasting)
{
    int64_t caster = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t target = obj_harness_create_proto(OBJ_TYPE_NPC);
    int stock;
    int twice;

    // The demonstration battle (2026-10-30): the estimate pre-rolls the
    // target's saving throw, and one bad roll on each of Arronax's two spells
    // sent him in with his fists.
    magictech_spells = (MagicTechInfo*)calloc(MT_SPELL_COUNT, sizeof(MagicTechInfo));
    magictech_spells[FLICKERING_SPELL].resistance.stat = STAT_STRENGTH;
    spell_college_level_set(caster, COLLEGE_FROM_SPELL(FLICKERING_SPELL), 5);
    stat_base_set(caster, STAT_INTELLIGENCE, 20);
    stat_base_set(caster, STAT_WILLPOWER, 20);
    stat_base_set(target, STAT_STRENGTH, 10);

    feature_override(FEATURE_AI_SPELLCASTING, false);
    stock = refusals(caster, target);
    CHECK(stock > FLICKERING_TRIES * 35 / 100);
    CHECK(stock < FLICKERING_TRIES * 65 / 100);

    feature_override(FEATURE_AI_SPELLCASTING, true);
    twice = refusals(caster, target);
    // A quarter, give or take: well under the stock half, and never none.
    CHECK(twice < stock * 70 / 100);
    CHECK(twice > 0);

    obj_harness_destroy(caster);
    obj_harness_destroy(target);
    free(magictech_spells);
    magictech_spells = NULL;
    feature_clear_overrides();
}

// A spell that cannot take the player, as Dominate Will's `Tgt_Obj_No_T_PC`
// (zsogg's Mental-mage playtest, 2026-11-04): the cast refused it and the
// mage lost its turn-based turn. Under `ai spellcasting` the AI refuses it
// first (18); an NPC target is weighed as before, and stock is unchanged.
#define NOT_THE_PLAYER_SPELL 39

TEST(a_spell_that_cannot_take_its_target_is_refused_under_spellcasting)
{
    int64_t caster = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t player = obj_harness_create_proto(OBJ_TYPE_PC);
    int64_t npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    int stock_player;
    int stock_npc;

    magictech_spells = (MagicTechInfo*)calloc(MT_SPELL_COUNT, sizeof(MagicTechInfo));
    magictech_spells[NOT_THE_PLAYER_SPELL].target_params[MAGICTECH_ACTION_BEGIN].tgt
        = TGT_OBJECT | TGT_OBJ_T_CRITTER | TGT_OBJ_NO_T_PC;
    spell_college_level_set(caster, COLLEGE_FROM_SPELL(NOT_THE_PLAYER_SPELL), 5);
    stat_base_set(caster, STAT_INTELLIGENCE, 20);
    stat_base_set(caster, STAT_WILLPOWER, 20);

    feature_override(FEATURE_AI_SPELLCASTING, false);
    random_seed(20261104);
    stock_player = ai_check_use_spell(caster, OBJ_HANDLE_NULL, player, NOT_THE_PLAYER_SPELL);
    random_seed(20261104);
    stock_npc = ai_check_use_spell(caster, OBJ_HANDLE_NULL, npc, NOT_THE_PLAYER_SPELL);
    CHECK_INT(stock_player, stock_npc);

    feature_override(FEATURE_AI_SPELLCASTING, true);
    CHECK_INT(ai_check_use_spell(caster, OBJ_HANDLE_NULL, player, NOT_THE_PLAYER_SPELL), 18);
    random_seed(20261104);
    CHECK_INT(ai_check_use_spell(caster, OBJ_HANDLE_NULL, npc, NOT_THE_PLAYER_SPELL), stock_npc);

    // Review 2026-11-08, casting-3: each switch that changes the choice
    // refuses it too, alone.
    feature_override(FEATURE_AI_SPELLCASTING, false);
    feature_override(FEATURE_CASTER_JUDGEMENT, true);
    CHECK_INT(ai_check_use_spell(caster, OBJ_HANDLE_NULL, player, NOT_THE_PLAYER_SPELL), 18);
    feature_override(FEATURE_CASTER_JUDGEMENT, false);
    feature_override(FEATURE_CASTER_TACTICS, true);
    CHECK_INT(ai_check_use_spell(caster, OBJ_HANDLE_NULL, player, NOT_THE_PLAYER_SPELL), 18);

    obj_harness_destroy(caster);
    obj_harness_destroy(player);
    obj_harness_destroy(npc);
    free(magictech_spells);
    magictech_spells = NULL;
    feature_clear_overrides();
}

void test_ai_memo_suite(void)
{
    CHECK(obj_harness_init());
    RUN(dead_casters_free_their_desperation_slots);
    RUN(an_npc_whose_ai_is_fighting_is_in_a_fight_before_its_first_swing);
    RUN(a_likely_to_fail_refusal_takes_two_bad_estimates_under_spellcasting);
    RUN(a_spell_that_cannot_take_its_target_is_refused_under_spellcasting);
}
