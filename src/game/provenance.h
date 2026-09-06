#ifndef ARCANUM_GAME_PROVENANCE_H_
#define ARCANUM_GAME_PROVENANCE_H_

#include <stdbool.h>

/**
 * Diagnostic provenance for a savegame: what configuration wrote it.
 *
 * Card H9. A save is a stream of ints keyed by ids from data files, and
 * several of those ids are things a mod can change the meaning of - areas,
 * quests, rumours, spells (`arcanum-work/mods/ID-BUDGETS.md`, *The second
 * rule*). When a save then behaves strangely, the first question is *what was
 * this made with*, and until now nothing in the save could answer it.
 *
 * **This is diagnosis and never enforcement.** Nothing reads these files
 * during a load, no load is refused because of them, and a save without them
 * is not suspect - it is a save written before they existed, which answers
 * *unknown*. That is the whole contract, and it is deliberate: metadata
 * identifies a configuration and cannot certify that the configuration is
 * safe.
 */
bool provenance_save(void);

#endif /* ARCANUM_GAME_PROVENANCE_H_ */
