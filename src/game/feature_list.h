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
FEATURE(SPELL_COSTS, "spell costs", false)
FEATURE(SPELL_APTITUDE_DAMAGE, "spell aptitude damage", false)
FEATURE(SPELL_CRITICALS, "spell criticals", false)
FEATURE(SPELL_NO_STACKING, "spell no stacking", false)
FEATURE(SUMMONING_SICKNESS, "summoning sickness", false)
FEATURE(OPENING_SAFEGUARD, "opening safeguard", false)
FEATURE(POISON_PER_TURN, "poison per turn", false)
FEATURE(PROWL_ATTACK_REVEALS, "prowl attack reveals", false)
FEATURE(PROWL_FOLLOWERS_NEED_SKILL, "prowl followers need skill", false)
FEATURE(PROWL_IN_COMBAT_MASTER_ONLY, "prowl in combat master only", false)
FEATURE(PROWL_DARKNESS_HELPS, "prowl darkness helps", false)
FEATURE(PROWL_OPEN_GROUND_HALVES, "prowl open ground halves", false)
FEATURE(REAL_TIME_RESTORATIVES, "real time restoratives", false)
FEATURE(THROWN_WEAPON_TARGET, "thrown weapon target", false)
FEATURE(UAP_LEVEL_CAP_100, "uap level cap 100", false)
FEATURE(UAP_MASTER_EDUCATOR, "uap master educator", false)
FEATURE(UAP_NO_XP_PER_HIT, "uap no xp per hit", false)
FEATURE(UAP_NO_COMBAT_MUSIC, "uap no combat music", false)
FEATURE(CE_SAVE_FOLDER, "ce save folder", false)
FEATURE(LOG_FILE, "log file", false)
// The Followers mod (card X9; zsogg, 2026-10-19), read by `ai_check_follow`
// and `ai_check_leader`: alignment and level never stop an NPC joining or make
// a follower leave, and low reaction no longer stops one joining (a follower
// whose reaction collapses still leaves).
FEATURE(FOLLOWER_ALIGNMENT_FREE, "follower alignment free", false)
FEATURE(FOLLOWER_LEVEL_FREE, "follower level free", false)
FEATURE(FOLLOWER_REACTION_JOINS, "follower reaction joins", false)
// Brighter lanterns (zsogg, 2026-10-19), read by `critter_light_get`: a carried
// light - the Lantern, Electric Light, Helm of Light, Miner's Helmet - draws the
// light art `carried light art` names (29, the mod's brightened Disk3x3; Disk3x3
// itself when 29 is not installed) instead of the stock Disk2x2, and the
// Electric Light's white becomes warm.
FEATURE(CARRIED_LIGHTS, "carried lights", false)
// Tech specialists (zsogg, 2026-10-19), read by `tech_component_level_required`:
// building a schematic, a component outside the schematic's own discipline
// needs at most `schematic second discipline level` (35, Associate) in its
// discipline, not its full complexity; the own discipline is unchanged.
FEATURE(SCHEMATIC_SECOND_DISCIPLINE_CAP, "schematic second discipline cap", false)
// Initiative and Awareness (card X10; zsogg, 2026-10-20), read by
// `sort_combat_list`: a turn-based fight's order follows each critter's
// initiative, Dexterity + 1-10 rolled once per fight, ties to the party,
// instead of the party acting last in every round.
FEATURE(TURN_ORDER_INITIATIVE, "turn order initiative", false)
// Initiative and Awareness, milestones 2-3 (card X10): before a random
// encounter spawns, the party's best Perception + Intelligence + 1-20 against
// a difficulty rising with the area's encounter power; spotted, avoid it or
// engage at a distance, unspotted, an ambush close by (`wmap_rnd.c`). The
// ambush's opening round lets at most one critter act before the party's
// best-initiative member able to (`combat.c`).
FEATURE(ENCOUNTER_AWARENESS, "encounter awareness", false)
FEATURE(AMBUSH_SAFEGUARD, "ambush safeguard", false)
// Heal living patients (zsogg, 2026-10-21), read by `ai_check_use_skill`: an
// NPC will not use Heal on an undead or mechanical patient, and its own
// nature does not stop it healing - the rule CE had before porting bug #85
// restored the original's (the healer, not the patient, may not be undead or
// mechanical). zsogg's preference, as an option.
FEATURE(HEAL_LIVING_PATIENTS, "heal living patients", false)

// clang-format on
