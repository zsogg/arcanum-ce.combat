// Tests for `game/feature.c`, the named feature-flag registry.
//
// The registry is a thin layer over `settings`, so what is worth pinning is
// not "does a bool round-trip" but the four contracts the rest of track X will
// lean on:
//
//   * a flag reads its declared default before anything initialises it, so a
//     test binary that never boots the game still gets the faithful answer;
//   * a value already in the config wins over the default, because
//     `settings_load` runs first and registering an existing key must not
//     overwrite what was loaded;
//   * a change callback fires **exactly once per change**, and not on
//     attachment -- `uap_init` relies on both halves, which is why it calls
//     its own callback by hand;
//   * an unknown key is a refusal, not a false. `settings_get_value` answers 0
//     for a key nobody registered, and 0 reads exactly like "switched off".
//     That silent false is the thing this module exists to remove, so it is
//     the case tested hardest.

#include "game/feature.h"
#include "game/gamelib.h"
#include "game/settings.h"
#include "game/uap.h"

#include "test_harness.h"

static int feature_test_callback_count;

static void feature_test_callback(void)
{
    feature_test_callback_count++;
}

// Every test here shares the one global `settings` the engine uses -- it has
// to, because that is the instance `feature.c` reads -- so each leaves the flag
// as it found it. A test that switched `uap fixes` on and forgot would silently
// change what `test_stat.c` and `test_proto.c` measure, and `test_proto_suite`
// runs after this one.
//
// The pairing matters for the same reason: `settings_exit` leaves `path` and
// `entries` NULL, which is exactly the zero-initialised state the global static
// starts in, so a matched `settings_init`/`settings_exit` here hands the next
// suite the tree it would have seen had this file never existed. Verified in
// `settings_exit` rather than assumed.
static void feature_test_reset(void)
{
    feature_on_change(FEATURE_UAP_FIXES, NULL);
    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 0);
    feature_test_callback_count = 0;
}

TEST(a_flag_reads_its_declared_default_before_any_init)
{
    // No `feature_init`, no `settings_load`, nothing. This is the state a unit
    // test binary is in, and the answer has to be the faithful one.
    CHECK(!feature_enabled(FEATURE_UAP_FIXES));
    CHECK(!feature_default(FEATURE_UAP_FIXES));
    CHECK_STR(feature_key(FEATURE_UAP_FIXES), "uap fixes");
}

TEST(every_declared_feature_has_a_key_and_they_are_distinct)
{
    int i;
    int j;

    CHECK(FEATURE_COUNT > 0);

    for (i = 0; i < FEATURE_COUNT; i++) {
        const char* key = feature_key((FeatureId)i);

        CHECK(key != NULL);
        CHECK(key[0] != '\0');

        // Two features sharing a key would both register against one settings
        // entry, and the second would silently win.
        for (j = 0; j < i; j++) {
            CHECK(strcmp(key, feature_key((FeatureId)j)) != 0);
        }
    }
}

TEST(a_value_already_in_the_config_beats_the_default)
{
    settings_init(&settings, "unused");

    // What `settings_load` does with a line in `arcanum.cfg`: registers the
    // key with no callback. `feature_init` must not overwrite that.
    settings_register(&settings, "uap fixes", "1", NULL);
    feature_init();

    CHECK(feature_enabled(FEATURE_UAP_FIXES));
    CHECK(!feature_default(FEATURE_UAP_FIXES));

    feature_test_reset();
    settings_exit(&settings);
}

TEST(the_change_callback_fires_once_per_change_and_not_on_attach)
{
    settings_init(&settings, "unused");
    feature_init();
    feature_test_callback_count = 0;

    // Attaching must not fire. `uap_init` calls its callback by hand straight
    // after attaching precisely because this is true; if attaching fired, the
    // tig push would happen twice at every boot.
    feature_on_change(FEATURE_UAP_FIXES, feature_test_callback);
    CHECK_INT(feature_test_callback_count, 0);

    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 1);
    CHECK_INT(feature_test_callback_count, 1);
    CHECK(feature_enabled(FEATURE_UAP_FIXES));

    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 0);
    CHECK_INT(feature_test_callback_count, 2);
    CHECK(!feature_enabled(FEATURE_UAP_FIXES));

    feature_test_reset();
    settings_exit(&settings);
}

TEST(a_detached_callback_stops_firing_but_the_flag_still_moves)
{
    settings_init(&settings, "unused");
    feature_init();
    feature_on_change(FEATURE_UAP_FIXES, feature_test_callback);
    feature_test_callback_count = 0;

    feature_on_change(FEATURE_UAP_FIXES, NULL);
    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 1);

    CHECK_INT(feature_test_callback_count, 0);
    CHECK(feature_enabled(FEATURE_UAP_FIXES));

    feature_test_reset();
    settings_exit(&settings);
}

TEST(an_unknown_key_is_refused_rather_than_answered)
{
    FeatureId id = (FeatureId)-1;

    // The refusal. `settings_get_value` would answer 0 here, which is
    // indistinguishable from a flag that exists and is off.
    CHECK(!feature_find("no such feature", &id));
    CHECK(!feature_find("", &id));
    CHECK(!feature_find(NULL, &id));

    // A refused lookup must not have written an id the caller might then use.
    CHECK_INT((int)id, -1);

    // ...and the contrast, so this is not passing because lookup never works.
    CHECK(feature_find("uap fixes", &id));
    CHECK_INT((int)id, (int)FEATURE_UAP_FIXES);
}

TEST(a_key_that_settings_knows_but_the_registry_does_not_is_still_unknown)
{
    settings_init(&settings, "unused");

    // `difficulty` is a real settings key and not a feature. The registry must
    // not answer for it just because `settings` would.
    settings_register(&settings, "difficulty", "1", NULL);

    CHECK(!feature_find("difficulty", NULL));

    settings_exit(&settings);
}

TEST(an_out_of_range_id_is_refused_rather_than_read)
{
    // Unreachable from a call site naming a `FEATURE_*` constant -- that is
    // what the enum buys -- so this reaches it the only way left, by cast. The
    // point is that it does not index past the table.
    CHECK(!feature_enabled((FeatureId)FEATURE_COUNT));
    CHECK(!feature_enabled((FeatureId)-1));
    CHECK(feature_key((FeatureId)FEATURE_COUNT) == NULL);
    CHECK(!feature_default((FeatureId)-1));
}

TEST(uap_fixes_reads_through_the_registry_after_the_migration)
{
    settings_init(&settings, "unused");
    feature_init();
    uap_init();

    // The migration check: `uap_fixes_enabled` is the same answer as the
    // registry's, because it is now a read of it. If these ever disagree the
    // flag has two homes again, which is the state this card removed.
    CHECK_INT(uap_fixes_enabled(), feature_enabled(FEATURE_UAP_FIXES));

    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 1);
    CHECK(uap_fixes_enabled());
    CHECK_INT(uap_fixes_enabled(), feature_enabled(FEATURE_UAP_FIXES));

    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 0);
    CHECK(!uap_fixes_enabled());
    CHECK_INT(uap_fixes_enabled(), feature_enabled(FEATURE_UAP_FIXES));

    feature_test_reset();
    settings_exit(&settings);
}

TEST(an_override_holds_for_the_session_and_does_not_write_the_config)
{
    // Release 1, R4 (2026-10-13): an enabled mod bundle sets its switches this
    // way, so turning the bundle off restores what the config says.
    settings_init(&settings, "unused");
    feature_init();
    CHECK(!feature_enabled(FEATURE_CALLED_SHOT_UI));

    feature_override(FEATURE_CALLED_SHOT_UI, true);
    CHECK(feature_enabled(FEATURE_CALLED_SHOT_UI));
    CHECK(feature_overridden(FEATURE_CALLED_SHOT_UI));
    CHECK_INT(settings_get_value(&settings, feature_key(FEATURE_CALLED_SHOT_UI)), 0);

    // The config changing does not move an overridden feature.
    settings_set_value(&settings, feature_key(FEATURE_CALLED_SHOT_UI), 0);
    CHECK(feature_enabled(FEATURE_CALLED_SHOT_UI));

    feature_clear_overrides();
    CHECK(!feature_overridden(FEATURE_CALLED_SHOT_UI));
    CHECK(!feature_enabled(FEATURE_CALLED_SHOT_UI));
    settings_exit(&settings);
}

void test_feature_suite(void)
{
    RUN(an_override_holds_for_the_session_and_does_not_write_the_config);
    RUN(a_flag_reads_its_declared_default_before_any_init);
    RUN(every_declared_feature_has_a_key_and_they_are_distinct);
    RUN(a_value_already_in_the_config_beats_the_default);
    RUN(the_change_callback_fires_once_per_change_and_not_on_attach);
    RUN(a_detached_callback_stops_firing_but_the_flag_still_moves);
    RUN(an_unknown_key_is_refused_rather_than_answered);
    RUN(a_key_that_settings_knows_but_the_registry_does_not_is_still_unknown);
    RUN(an_out_of_range_id_is_refused_rather_than_read);
    RUN(uap_fixes_reads_through_the_registry_after_the_migration);
}
