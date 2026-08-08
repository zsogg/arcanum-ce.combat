#ifndef ARCANUM_GAME_UAP_H_
#define ARCANUM_GAME_UAP_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * Fixes ported from the Unofficial Arcanum Patch 2.0.2, off by default.
 *
 * The original 1.0.7.4 binary is what this port mirrors, so every behaviour
 * UAP changes lives behind this switch: with it off the engine does what
 * Troika's build does, bug for bug. Set `uap fixes=1` in `arcanum.cfg` to get
 * the patched behaviour instead.
 *
 * Each ported site carries a `// UAP (0xADDR)` comment naming the address the
 * patch rewrites in the binary, so `arcanum-work/scripts/uap_show.py` can be
 * pointed straight at it. Only engine changes are ported here; UAP's data,
 * dialog and map work rides in its `.dat` archives, which load as-is, and the
 * High Resolution Patch is a separate component that `hrp.c` already covers.
 */
void uap_init(void);
bool uap_fixes_enabled(void);

/**
 * Returns the object handle of the item currently being dragged in the
 * inventory UI, or `OBJ_HANDLE_NULL` if none is.
 */
typedef int64_t (*UapDragItemQuery)(void);

/**
 * Installs the inventory UI's drag-item accessor, or clears it with `NULL`.
 *
 * One UAP change - the action-point override at 0x4B77BD - needs a `src/game`
 * function to read `inven_ui_drag_item_obj` (0x6810E0), a static in
 * `src/ui/inven_ui.c`. Nothing in CE reaches upward like that: no file under
 * `src/game` includes a `src/ui` header, and breaking that for one
 * flag-gated read is not worth it. So the dependency is inverted here
 * instead. `inven_ui_init` hands its existing `inven_ui_drag_item_obj_get`
 * down, and `inven_ui_exit` clears it - the same shape as TIG receiving the
 * switch itself through `tig_art_uap_fixes_set`.
 *
 * Only UAP-gated code should read this. Nothing in the faithful path needs to
 * know what the inventory UI is doing.
 */
void uap_drag_item_query_set(UapDragItemQuery query);

/**
 * Whether an item is currently being dragged in the inventory UI.
 *
 * False when no query has been installed, which is the case before
 * `inven_ui_init` runs - correct, since there is no inventory to drag in yet.
 * Callers still gate on `uap_fixes_enabled` themselves; this is a plain query.
 */
bool uap_drag_item_active(void);

#endif /* ARCANUM_GAME_UAP_H_ */
