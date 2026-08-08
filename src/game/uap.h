#ifndef ARCANUM_GAME_UAP_H_
#define ARCANUM_GAME_UAP_H_

#include <stdbool.h>

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

#endif /* ARCANUM_GAME_UAP_H_ */
