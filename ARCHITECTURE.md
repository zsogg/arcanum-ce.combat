# Architecture

An orientation guide to the Arcanum Community Edition source tree: what the
layers are, how the ~100 modules are wired together, and the conventions the
code follows. It describes the code as it exists rather than prescribing how it
ought to be organised.

## Layers

The tree has three layers, bottom to top:

| Layer | Path | Size | Role |
|---|---|---|---|
| **TIG** | `first_party/tig` | ~20k lines | Troika's engine: video, art, windows, input, sound, archives |
| **Game** | `src/game` | ~140k lines | Rules and world simulation, with no UI knowledge |
| **UI** | `src/ui` | ~60k lines | Every screen and the interactive game view |

`src/main.c` sits on top and owns the process: it parses the command line,
brings up the three layers in order, runs the event loop, and tears them down.

Dependencies point strictly downward. TIG knows nothing about Arcanum; the game
layer knows nothing about the UI. Where the game layer needs to notify the
interface — "this critter's stats changed, redraw the bars" — it calls through
the `UiCallbacks` function-pointer table in `src/game/ui.h`, which the UI layer
fills in at startup. That indirection is what keeps the dependency one-way.

### TIG (`first_party/tig`)

The platform abstraction, originally Troika's in-house engine and now
implemented on SDL3. Notable modules:

- `art.c` (~6k lines) — the art system: sprite/animation cache, palettes,
  frame lookup, blitting. The largest and most central TIG module.
- `file.c`, `database.c`, `find_file.c` — the virtual filesystem. Game data
  lives in `.dat` archives (`database.c`) which are mounted as *repositories*
  alongside plain directories; lookups search repositories most-recently-mounted
  first. This is the mechanism mods rely on to override base assets.
- `window.c`, `button.c`, `mouse.c`, `kb.c` — windowing and input.
- `video.c`, `bmp.c`, `palette.c`, `font.c`, `color.c` — rendering primitives.
- `sound.c`, `movie.c` — audio and Bink video playback.
- `memory.c`, `idxtable.c`, `bsearch.c`, `str_parse.c`, `guid.c`, `timer.c` —
  utilities.

Everything public is prefixed `tig_` and returns `TIG_OK` / `TIG_ERR_*`.

### Game (`src/game`)

World state and rules. Roughly grouped:

- **Objects** — `obj.c`, `object.c`, `proto.c`, `obj_id.c`, `sector_object_list.c`.
  Everything in the world is an object referred to by an `int64_t` handle.
  `proto.c` (~17k lines, the largest file in the tree) manages prototypes: the
  templates objects are instantiated from, identified by the `BP_*` numbers in
  `descriptions.h`.
- **World** — `map.c`, `sector.c`, `location.c`, `tile.c`, `terrain.c`,
  `jumppoint.c`, `teleport.c`. The map is divided into sectors that load and
  unload around the player.
- **Actors** — `critter.c`, `player.c`, `stat.c`, `skill.c`, `background.c`,
  `fate.c`, `reputation.c`.
- **Systems** — `combat.c`, `magictech.c` (spells and tech), `item.c`,
  `anim.c`, `ai.c`, `dialog.c`, `script.c`, `quest.c`, `rumor.c`.
- **Infrastructure** — `gamelib.c` (module registry and save/load),
  `timeevent.c` (the scheduler), `mes.c` (message-file parsing), `random.c`.
- **Multiplayer** — `multiplayer.c`, `mp_*.c`, and the `mt_*.c` mirrors.

### UI (`src/ui`)

One module per screen, each `*_ui.c`, plus two special cases:

- `intgame.c` (~9k lines) — the interactive game screen: the main HUD, hotbars,
  and the dispatcher for in-world mouse and keyboard input.
- `iso.c` — the isometric world view itself.

The rest are self-contained screens: `mainmenu_ui.c`, `charedit_ui.c`,
`inven_ui.c`, `wmap_ui.c` (world map), `dialog_ui.c`, `logbook_ui.c`,
`schematic_ui.c`, and so on.

## The module lifecycle

This is the single most important pattern in the codebase, and the one most
worth understanding before changing anything.

Both `gamelib.c` and `gameuilib.c` hold a static table of modules — 63 and 29
entries respectively — where each row is a name plus a set of lifecycle
function pointers (`GameLibModule` in `src/game/gamelib.c`):

```c
static GameLibModule gamelib_modules[] = {
    { "HighResolution", hrp_init, NULL, NULL, NULL, NULL, ... },
    { "Description",    description_init, NULL, description_mod_load, ... },
    { "Random",         random_init, NULL, NULL, NULL, random_exit, ... },
    ...
};
```

The available hooks are:

| Hook | When it runs |
|---|---|
| `init_func` | once at startup, in table order |
| `reset_func` | when a new game starts |
| `mod_load_func` / `mod_unload_func` | when a game module (campaign) is loaded or unloaded |
| `ping_func` | every frame, with the current time |
| `update_view_func` | when view options change |
| `save_func` / `load_func` | on save and load, writing to one shared stream |
| `resize_func` | when the window changes size |
| `exit_func` | at shutdown, in reverse table order |

Two consequences matter in practice:

- **Table order is load-bearing.** Modules are initialised top to bottom and
  shut down bottom to top, so a module may only depend on ones listed above it.
- **Save format is positional.** Each `save_func` appends to the same stream and
  each `load_func` reads back in the same order, so changing the table order or
  a module's serialised fields breaks existing saves.

Adding a feature usually means adding a module to one of these tables rather
than calling into existing code from the outside.

### Startup sequence

```
main()
├── tig_init()                 platform, video, art, input
├── gamelib_init()             63 game modules, in table order
│   └── gamelib_load_data()    mounts arcanum*.dat, then ./data
├── gameuilib_init()           29 UI modules, in table order
├── main event loop            ping modules, dispatch input, draw
└── gameuilib_exit() → gamelib_exit() → tig_exit()
```

## Conventions

**Address annotations.** Nearly every function and static carries the address of
the corresponding routine in the original 1.0.7.4 binary:

```c
// 0x404C10
bool gamelib_load_module_data(const char* module_name)
```

These are accurate and are the main tool for reverse-engineering work: load the
original executable in a disassembler and the annotation takes you straight to
the routine a given function came from. Preserve them when moving code.

**Unidentified names.** Functions and data that have not been worked out yet
keep decompiler placeholders — `sub_XXXXXX`, `dword_XXXXXX`, `off_XXXXXX` —
where the suffix is the address.

**Naming is no longer the main task, and the tail is small.** This paragraph
said it was, and that roughly half the modules were fully named, until
2026-09-05; both had stopped being true well before that. The remaining
in-scope unnamed functions are a handful of deliberate keep-outs with written
reasons, and naming is parked as an activity — see `arcanum-work/STATUS.md`
for the current counts, which are generated, and `arcanum-work/tasks/README.md`
for why it is parked. `anim.c` is still the least explored module.

Note that a placeholder-named function usually has *no function defined at that
address* in a freshly analysed disassembly either, because it is only reachable
through the module tables above — auto-analysis cannot follow those indirect
references. Defining functions at the annotated addresses first makes
cross-references usable and greatly speeds up identification.

**Fidelity over cleanliness.** The code deliberately mirrors the original,
including its quirks. Two annotations mark deviations:

- `// FIX:` — a deliberate correction of an original bug, with the reasoning.
- `// NOTE:` — an explanation of where the port differs from the original, or of
  behaviour that looks wrong but is faithful.

Before "cleaning up" something odd, check it against the original binary: it is
often intentional. A refactor that changes a dice roll or a save field is a
regression even when the new code is clearly nicer.

**Naming.** `tig_*` for engine APIs, `<module>_*` for everything else, matching
the file name. Object handles are bare `int64_t`. Message files (`.mes`) hold
all user-visible strings and are read through `mes.c`.

## Building

```console
$ cmake --preset linux-x64-debug
$ cmake --build --preset linux-x64-debug
```

Presets exist for Linux (x86/x64/arm64), Windows, macOS, iOS and Android; see
`CMakePresets.json`. Dependencies (SDL3, SDL3_mixer, zlib, fpattern) are fetched
by CMake at configure time. CI additionally enforces `clang-format`:

```console
$ find src -type f -name \*.c -o -name \*.h | xargs clang-format --dry-run --Werror
```

The build produces a drop-in replacement for `arcanum.exe` that reads the
original game data; it must be run from a directory containing that data.
