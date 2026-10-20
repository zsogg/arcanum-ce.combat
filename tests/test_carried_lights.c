// Brighter lanterns (zsogg, 2026-10-19): `carried lights` and `carried light
// art`, against the real `critter_light_get`, which makes a critter's carried
// light from the light flags its worn items set. The mod's light art 29 is not
// installed here - the light art table is not loaded - so the switch falls back
// to Disk3x3 (2), the case of the switch without the mod's art. Its sibling
// `light_carried_sync`, which brings a saved light up to date before it is
// recreated, is `static`; it is measured in a game (mods/brighter-lanterns).
#include "game/critter.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/light.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/settings.h"
#include "obj_harness.h"
#include "test_harness.h"
#include "tig/art.h"
#include "tig/file.h"

static int64_t lights_pc;

static bool lights_begin(void)
{
    settings_init(&settings, "unused");
    feature_init();
    // `critter_init` registers it; this suite does not run that init.
    settings_register(&settings, CRITTER_CARRIED_LIGHT_ART_KEY, "29", NULL);
    if (!obj_harness_init()) {
        return false;
    }
    lights_pc = obj_harness_create_proto(OBJ_TYPE_PC);
    return true;
}

static void lights_end(void)
{
    settings_set_value(&settings, feature_key(FEATURE_CARRIED_LIGHTS), 0);
    obj_harness_destroy(lights_pc);
    // As test_followers.c does: later suites enumerate repositories.
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

static tig_art_id_t lights_get(unsigned int flags, unsigned int* color)
{
    obj_field_int32_set(lights_pc, OBJ_F_CRITTER_FLAGS, flags);
    return critter_light_get(lights_pc, color);
}

TEST(stock_every_size_draws_disk2x2_in_its_own_colour)
{
    unsigned int color;

    CHECK(lights_begin());
    CHECK_INT(tig_art_num_get(lights_get(OCF_LIGHT_LARGE, &color)), 1);
    CHECK_INT(color, 0xFFD6AC); // 255, 214, 172: the Lantern's peach
    CHECK_INT(tig_art_num_get(lights_get(OCF_LIGHT_XLARGE, &color)), 1);
    CHECK_INT(color, 0xF4FFFF); // 244, 255, 255: the Electric Light's white
    CHECK_INT(tig_art_num_get(lights_get(OCF_LIGHT_MEDIUM, &color)), 1);
    CHECK_INT(color, 0xEC8A55);
    CHECK_INT(lights_get(0, &color), TIG_ART_ID_INVALID);
    lights_end();
}

TEST(carried_lights_draw_the_configured_art_or_disk3x3_and_warm_the_white)
{
    unsigned int color;

    CHECK(lights_begin());
    settings_set_value(&settings, feature_key(FEATURE_CARRIED_LIGHTS), 1);
    // Art 29 is not listed here, so the fallback: Disk3x3.
    CHECK_INT(tig_art_num_get(lights_get(OCF_LIGHT_LARGE, &color)), 2);
    CHECK_INT(color, 0xFFD6AC); // the Lantern's colour is kept
    CHECK_INT(tig_art_num_get(lights_get(OCF_LIGHT_XLARGE, &color)), 2);
    CHECK_INT(color, 0xFFE8C4); // 255, 232, 196: the Electric Light, warm
    CHECK_INT(tig_art_num_get(lights_get(OCF_LIGHT_SMALL, &color)), 2);
    CHECK_INT(color, 0xA7583A); // 167, 88, 58: unchanged
    CHECK_INT(lights_get(0, &color), TIG_ART_ID_INVALID);
    lights_end();
}

void test_carried_lights_suite(void)
{
    RUN(stock_every_size_draws_disk2x2_in_its_own_colour);
    RUN(carried_lights_draw_the_configured_art_or_disk3x3_and_warm_the_white);
}
