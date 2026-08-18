// Tests for `magictech_invocation_parent_get`.
//
// Audit E from `CORRECTNESS-AUDIT-CANDIDATES.md`. This function is the trap a
// mod hits when it creates an item object directly: the magictech invocation
// uses the *wielder* as the parent only when the object is an inventory item
// with OF_INVENTORY set, and otherwise uses the object itself. The original
// 0x450A50 decompiles as:
//
//     if (obj != OBJ_HANDLE_NULL) {
//         type = obj_field_int32_get(obj, OBJ_F_TYPE);
//         if (type != OBJ_TYPE_PC
//             && type != OBJ_TYPE_NPC
//             && type > OBJ_TYPE_PROJECTILE      // > 4
//             && type < OBJ_TYPE_PC              // < 15
//             && (obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_INVENTORY) != 0) {
//             return obj_field_handle_get(obj, OBJ_F_ITEM_PARENT);
//         }
//     }
//     return obj;
//
// So the full decision table is: WEAPON..GENERIC (5..14) plus OF_INVENTORY
// and a valid parent handle. Everything else returns the input object. These
// tests pin that table with real prototype objects from the obj_harness.

#include "game/magictech.h"
#include "game/obj.h"

#include "obj_harness.h"
#include "test_harness.h"

// `magictech_invocation_parent_get` is file-local to magictech.c and
// deliberately not exported in magictech.h. Linking the real object gives us
// the symbol; declaring it here keeps the test dependency explicit.
extern int64_t magictech_invocation_parent_get(int64_t obj);

static int64_t make_proto(int type)
{
    return obj_harness_create_proto(type);
}

static void set_inventory_item_state(int64_t item_obj, int64_t parent_obj)
{
    int flags = obj_field_int32_get(item_obj, OBJ_F_FLAGS);

    obj_field_int32_set(item_obj, OBJ_F_FLAGS, flags | OF_INVENTORY);
    obj_field_handle_set(item_obj, OBJ_F_ITEM_PARENT, parent_obj);
}

static void set_inventory_flag(int64_t obj)
{
    int flags = obj_field_int32_get(obj, OBJ_F_FLAGS);

    obj_field_int32_set(obj, OBJ_F_FLAGS, flags | OF_INVENTORY);
}

TEST(null_parent_resolves_to_null)
{
    CHECK(obj_harness_init());

    CHECK(magictech_invocation_parent_get(OBJ_HANDLE_NULL) == OBJ_HANDLE_NULL);
}

TEST(every_item_type_resolves_to_parent_when_in_inventory)
{
    // The declared range is OBJ_TYPE_WEAPON..OBJ_TYPE_GENERIC (5..14). Both
    // ends are the boundary a mod-added item is most likely to touch.
    static const int item_types[] = {
        OBJ_TYPE_WEAPON,
        OBJ_TYPE_AMMO,
        OBJ_TYPE_ARMOR,
        OBJ_TYPE_GOLD,
        OBJ_TYPE_FOOD,
        OBJ_TYPE_SCROLL,
        OBJ_TYPE_KEY,
        OBJ_TYPE_KEY_RING,
        OBJ_TYPE_WRITTEN,
        OBJ_TYPE_GENERIC,
    };
    int64_t parent_obj;
    int i;

    CHECK(obj_harness_init());

    parent_obj = make_proto(OBJ_TYPE_NPC);
    CHECK(parent_obj != OBJ_HANDLE_NULL);

    for (i = 0; i < (int)(sizeof(item_types) / sizeof(item_types[0])); i++) {
        int64_t item_obj = make_proto(item_types[i]);

        CHECK(item_obj != OBJ_HANDLE_NULL);
        set_inventory_item_state(item_obj, parent_obj);

        CHECK_INT(magictech_invocation_parent_get(item_obj), parent_obj);

        obj_harness_destroy(item_obj);
    }

    obj_harness_destroy(parent_obj);
}

TEST(item_without_inventory_flag_resolves_to_itself)
{
    // OF_INVENTORY is the other half of the condition. A freshly created item
    // does not have it, which is the "created objects directly" trap: CE must
    // return the item itself, not read an unset parent field.
    static const int item_types[] = {
        OBJ_TYPE_WEAPON,
        OBJ_TYPE_ARMOR,
        OBJ_TYPE_GENERIC,
    };
    int64_t parent_obj;
    int i;

    CHECK(obj_harness_init());

    parent_obj = make_proto(OBJ_TYPE_NPC);
    CHECK(parent_obj != OBJ_HANDLE_NULL);

    for (i = 0; i < (int)(sizeof(item_types) / sizeof(item_types[0])); i++) {
        int64_t item_obj = make_proto(item_types[i]);

        CHECK(item_obj != OBJ_HANDLE_NULL);
        // Parent field deliberately set: the flag, not the field, must gate
        // the lookup.
        obj_field_handle_set(item_obj, OBJ_F_ITEM_PARENT, parent_obj);

        CHECK(magictech_invocation_parent_get(item_obj) == item_obj);

        obj_harness_destroy(item_obj);
    }

    obj_harness_destroy(parent_obj);
}

TEST(critters_never_resolve_to_an_item_parent)
{
    // The original checks PC and NPC explicitly even though the range check
    // already excludes them. Pin both.
    static const int critter_types[] = { OBJ_TYPE_PC, OBJ_TYPE_NPC };
    int64_t other_obj;
    int i;

    CHECK(obj_harness_init());

    other_obj = make_proto(OBJ_TYPE_NPC);
    CHECK(other_obj != OBJ_HANDLE_NULL);

    for (i = 0; i < (int)(sizeof(critter_types) / sizeof(critter_types[0])); i++) {
        int64_t critter_obj = make_proto(critter_types[i]);

        CHECK(critter_obj != OBJ_HANDLE_NULL);
        set_inventory_flag(critter_obj);

        CHECK(magictech_invocation_parent_get(critter_obj) == critter_obj);

        obj_harness_destroy(critter_obj);
    }

    obj_harness_destroy(other_obj);
}

TEST(non_item_types_resolve_to_themselves)
{
    // The other side of the range: below WEAPON and above GENERIC. A mod item
    // with a bad type must not be proxied to another object.
    // MONSTER/UNIQUE_NPC are deliberately absent: `obj_create_proto` asserts
    // on those types (no per-type field table), so the object harness cannot
    // instantiate them. WALL/PORTAL/CONTAINER/SCENERY/PROJECTILE cover the
    // below-range half and TRAP covers the above-range half of the decision.
    static const int non_item_types[] = {
        OBJ_TYPE_WALL,
        OBJ_TYPE_PORTAL,
        OBJ_TYPE_CONTAINER,
        OBJ_TYPE_SCENERY,
        OBJ_TYPE_PROJECTILE,
        OBJ_TYPE_TRAP,
    };
    int64_t other_obj;
    int i;

    CHECK(obj_harness_init());

    other_obj = make_proto(OBJ_TYPE_NPC);
    CHECK(other_obj != OBJ_HANDLE_NULL);

    for (i = 0; i < (int)(sizeof(non_item_types) / sizeof(non_item_types[0])); i++) {
        int64_t obj = make_proto(non_item_types[i]);

        CHECK(obj != OBJ_HANDLE_NULL);
        set_inventory_flag(obj);

        CHECK(magictech_invocation_parent_get(obj) == obj);

        obj_harness_destroy(obj);
    }

    obj_harness_destroy(other_obj);
}

void test_magitech_suite(void)
{
    RUN(null_parent_resolves_to_null);
    RUN(every_item_type_resolves_to_parent_when_in_inventory);
    RUN(item_without_inventory_flag_resolves_to_itself);
    RUN(critters_never_resolve_to_an_item_parent);
    RUN(non_item_types_resolve_to_themselves);

    obj_harness_exit();
}
