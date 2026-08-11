#include "obj_harness.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/context.h"
#include "game/effect.h"
#include "game/obj.h"

// See obj_harness.h for what this is and why it works.

static bool obj_harness_initialized;

// `mes_load` resolves a relative path like "rules\\effect.mes" only through a
// registered *directory repository* - there is no fall-back to the working
// directory (see `tig_file_open_internal_native`). So the harness registers "."
// and lays down its own fixture beneath it. These are fixtures, not game data:
// the suite still runs with no Arcanum install.
static bool obj_harness_write_fixture(const char* path, const char* contents)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }

    fwrite(contents, 1, strlen(contents), stream);
    fclose(stream);

    return true;
}

// The effect system needs `rules\\effect.mes` to exist and to yield at least one
// entry, or `effect_init` fails and every effect table stays NULL - which is a
// null dereference the moment anything reads a stat, because `stat_level_get`
// runs the value through `effect_adjust_stat_level`.
//
// It does *not* need the real effects. `effect_init` allocates 400 entries and
// parses whatever numbers the file happens to carry; anything absent is left
// zeroed, and a zeroed entry contributes no changes. So a critter built here
// has its base stats and no effects on them, which is the right default for a
// unit test - the alternative would be silently testing against Troika's
// racial and equipment modifiers.
static bool obj_harness_install_effect_fixture(void)
{
    if (!tig_file_mkdir("rules")) {
        // Already there is fine; only a genuine failure matters, and that shows
        // up as the write below failing.
    }

    // Entry number deliberately below 50: `effect_init` parses only 50 and up,
    // so this satisfies `mes_load`'s "at least one entry" requirement without
    // offering the parser anything to reject.
    return obj_harness_write_fixture("rules/effect.mes",
        "{1}{unused - see obj_harness.c}\n");
}

bool obj_harness_init(void)
{
    GameInitInfo init_info;

    if (obj_harness_initialized) {
        return true;
    }

    // `obj_set_defaults` fills OBJ_F_BLIT_COLOR and OBJ_F_LIGHT_COLOR through
    // `tig_color_make`, which reads the channel shifts. Without this every new
    // object gets colours built from zeroed masks - not a crash, but a silently
    // wrong default, which is worse. ARGB8888 is what the game runs in.
    tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    // The only field of `GameInitInfo` the object system reads is `editor`;
    // the window handle and draw callbacks are for modules that draw. Zeroed
    // first so this does not depend on that staying true.
    memset(&init_info, 0, sizeof(init_info));
    init_info.editor = false;

    // Registering the working directory lets `mes_load` find the fixture
    // below. Harmless if a real install is also present - a test run does not
    // add game archives, so nothing else becomes visible.
    tig_file_repository_add(".");

    if (!obj_harness_install_effect_fixture()) {
        return false;
    }

    if (!obj_init(&init_info)) {
        return false;
    }

    // Ordered after `obj_init` because effects are read through object fields.
    if (!effect_init(&init_info)) {
        return false;
    }

    obj_harness_initialized = true;

    return true;
}

void obj_harness_exit(void)
{
    if (!obj_harness_initialized) {
        return;
    }

    effect_exit();
    obj_exit();
    obj_harness_initialized = false;
}

int64_t obj_harness_create_proto(int type)
{
    int64_t obj = OBJ_HANDLE_NULL;

    if (!obj_harness_init()) {
        return OBJ_HANDLE_NULL;
    }

    obj_create_proto(type, &obj);

    return obj;
}

void obj_harness_destroy(int64_t obj)
{
    if (obj != OBJ_HANDLE_NULL) {
        obj_deallocate(obj);
    }
}
