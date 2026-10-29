# The macOS port - a guide for Mac developers and their AI assistants

This fork's macOS build is **experimental and untested**: it is compiled on a
GitHub-hosted macOS runner, and nobody maintaining the fork owns a Mac. If you
have one and want to fix what breaks, this document is the context you need.
It is also written to be handed whole to an AI coding assistant (Claude or
another): give it this file and `ARCHITECTURE.md` before asking for anything.

## For the AI assistant: read this first

You are helping a developer with a Mac fix bugs in the macOS build of Arcanum
CE, a C reimplementation of *Arcanum: Of Steamworks and Magick Obscura*
(Troika, 2001), on SDL3. The project's value is **fidelity**: the code mirrors
the original game, quirks included. So:

1. **Fix the platform, not the game.** A macOS fix changes how the program
   meets macOS - paths, windows, input, rendering, packaging. It never
   changes a dice roll, a rule, a save field or a script. If a fix seems to
   need a gameplay change, stop and say so.
2. **Guard platform code** with SDL's macros, the style the tree already uses
   (`src/main.c`, `first_party/tig/src/video.c`):
   ```c
   #if SDL_PLATFORM_MACOS
       ...
   #endif
   ```
   Prefer an SDL3 function or hint over Cocoa code. Linux and Windows
   behaviour must stay byte-for-byte the same.
3. **Comments are a machine-read ledger.** `// 0x4XXXXX` above a function is
   the original binary's address: keep it directly above its definition,
   with nothing in between. Label a deliberate difference from the original
   `// FIX:` (the original is wrong) or `// CE:` (a CE-only change), with
   the reason. Write `// CE (macOS): <why>` for a platform change. Leave
   `// NOTE:` and `// FIX (porting bug #N):` markers as they are; the
   maintainers number porting bugs.
4. **Keep diffs small.** Do not reformat, rename or "clean up" code you are
   not fixing: odd-looking code is usually faithful (see "Fidelity over
   cleanliness" in `ARCHITECTURE.md`). Format with the repository's
   `.clang-format`.
5. **Do not edit the generated files**: `CHANGES.md`, `docs/INSTALL-LINUX.md`
   and `docs/INSTALL-WINDOWS.md` come from the maintainers' tree. Put any
   change they need in the pull request description.
6. **Say what you verified, and on what.** Report the macOS version, the
   chip (Apple Silicon or Intel), the build (Debug or Release, universal or
   one architecture), and which steps below you actually ran. "Builds"
   is not "plays".

## What is known

| | |
|---|---|
| Origin | Upstream ([alexbatalov/arcanum-ce](https://github.com/alexbatalov/arcanum-ce)) supports macOS: the presets, the app bundle and the `Info.plist` are its. |
| This fork's changes | Many engine changes since (see `CHANGES.md`). A search found no Linux-only code in `src/` or `first_party/tig`, but no Mac has run them. |
| Build | Our workflow, `.github/workflows/macos-build.yml`, builds the universal Release app, signs it ad hoc and checks it on a GitHub runner; its runs are the only evidence. First green on 2026-10-29 with Xcode 16.4 and the macOS 15.5 SDK, targeting 10.13: both slices, a valid ad-hoc signature, system libraries only. The executable inside the app is `arcanum-ce`. It runs on GitHub's Intel image, because the Apple Silicon runners were not available. |
| Tests | The same workflow builds and runs the C suite with Apple clang, without the 13 test files that need GNU tools (below): all passed on 2026-10-29, as x86_64. Apple clang found three missing declarations that glibc's headers had hidden, fixed. It also warns of 23 variables that may be used uninitialized on some path, which GCC does not report; they are not yet reviewed against the original. |
| Play | **Not tested at all.** |
| Signing | Ad-hoc only (`codesign -s -`), not notarized. |

## Building on a Mac

You need Xcode (or its command line tools) and CMake 3.25 or later
(`brew install cmake`; `CMakePresets.json` is format version 6). The
libraries (SDL3, SDL3_mixer, zlib, fpattern) are built with the game:
`third_party/` pins their versions, and CMake fetches them from GitHub the
first time you configure, so that step needs the network.

```console
cmake --preset macos                    # Xcode project, x86_64 + arm64, macOS 10.13+
cmake --build --preset macos-release    # or macos-debug
```

The app is `Arcanum Community Edition.app` under `out/build/macos/`, in a
`Release` or `Debug` folder. Its executable's name is the bundle's
`CFBundleExecutable` (`CMakeLists.txt` sets the output name and Xcode's
executable name apart), so the commands below use a wildcard for it. For a quicker build of one architecture while
fixing, configure your own folder:

```console
cmake -S . -B out/build/mac-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build out/build/mac-arm64
```

**The unit tests** are off by default. Build them with a single-config
generator (the preset's Xcode generator is multi-config):

```console
cmake -S . -B out/build/mac-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DARCANUM_BUILD_TESTS=ON
cmake --build out/build/mac-tests
(cd out/build/mac-tests && ctest --output-on-failure)
```

The suite links the real game code, so it is the cheapest check that Apple's
compiler and C library agree with Linux's. A test that passes on Linux and
fails here is a real finding: report it with its output.

On a Mac the suite is smaller. Thirteen test files reach `static` functions
through objects that GNU `objcopy --globalize-symbol` makes, or inject faults
with GNU `ld --wrap`; Apple's toolchain has neither, so
`ARCANUM_TESTS_GNU_LINK` is off on APPLE and leaves them out, with the
emulation differential and the script suite. Measured on Linux with the
option off: 693 cases of the full suite's 871. Do not turn the option on to
"fix" a Mac build.

**Checks on a built app:**

```console
lipo -archs "Arcanum Community Edition.app"/Contents/MacOS/*   # x86_64 arm64
otool -L "Arcanum Community Edition.app"/Contents/MacOS/*      # system libraries only
codesign --verify --deep --strict --verbose=2 "Arcanum Community Edition.app"
```

The commands above follow the presets and upstream's CI; the maintainers have
not run them. Correct this document if one is wrong.

## Setting up the game

The game's data is not in this repository. You need GOG's *Arcanum*
(1.0.7.4) and, for the usual setup, the Unofficial Arcanum Patch 2.0.2.

```console
brew install innoextract sevenzip
innoextract -d ~/Games/arcanum-gog setup_arcanum_*.exe
```

GOG's game folder is the `app` folder inside. Put the app **in the game
folder**, beside `Arcanum.exe` and `modules/`: its `Info.plist` sets
`SDL_FILESYSTEM_BASE_DIR_TYPE` to `parent`, so `src/main.c` changes to the
folder that contains the app and reads everything from there. The release
package's setup script does this, and adds UAP and CE's own files. When you
are fixing, a plain GOG folder should be enough to reach the main menu.

**Running your build without copying it.** Set `ARCANUM_CE_GAME_DIR` to the
game folder and CE uses that folder instead, whatever folder the app is in
(`src/main.c`; on every platform, and nothing changes when it is unset):

```console
ARCANUM_CE_GAME_DIR=~/Games/arcanum-ce \
    out/build/mac-arm64/"Arcanum Community Edition.app"/Contents/MacOS/* -window
```

**What CE reads and writes, all in the game folder:**

| | |
|---|---|
| `arcanum-ce.log` | the log, rewritten at each start; its first line names the build |
| `arcanum-ce.cfg` | CE's settings (without it, CE uses `arcanum.cfg` as the original does) |
| `ce-data/` | CE's own data layer, above `data/` |
| `ce-mods/<id>/` | an optional mod; `mod <id>=1` in the config turns it on |
| `modules/Arcanum/SaveCE/` | CE's saves |

**Starting it.** Double-click the app, or from Terminal, to pass arguments
and see output:

```console
"Arcanum Community Edition.app"/Contents/MacOS/* -window
```

`-window` runs in a window; without it the game is fullscreen.

**Testing downloaded builds from Terminal (developers only).** A build you
compile yourself is not quarantined and runs as it is. A build downloaded
from the CI runner or a release is quarantined, and macOS blocks its
unsigned binary. Instead of running `xattr` on every new download, you can
let Terminal run such binaries (macOS 13 Ventura or later): **System
Settings > Privacy & Security > Developer Tools**, add Terminal, turn it
on, and restart Terminal.

The trade-off: every program started from that Terminal then skips
Gatekeeper's check, not just this game. Use it on a development machine
you are comfortable with, or turn it off again after testing. It does not
help players: it covers only programs started from Terminal, not an app
opened in Finder, and it does not stop App Translocation (below). Players
still use **Open Anyway** or `xattr`.

## Where macOS is likely to differ

This is where to look first, roughly from the most to the least likely. None
of it is confirmed.

1. **Gatekeeper and App Translocation.** A downloaded app is quarantined.
   Until it is cleared, macOS may run it from a hidden, randomized,
   read-only copy. The "parent folder" is then not the game folder, so the
   game finds no data and cannot write its log. The symptom is an
   immediate exit or a missing-files report. Clear it with **System
   Settings > Privacy & Security > Open Anyway**, or with:
   ```console
   xattr -dr com.apple.quarantine "Arcanum Community Edition.app"
   ```
   If this turns out to be common, a clear message beats a silent exit.
2. **Rendering.** `first_party/tig/src/video.c` asks SDL for OpenGL first
   on macOS, because the game streams a texture every frame and upstream
   found that slow on Metal. OpenGL is deprecated on macOS but present.
   Watch for a black screen, flicker, or wrong scaling on a Retina display.
   The `Info.plist` declares `NSHighResolutionCapable`.
3. **Fullscreen and Spaces.** Fullscreen sizes the window to the display.
   macOS's own fullscreen (a separate Space), Cmd-Tab and Mission Control
   may behave differently from Linux and Windows.
4. **Input.**
   - A one-button trackpad: the game uses the right mouse button.
     Two-finger click works as macOS sends it. Control-click does not by
     default; SDL has a hint for it (`SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK`).
   - The keyboard: Mac keyboards lack some keys the game binds, and Cmd is
     not Ctrl.
   - Cmd-Q arrives as a quit event (`first_party/tig/src/message.c`).
     Check that it neither loses a save nor leaves a half-written one.
5. **Ties in sorting.** The engine calls the C library's `qsort` in 17
   places. Where two items compare equal, macOS's library may order them
   differently from glibc or Windows: an inventory or a list in a
   different order, or a different choice where the game takes "the first".
   Linux already differs from the original's Windows here. Report a
   difference with the save that shows it; do not "fix" it alone.
6. **Compiler.** Apple's clang may warn where GCC did not (`-Wall -Wextra`
   are on for the game). A warning in transcribed code is usually faithful
   to the original; report it rather than rewrite it.
7. **Paths.** macOS takes the same POSIX path code as Linux
   (`first_party/tig/src/compat.c`), so the case and separator handling
   Linux needed applies. A case-sensitive APFS volume behaves like Linux; the
   default, case-insensitive one is easier. The app's name contains spaces:
   quote it in scripts.
8. **Audio.** SDL3_mixer plays the music and sound; check that combat music
   starts and that sound survives a change of output device.
9. **Oldest supported macOS.** The preset targets 10.13. Whether SDL3 and the
   current Xcode still build and run there has not been checked.

## Reporting what you find

Open an issue or a pull request on
[zsogg/arcanum-ce.combat](https://github.com/zsogg/arcanum-ce.combat), with:

- the macOS version, the chip, and how you built or installed it;
- `arcanum-ce.log`, copied right after the problem;
- a crash report if there is one (Console.app, or
  `~/Library/Logs/DiagnosticReports/`);
- the save's four files from `modules/Arcanum/SaveCE/` if the problem is in a
  game;
- for a fix: which of the build, test and play steps above you ran, and
  that `clang-format` (the repository's `.clang-format`) leaves your change
  as it is.

The maintainers test every change on Linux and Windows before it ships, so a
macOS fix must leave those builds unchanged. That is why rule 2 above asks
for guarded code.
