#ifndef ARCANUM_GAME_INSTALL_CHECK_H_
#define ARCANUM_GAME_INSTALL_CHECK_H_

#include <stdbool.h>

// CE (release 1, R5, 2026-10-13): `arcanum-ce -check-install`, run in the game
// folder before anything starts. It reports, on standard output, what a
// release needs to know about the installation:
//
//   - the base game's archives (GOG 1.0.7.4: tig.dat, arcanum1-4.dat, the
//     module archive and its patches);
//   - UAP's data archive (Arcanum5.dat) and the High Resolution Patch's files;
//   - loose prototypes in `data/proto` that differ from the copy the archives
//     would otherwise give - an engine started before UAP was installed writes
//     the pre-UAP prototypes there (MODDING-NOTES quirk 35), and the loose copy
//     wins, hiding UAP's fixes (65 on the project's own installation,
//     2026-10-03);
//   - CE's own layout: `arcanum-ce.cfg`, `ce-data`, `ce-mods`, `SaveCE`.
//
// `-check-install-fix` also moves the shadowing prototypes into
// `data/proto-shadowing-backup/`, with a list of what it moved, so the
// archives' versions are used again; moving them back restores the old state.
//
// Returns the process exit status: 0 nothing to fix, 1 something found (or
// fixed), 2 the folder is not an Arcanum installation.
int install_check_run(bool fix);

// Pure: whether `name` is a main archive the game mounts, `arcanum*.dat` in
// any case.
bool install_check_is_main_archive(const char* name);

#endif /* ARCANUM_GAME_INSTALL_CHECK_H_ */
