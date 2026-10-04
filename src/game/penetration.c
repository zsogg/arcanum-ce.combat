#include "game/penetration.h"

#include <stdint.h>

#include "game/combat.h"

// The design's weights and factors, in hundredths.
#define PENETRATION_LIMB_BODY_WEIGHT 40
#define PENETRATION_LIMB_PIECE_WEIGHT 60
#define PENETRATION_GAP_FACTOR 35
#define PENETRATION_GAP_BASE_CHANCE 50
#define PENETRATION_GAP_AIMED_LIMB 10
#define PENETRATION_GAP_AIMED_HEAD 20

static int penetration_non_negative(int value)
{
    return value > 0 ? value : 0;
}

int penetration_location_protection_centi(int hit_loc, const PenetrationWorn* worn, int natural)
{
    int helmet = penetration_non_negative(worn->helmet);
    int body = penetration_non_negative(worn->body);
    int gloves = penetration_non_negative(worn->gloves);
    int boots = penetration_non_negative(worn->boots);
    int protection;

    switch (hit_loc) {
    case HIT_LOC_HEAD:
        protection = 100 * helmet;
        break;
    case HIT_LOC_ARM:
        protection = PENETRATION_LIMB_BODY_WEIGHT * body + PENETRATION_LIMB_PIECE_WEIGHT * gloves;
        break;
    case HIT_LOC_LEG:
        protection = PENETRATION_LIMB_BODY_WEIGHT * body + PENETRATION_LIMB_PIECE_WEIGHT * boots;
        break;
    default:
        protection = 100 * body;
        break;
    }

    return protection + 100 * penetration_non_negative(natural);
}

int penetration_gap_chance(int crit_bonus, int hit_loc, bool aimed)
{
    int chance = PENETRATION_GAP_BASE_CHANCE + crit_bonus;

    if (aimed) {
        if (hit_loc == HIT_LOC_HEAD) {
            chance += PENETRATION_GAP_AIMED_HEAD;
        } else if (hit_loc == HIT_LOC_ARM || hit_loc == HIT_LOC_LEG) {
            chance += PENETRATION_GAP_AIMED_LIMB;
        }
    }

    if (chance < 0) {
        chance = 0;
    } else if (chance > 100) {
        chance = 100;
    }
    return chance;
}

int penetration_expected_permille(const PenetrationHit* hit, int block_chance)
{
    static const int weights[HIT_LOC_COUNT] = {
        /* HIT_LOC_TORSO */ 70,
        /*  HIT_LOC_HEAD */ 5,
        /*   HIT_LOC_ARM */ 10,
        /*   HIT_LOC_LEG */ 15,
    };
    PenetrationHit each = *hit;
    PenetrationResult result;
    int64_t sum;
    int64_t pass[2];
    int64_t total = 0;
    int loc;
    int blocked;
    int variation;

    if (block_chance < 0) {
        block_chance = 0;
    } else if (block_chance > 100) {
        block_chance = 100;
    }

    each.gap = false;
    for (loc = 0; loc < HIT_LOC_COUNT; loc++) {
        each.hit_loc = loc;
        for (blocked = 0; blocked < 2; blocked++) {
            each.blocked = blocked != 0;
            sum = 0;
            for (variation = PENETRATION_VARIATION_MIN; variation <= PENETRATION_VARIATION_MAX; variation++) {
                each.variation_percent = variation;
                result = penetration_resolve(&each);
                sum += 1000 * (int64_t)result.passed_num / result.passed_den;
            }
            pass[blocked] = sum / (PENETRATION_VARIATION_MAX - PENETRATION_VARIATION_MIN + 1);
        }
        total += weights[loc] * (((100 - block_chance) * pass[0] + block_chance * pass[1]) / 100);
    }

    return (int)((total + 50) / 100);
}

PenetrationResult penetration_resolve(const PenetrationHit* hit)
{
    PenetrationResult result;
    int protection;
    int variation;
    int64_t a;
    int64_t p;
    int64_t num;
    int64_t den;
    int64_t damage;

    // Steps 1 to 3.
    protection = penetration_location_protection_centi(hit->hit_loc, &(hit->worn), hit->natural);
    if (hit->blocked) {
        if (100 * penetration_non_negative(hit->shield) > protection) {
            protection = 100 * penetration_non_negative(hit->shield);
        }
    } else if (hit->gap) {
        protection = protection * PENETRATION_GAP_FACTOR / 100;
    }

    // Step 4.
    protection += 100 * penetration_non_negative(hit->magic);

    // Step 5.
    variation = hit->variation_percent;
    if (variation < PENETRATION_VARIATION_MIN) {
        variation = PENETRATION_VARIATION_MIN;
    } else if (variation > PENETRATION_VARIATION_MAX) {
        variation = PENETRATION_VARIATION_MAX;
    }

    a = protection;
    p = (int64_t)penetration_non_negative(hit->penetration) * variation;

    if (a <= 0 || 2 * p >= 3 * a) {
        // No protection, or a ratio of 1.5 or more: everything.
        num = 1;
        den = 1;
    } else if (2 * p <= a) {
        // A ratio of 0.5 or less: nothing.
        num = 0;
        den = 1;
    } else if (p <= a) {
        // (2r - 1) / 3, with r = p / a.
        num = 2 * p - a;
        den = 3 * a;
    } else {
        // (4r - 3) / 3.
        num = 4 * p - 3 * a;
        den = 3 * a;
    }

    damage = penetration_non_negative(hit->damage);

    result.protection_centi = protection;
    result.penetration_centi = (int)p;
    result.passed_num = (int)num;
    result.passed_den = (int)den;
    // To the nearest point, halves up.
    result.damage = (int)((2 * damage * num + den) / (2 * den));
    return result;
}
