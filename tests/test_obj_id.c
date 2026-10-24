// Tests for the pure half of `game/obj_id.c`.
//
// ObjectIDs are the stable names saves, settings and scripts use for objects:
// null, blocked, numeric (`A_...`), GUID (`G_...`) and permanent-by-location
// (`P_...`). The string form is serialised and logged, so a formatting or
// parsing slip corrupts saves rather than merely printing something odd.
//
// The one object-facing function, `objid_id_perm_by_load_order`, is not
// covered here: it reads a live object's location/temp-id/map fields, and
// therefore belongs to the object-harness layer, not this dependency-free
// suite.

#include <string.h>

#include "game/context.h"
#include "game/location.h"
#include "game/obj_id.h"

#include "test_harness.h"

static void make_guid(ObjectID* oid)
{
    int i;

    oid->type = OID_TYPE_GUID;
    for (i = 0; i < 16; i++) {
        oid->d.g.data[i] = (uint8_t)i;
    }
}

TEST(validity_is_null_through_p_not_handle_or_blocked)
{
    ObjectID oid;

    memset(&oid, 0, sizeof(oid));

    oid.type = OID_TYPE_HANDLE;
    CHECK(!objid_is_valid(oid));
    oid.type = OID_TYPE_BLOCKED;
    CHECK(!objid_is_valid(oid));
    oid.type = OID_TYPE_NULL;
    CHECK(objid_is_valid(oid));
    oid.type = OID_TYPE_A;
    CHECK(objid_is_valid(oid));
    oid.type = OID_TYPE_GUID;
    CHECK(objid_is_valid(oid));
    oid.type = OID_TYPE_P;
    CHECK(objid_is_valid(oid));
    oid.type = 4;
    CHECK(!objid_is_valid(oid));
}

TEST(a_ids_compare_as_signed_integers_after_type)
{
    ObjectID negative = objid_create_a(-5);
    ObjectID zero = objid_create_a(0);
    ObjectID positive = objid_create_a(7);

    CHECK(objid_compare(negative, zero));
    CHECK(objid_compare(zero, positive));
    CHECK(!objid_compare(positive, zero));

    // Different types order before values: every A sorts before every GUID.
    make_guid(&zero);
    CHECK(objid_compare(objid_create_a(100), zero));
}

TEST(guid_ids_compare_lexicographically_byte_by_byte)
{
    ObjectID a;
    ObjectID b;

    make_guid(&a);
    make_guid(&b);
    a.d.g.data[15] = 0;
    b.d.g.data[15] = 1;

    CHECK(objid_compare(a, b));
    CHECK(!objid_compare(b, a));
    CHECK(!objid_is_equal(a, b));
}

TEST(p_ids_compare_by_location_then_temp_id_then_map)
{
    ObjectID a;
    ObjectID b;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.type = OID_TYPE_P;
    b.type = OID_TYPE_P;

    a.d.p.location = location_make(1, 2);
    b.d.p.location = location_make(2, 1);
    // The packed comparison is lexicographic on the int64: x is the low half,
    // so (2,1) sorts before (1,2).
    CHECK(objid_compare(b, a));
    CHECK(!objid_compare(a, b));

    a.d.p.location = b.d.p.location;
    a.d.p.temp_id = 3;
    b.d.p.temp_id = 4;
    CHECK(objid_compare(a, b));

    a.d.p.temp_id = b.d.p.temp_id;
    a.d.p.map = 5;
    b.d.p.map = 6;
    CHECK(objid_compare(a, b));

    a.d.p.map = b.d.p.map;
    CHECK(!objid_compare(a, b));
    CHECK(objid_is_equal(a, b));
}

TEST(null_and_blocked_parse_from_their_names)
{
    ObjectID oid;

    CHECK(objid_id_from_str(&oid, "NULL"));
    CHECK_INT(oid.type, OID_TYPE_NULL);

    CHECK(objid_id_from_str(&oid, "Blocked"));
    CHECK_INT(oid.type, OID_TYPE_BLOCKED);
}

TEST(a_ids_round_trip_through_their_text_form)
{
    char buffer[OID_STR_LEN];
    ObjectID oid;

    objid_id_to_str(buffer, objid_create_a(0x1234));
    CHECK_STR(buffer, "A_00001234");

    CHECK(objid_id_from_str(&oid, buffer));
    CHECK_INT(oid.type, OID_TYPE_A);
    CHECK_INT(oid.d.a, 0x1234);
}

TEST(p_ids_round_trip_through_their_text_form)
{
    char buffer[OID_STR_LEN];
    ObjectID oid;
    ObjectID parsed;

    memset(&oid, 0, sizeof(oid));
    oid.type = OID_TYPE_P;
    oid.d.p.location = location_make(0x1234, 0x5678);
    oid.d.p.temp_id = 0x9ABC;
    oid.d.p.map = 0xDEF0;

    objid_id_to_str(buffer, oid);
    CHECK_STR(buffer, "P_00001234_00005678_00009ABC_0000DEF0");

    CHECK(objid_id_from_str(&parsed, buffer));
    CHECK_INT(parsed.type, OID_TYPE_P);
    CHECK_INT(parsed.d.p.location, oid.d.p.location);
    CHECK_INT(parsed.d.p.temp_id, oid.d.p.temp_id);
    CHECK_INT(parsed.d.p.map, oid.d.p.map);
}

TEST(guid_ids_round_trip_through_their_text_form)
{
    char buffer[OID_STR_LEN];
    ObjectID oid;
    ObjectID parsed;

    // Porting bug #298: the first three fields are the little-endian dword
    // and words the original prints (0x4E6570).
    make_guid(&oid);
    objid_id_to_str(buffer, oid);
    CHECK_STR(buffer, "G_03020100_0504_0706_0809_0A0B0C0D0E0F");

    CHECK(objid_id_from_str(&parsed, buffer));
    CHECK_INT(parsed.type, OID_TYPE_GUID);
    CHECK(memcmp(parsed.d.g.data, oid.d.g.data, sizeof(oid.d.g.data)) == 0);
}

TEST(a_shipped_guid_file_name_parses_to_the_guid_its_file_holds)
{
    // modules/Arcanum/maps/ShopMap/G_6DAE2D90_6C4D_11D4_8F1D_00A0CC6511C6.mob
    // holds this GUID's bytes at offset 0x24.
    static const uint8_t bytes[16] = {
        0x90, 0x2D, 0xAE, 0x6D, 0x4D, 0x6C, 0xD4, 0x11,
        0x8F, 0x1D, 0x00, 0xA0, 0xCC, 0x65, 0x11, 0xC6
    };
    ObjectID oid;

    CHECK(objid_id_from_str(&oid, "G_6DAE2D90_6C4D_11D4_8F1D_00A0CC6511C6"));
    CHECK_INT(oid.type, OID_TYPE_GUID);
    CHECK(memcmp(oid.d.g.data, bytes, sizeof(bytes)) == 0);
}

TEST(malformed_or_unknown_ids_are_rejected)
{
    ObjectID oid;

    CHECK(!objid_id_from_str(&oid, ""));
    CHECK(!objid_id_from_str(&oid, "X"));
    CHECK(!objid_id_from_str(&oid, "A_0001")); // short numeric field
    CHECK(!objid_id_from_str(&oid, "A_0000000G")); // non-hex
    CHECK(!objid_id_from_str(&oid, "G_00010203_0405_0607_0809_0A0B0C0D0E")); // short GUID
    CHECK(!objid_id_from_str(&oid, "P_00001234_00005678_00009ABC")); // missing map
    CHECK(!objid_id_from_str(&oid, "Handle_1234")); // not implemented, by design
}

void test_obj_id_suite(void)
{
    RUN(validity_is_null_through_p_not_handle_or_blocked);
    RUN(a_ids_compare_as_signed_integers_after_type);
    RUN(guid_ids_compare_lexicographically_byte_by_byte);
    RUN(p_ids_compare_by_location_then_temp_id_then_map);
    RUN(null_and_blocked_parse_from_their_names);
    RUN(a_shipped_guid_file_name_parses_to_the_guid_its_file_holds);
    RUN(a_ids_round_trip_through_their_text_form);
    RUN(p_ids_round_trip_through_their_text_form);
    RUN(guid_ids_round_trip_through_their_text_form);
    RUN(malformed_or_unknown_ids_are_rejected);
}
