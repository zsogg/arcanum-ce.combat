#ifndef ARCANUM_GAME_FRESH_STOCK_H_
#define ARCANUM_GAME_FRESH_STOCK_H_

#include <stdbool.h>

// CE (`fresh shop stock`; zsogg, 2026-11-05): a shop whose stock is still the
// one the module's maps ship restocks once, when its map has loaded, so a
// shop mod's list shows from the first visit. Stock, every campaign shop
// starts with stock baked into the maps, pre-rolled from its own list, and a
// restock is scheduled only when the player closes its trade window
// (`item_inventory_respawn_schedule`), so a list's change showed only from
// the second visit.
//
// Pure: whether `container_obj`, an NPC's substitute inventory, restocks.
// It must be a container with an inventory source, not a spawn-once one,
// never changed since the module shipped it (`obj_is_modified`: a restock,
// a sale or a theft marks it, and a save keeps the mark), and kept by a
// living NPC no one has recruited - the keeper stock's own restock needs.
bool fresh_stock_wanted(bool is_container, int inventory_source, unsigned int container_flags,
    bool modified, bool keeper_dead, bool keeper_has_pc_leader);

#endif /* ARCANUM_GAME_FRESH_STOCK_H_ */
