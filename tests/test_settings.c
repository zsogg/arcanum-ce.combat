// Tests for `game/settings.c`.
//
// Settings is a small linked-list registry with integer, string and ObjectID
// views over the same entries. It is pure logic with one file-system pair
// (`settings_load` / `settings_save`); this suite covers the registry and
// callback contracts. The file pair reads and writes plain `key=value` text
// through libc and is exercised every startup/save cycle by the smoke gate.
//
// The callback contract matters because several engine switches - `uap fixes`,
// text duration, combat taunts - register a callback and rely on it firing on
// update but not on registration.

#include "game/obj_id.h"
#include "game/settings.h"

#include "test_harness.h"

static int settings_callback_count;

static void settings_test_callback(void)
{
    settings_callback_count++;
}

TEST(registered_defaults_are_visible)
{
    Settings settings;

    settings_init(&settings, "unused");
    settings_register(&settings, "brightness", "9", NULL);
    settings_register(&settings, "text duration", "6", NULL);

    CHECK_STR(settings_get_str_value(&settings, "brightness"), "9");
    CHECK_STR(settings_get_str_value(&settings, "text duration"), "6");
    CHECK_INT(settings_get_value(&settings, "text duration"), 6);
    CHECK(settings_get_str_value(&settings, "missing") == NULL);
    CHECK_INT(settings_get_value(&settings, "missing"), 0);

    settings_exit(&settings);
    CHECK(settings.path == NULL);
    CHECK(settings.entries == NULL);
}

TEST(keys_are_case_insensitive)
{
    Settings settings;

    settings_init(&settings, "unused");
    settings_register(&settings, "Brightness", "9", NULL);

    settings_set_str_value(&settings, "BRIGHTNESS", "12");
    CHECK_STR(settings_get_str_value(&settings, "brightness"), "12");
    CHECK_STR(settings_get_str_value(&settings, "bRiGhTnEsS"), "12");

    settings_exit(&settings);
}

TEST(integer_view_round_trips_positive_and_negative_values)
{
    Settings settings;

    settings_init(&settings, "unused");
    settings_register(&settings, "value", "0", NULL);

    settings_set_value(&settings, "value", 42);
    CHECK_INT(settings_get_value(&settings, "value"), 42);

    settings_set_value(&settings, "value", -17);
    CHECK_INT(settings_get_value(&settings, "value"), -17);

    settings_exit(&settings);
}

TEST(setting_a_new_key_adds_it_with_the_supplied_default)
{
    Settings settings;

    settings_init(&settings, "unused");
    settings_set_str_value(&settings, "new key", "new value");

    CHECK_STR(settings_get_str_value(&settings, "new key"), "new value");

    settings_exit(&settings);
}

TEST(callback_fires_on_update_but_not_on_registration)
{
    Settings settings;

    settings_callback_count = 0;
    settings_init(&settings, "unused");
    settings_register(&settings, "key", "1", settings_test_callback);
    CHECK_INT(settings_callback_count, 0);

    settings_set_str_value(&settings, "key", "2");
    CHECK_INT(settings_callback_count, 1);

    settings_set_value(&settings, "key", 3);
    CHECK_INT(settings_callback_count, 2);

    settings_exit(&settings);
}

TEST(objectid_view_round_trips)
{
    Settings settings;
    ObjectID expected;
    ObjectID actual;

    settings_init(&settings, "unused");

    expected = objid_create_a(0x1234);
    settings_set_obj_value(&settings, "selected", expected);
    actual = settings_get_obj_value(&settings, "selected");
    CHECK_INT(actual.type, OID_TYPE_A);
    CHECK_INT(actual.d.a, 0x1234);

    actual = settings_get_obj_value(&settings, "missing");
    CHECK_INT(actual.type, OID_TYPE_NULL);

    settings_exit(&settings);
}

void test_settings_suite(void)
{
    RUN(registered_defaults_are_visible);
    RUN(keys_are_case_insensitive);
    RUN(integer_view_round_trips_positive_and_negative_values);
    RUN(setting_a_new_key_adds_it_with_the_supplied_default);
    RUN(callback_fires_on_update_but_not_on_registration);
    RUN(objectid_view_round_trips);
}
