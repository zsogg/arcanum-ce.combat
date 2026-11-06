#include "game/fresh_stock.h"

#include "game/obj_flags.h"

bool fresh_stock_wanted(bool is_container, int inventory_source, unsigned int container_flags,
    bool modified, bool keeper_dead, bool keeper_has_pc_leader)
{
    return is_container
        && inventory_source != 0
        && (container_flags & OCOF_INVEN_SPAWN_ONCE) == 0
        && !modified
        && !keeper_dead
        && !keeper_has_pc_leader;
}
