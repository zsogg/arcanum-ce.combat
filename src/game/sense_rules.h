#ifndef ARCANUM_GAME_SENSE_RULES_H_
#define ARCANUM_GAME_SENSE_RULES_H_

#include <stdbool.h>

// CE (DB-3, sight and hearing; zsogg, 2026-10-27): the pure rules of the
// sight and hearing overhaul. `ai_can_see` and `ai_can_hear` apply them under
// their switches; arcanum-work `tasks/DB3-sight-and-hearing.md` has the card.

// Rule 2, `walls stop sound`: a loud sound's range through a wall or closed
// door, as a fraction of its range in the open.
#define SENSE_LOUD_THROUGH_WALL_DIVISOR 3

// Pure: the distance at which a sound is heard. `open_range` is the range in
// the open (stock's formula before muffling), `sound_cost` what lies between
// (`ai_line_of_sound_cost`: 2 a wall, 1 a closed door, 0 open air). Stock
// subtracts the cost. Under `walls_stop`, anything between stops a sound that
// is not `loud`, and a loud one carries a third of its open range. -1: not
// heard at any distance.
int sense_rules_hear_distance(int open_range, int sound_cost, bool loud, bool walls_stop);

// Pure: how far a critter sees towards something `facing_delta` steps of
// rotation (0-7) from where it faces. `range` is its sight range in tiles.
// Stock sees the full range within two steps either side and nothing beyond;
// under `side_vision` the two outer steps see half as far, at least a tile.
// -1: not seen.
int sense_rules_sight_range(int range, int facing_delta, bool side_vision);

// Pure: a weapon strike's loudness (a `Loudness`). Under `walls_stop`, a
// blunt weapon strikes loud; otherwise the weapon's own.
int sense_rules_weapon_loudness(int loudness, bool blunt, bool walls_stop);

// Pure: the noise a critter makes by being there (a `Loudness`): silent
// while prowling, and under `still_is_silent` while not moving; normal
// otherwise.
int sense_rules_presence_loudness(bool concealed, bool moving, bool still_is_silent);

#endif /* ARCANUM_GAME_SENSE_RULES_H_ */
