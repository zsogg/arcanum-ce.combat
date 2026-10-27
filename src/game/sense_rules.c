#include "game/sense_rules.h"

#include "game/combat.h"

int sense_rules_hear_distance(int open_range, int sound_cost, bool loud, bool walls_stop)
{
    if (!walls_stop || sound_cost <= 0) {
        return open_range - sound_cost;
    }
    if (!loud) {
        return -1;
    }
    return open_range / SENSE_LOUD_THROUGH_WALL_DIVISOR;
}

int sense_rules_sight_range(int range, int facing_delta, bool side_vision)
{
    int delta = ((facing_delta % 8) + 8) % 8;

    if (delta == 0 || delta == 1 || delta == 7) {
        return range;
    }
    if (delta == 2 || delta == 6) {
        if (!side_vision) {
            return range;
        }
        return range / 2 > 1 ? range / 2 : 1;
    }
    return -1;
}

int sense_rules_weapon_loudness(int loudness, bool blunt, bool walls_stop)
{
    if (walls_stop && blunt) {
        return LOUDNESS_LOUD;
    }
    return loudness;
}

int sense_rules_presence_loudness(bool concealed, bool moving, bool still_is_silent)
{
    if (concealed || (still_is_silent && !moving)) {
        return LOUDNESS_SILENT;
    }
    return LOUDNESS_NORMAL;
}
