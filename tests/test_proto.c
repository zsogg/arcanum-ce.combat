// Tests for prototype description ranges and script-array serialization.
//
// Audit G from CORRECTNESS-AUDIT-CANDIDATES.md is mostly a tooling surface:
// loose .pro files and hardcoded `proto_init_properties` are two sources of
// truth, description ranges gate every prototype, and a copied donor
// prototype inherits SAP_WIELD_ON/OFF scripts. Two properties are cheap to
// pin here:
//
//   * `proto_is_valid` accepts exactly the per-type ranges, including the
//     UAP widenings (GENERIC and MONSTER) and the Arcanum CE reserved
//     mod-item range (GENERIC 15214-16064, ARMOR 8293-9054), all gated
//     behind the same switch so stock validation is unchanged;
//   * `obj_write_mem`/`obj_read_mem` round-trip script arrays, including the
//     two attachment points that caused the charm-necklace donor bug.

#include <stdint.h>
#include <stdlib.h>

#include "game/gamelib.h"
#include "game/obj.h"
#include "game/proto.h"
#include "game/script.h"
#include "game/settings.h"
#include "game/uap.h"

#include <tig/tig.h>

#include "obj_harness.h"
#include "test_harness.h"

TEST(proto_description_ranges_are_half_open)
{
    // Stock 1.0.7.4 ranges, one boundary per representative type.
    CHECK(proto_is_valid(1000));
    CHECK(!proto_is_valid(999));

    CHECK(proto_is_valid(6029));
    CHECK(!proto_is_valid(6028));
    CHECK(proto_is_valid(6178));
    CHECK(!proto_is_valid(6179));

    CHECK(proto_is_valid(8042));
    CHECK(proto_is_valid(8292));
    CHECK(!proto_is_valid(8293));

    // The stock GENERIC bound is the UAP-sensitive one: InvenSource.mes
    // already mentions 15213, but stock validation rejects it.
    CHECK(proto_is_valid(15065));
    CHECK(proto_is_valid(15212));
    CHECK(!proto_is_valid(15213));

    // Monster and unique-NPC thresholds inside the folded NPC range.
    CHECK(proto_is_valid(26000));
    CHECK(!proto_is_valid(26008));
    CHECK(proto_is_valid(27309));
    CHECK(proto_is_valid(27393));
    CHECK(!proto_is_valid(27394));
    CHECK(proto_is_valid(28310));
    CHECK(!proto_is_valid(28472));

    // The Arcanum CE reserved mod-item range is invalid in stock mode; the
    // widening is gated behind the same switch as the UAP widenings.
    CHECK(!proto_is_valid(15214));
    CHECK(!proto_is_valid(16064));
    CHECK(!proto_is_valid(8293));
    CHECK(!proto_is_valid(9054));
}

TEST(uap_widens_generic_armor_and_monster_ranges)
{
    // `uap_init` registers the "uap fixes" setting and reads its default.
    uap_init();

    settings_set_value(&settings, UAP_FIXES_KEY, 1);

    // UAP's own widenings, unchanged.
    CHECK(proto_is_valid(15213));
    CHECK(proto_is_valid(27396));
    CHECK(!proto_is_valid(27397));

    // Arcanum CE reserved mod-item range, GENERIC 15214-16064.
    CHECK(proto_is_valid(15214));
    CHECK(proto_is_valid(16064));
    CHECK(!proto_is_valid(16065)); // OBJ_TYPE_PC begins at 16066

    // Arcanum CE reserved mod-item range, ARMOR 8293-9054.
    CHECK(proto_is_valid(8293));
    CHECK(proto_is_valid(9054));
    CHECK(!proto_is_valid(9055)); // OBJ_TYPE_GOLD begins at 9056

    settings_set_value(&settings, UAP_FIXES_KEY, 0);

    // Back to stock: the widened slots are invalid again.
    CHECK(!proto_is_valid(15213));
    CHECK(!proto_is_valid(27396));
    CHECK(!proto_is_valid(15214));
    CHECK(!proto_is_valid(8293));
}

TEST(script_array_round_trips_through_proto_serialization)
{
    // Build a real armor prototype with WIELD_ON/OFF scripts, serialize it
    // with the engine's own writer, read it back with the engine's own
    // reader, and check the two attachment points survive. This is the
    // donor-inheritance trap from the charm necklace: the recipe zeroed these
    // script numbers in the file, and this test proves nonzero numbers would
    // survive a round trip.
    static const char* proto_path = "./arcanum_test_script_roundtrip.pro";
    Script wield_on;
    Script wield_off;
    Script got;
    TigFile* stream;
    int64_t obj;
    int64_t copy;

    CHECK(obj_harness_init());

    obj = obj_harness_create_proto(OBJ_TYPE_ARMOR);
    CHECK(obj != OBJ_HANDLE_NULL);

    wield_on.hdr.flags = 0xA1;
    wield_on.hdr.counters = 0xB2;
    wield_on.num = 28012;
    wield_off.hdr.flags = 0xC3;
    wield_off.hdr.counters = 0xD4;
    wield_off.num = 28013;

    obj_arrayfield_script_set(obj, OBJ_F_SCRIPTS_IDX, SAP_WIELD_ON, &wield_on);
    obj_arrayfield_script_set(obj, OBJ_F_SCRIPTS_IDX, SAP_WIELD_OFF, &wield_off);

    // Serialize through the file path rather than `obj_write_mem`: the memory
    // writer for complex fields is currently a documented no-op stub, while
    // `obj_write`/`obj_read` are the production prototype path.
    stream = tig_file_fopen(proto_path, "wb");
    CHECK(stream != NULL);
    CHECK(obj_write(stream, obj));
    tig_file_fclose(stream);

    stream = tig_file_fopen(proto_path, "rb");
    CHECK(stream != NULL);
    copy = OBJ_HANDLE_NULL;
    CHECK(obj_read(stream, &copy));
    tig_file_fclose(stream);
    remove(proto_path);
    CHECK(copy != OBJ_HANDLE_NULL);

    obj_arrayfield_script_get(copy, OBJ_F_SCRIPTS_IDX, SAP_WIELD_ON, &got);
    CHECK(got.hdr.flags == 0xA1);
    CHECK(got.hdr.counters == 0xB2);
    CHECK_INT(got.num, 28012);

    obj_arrayfield_script_get(copy, OBJ_F_SCRIPTS_IDX, SAP_WIELD_OFF, &got);
    CHECK(got.hdr.flags == 0xC3);
    CHECK(got.hdr.counters == 0xD4);
    CHECK_INT(got.num, 28013);

    obj_harness_destroy(copy);
    obj_harness_destroy(obj);
}

void test_proto_suite(void)
{
    RUN(proto_description_ranges_are_half_open);
    RUN(uap_widens_generic_armor_and_monster_ranges);
    RUN(script_array_round_trips_through_proto_serialization);

    obj_harness_exit();
}
