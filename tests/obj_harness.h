#ifndef ARCANUM_TESTS_OBJ_HARNESS_H_
#define ARCANUM_TESTS_OBJ_HARNESS_H_

#include <stdbool.h>
#include <stdint.h>

// A minimal, free-standing object system.
//
// **The problem this solves.** Most of `src/game` is written against
// `obj_field_int32_get(obj, ...)`, so almost none of it can be tested without an
// object to hand. Until now the suites have covered tables, arithmetic and
// packing - everything that needs a critter to exist has been out of reach,
// including the functions that decide what actually happens in play.
//
// **What makes it possible.** `obj_create_proto` builds a *prototype* object:
// it allocates from the object pool, assigns an id and fills in field defaults,
// and it never touches a map, a sector, a location or the art cache. So an
// object can exist with nothing else standing up around it. That is the whole
// trick; the rest of this file is bookkeeping.
//
// **What it is not.** These objects have no location and are in no sector, so
// anything spatial - line of sight, pathing, `object_list_vicinity`, drawing -
// will not work on them and is not meant to. Code that only reads and writes
// fields does.
//
// The boundary has a precise edge, worth stating because it is not obvious:
// **do not write `OBJ_F_LOCATION`.** It is not an ordinary field. Setting it
// calls `obj_find_move` to reindex the object spatially, which dereferences
// sector structures that do not exist here, and the result is a segfault rather
// than a polite failure. Reading it is fine. Everything else in this header
// works because prototype objects genuinely have no place in the world.
//
// ---
//
// **On using this to create objects in a mod.** This is deliberately built out
// of the engine's own entry points rather than by writing struct bytes, so the
// same three calls work outside a test:
//
//     obj_init(&init_info);                      // once, at startup
//     obj_create_proto(OBJ_TYPE_NPC, &proto);    // a prototype
//     obj_create_inst(proto, loc, &instance);    // a thing on the map
//
// A mod that wants a new kind of critter or item wants that third call, which
// needs a location and therefore a loaded map - which is exactly the line this
// harness does not cross. So treat `obj_harness_create_proto` as the supported
// half: it is how you build the template, and `obj_create_inst` is how the game
// stamps copies of it into the world. Keeping the harness on the engine's API
// rather than on a private struct is what makes that continuation possible.

// Stands up the object system and everything it needs, once. Safe to call more
// than once; later calls do nothing.
//
// Returns false if the object system refused to initialise, in which case no
// object call is safe.
bool obj_harness_init(void);

// Tears it down again. Optional - the process is about to exit - but it keeps
// the sanitizer preset quiet about the pool.
void obj_harness_exit(void);

// Creates a prototype object of the given `OBJ_TYPE_*` with its fields at their
// engine defaults. Returns OBJ_HANDLE_NULL on failure.
int64_t obj_harness_create_proto(int type);

// Destroys an object created by `obj_harness_create_proto`.
void obj_harness_destroy(int64_t obj);

#endif /* ARCANUM_TESTS_OBJ_HARNESS_H_ */
