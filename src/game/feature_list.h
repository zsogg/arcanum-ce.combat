// The feature-flag table. **This file is the registry.**
//
// One row per engine-side feature a mod may switch on. It is included several
// times with `FEATURE` defined differently each time -- to build the enum, the
// table, and one settings trampoline per feature -- so a feature is declared
// exactly once and cannot be half-added.
//
// `arcanum-work/mods/lib/modkit.py` parses this same file, which is the point:
// the engine and the toolkit read one list rather than two that agree until
// they do not. Keep each row on one line and the strings literal.
//
//     FEATURE(id, settings key, default)
//
// * `id` becomes `FEATURE_<id>`, so a call site naming a flag that does not
//   exist is a compile error rather than a silent false.
// * the settings key is what appears in `arcanum.cfg`. **Changing one breaks
//   every existing config and any mod manifest naming it.**
// * the default is what the flag reads when the key is absent. Everything
//   here defaults to off: the port mirrors Troika's 1.0.7.4 build, and a
//   feature that changed behaviour without being asked for would make that
//   false.
//
// A change callback is *not* declared here. Modules attach their own with
// `feature_on_change` at init, which keeps a callback's side effects in the
// module they belong to -- `uap fixes` pushes state down into tig, and that
// push belongs in `uap.c`, not in a table.

// clang-format off

FEATURE(UAP_FIXES, "uap fixes", false)
FEATURE(CALLED_SHOT_UI, "called shot ui", false)
FEATURE(CALLED_SHOT_THROWS, "called shot throws", false)
FEATURE(DEBUG_CHARACTER_POINTS, "debug character points", false)
FEATURE(REAL_TIME_BUDGET, "real time budget", false)
FEATURE(REAL_TIME_CHASE, "real time chase", false)
FEATURE(FIREARM_RELOADING, "firearm reloading", false)
FEATURE(WEAPON_SWAP_COST, "weapon swap cost", false)
FEATURE(EXERTION, "exertion", false)
FEATURE(WEAPON_RULES, "weapon rules", false)
FEATURE(ON_HIT_SPELLS_FREE, "on-hit spells free", false)
FEATURE(ARMOR_PENETRATION, "armor penetration", false)
FEATURE(AI_ARMOR_AWARE, "ai armor aware", false)
FEATURE(AI_SPELLCASTING, "ai spellcasting", false)
FEATURE(AI_TARGETING, "ai targeting", false)
FEATURE(AI_FOLLOWER_LEASH, "ai follower leash", false)
FEATURE(AI_RESTORATIVES, "ai restoratives", false)
FEATURE(RESTORATIVE_COST, "restorative cost", false)
FEATURE(CRITICAL_PROTECTION, "critical protection", false)
FEATURE(DEBUG_CRITICALS, "debug criticals", false)
FEATURE(MORE_CRITICALS, "more criticals", false)
FEATURE(CRITICAL_EFFECTS, "critical effects", false)
FEATURE(OPENING_SHOT, "opening shot", false)
FEATURE(DEBUG_FUMBLES, "debug fumbles", false)
FEATURE(FIREARM_FUMBLES, "firearm fumbles", false)
FEATURE(FIREARM_CRITICALS, "firearm criticals", false)

// clang-format on
