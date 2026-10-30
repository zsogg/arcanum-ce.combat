# Arcanum CE

> No pretensions to any originality in this work.
> Coding was done nearly entirely using LLM tools, Claude, Astra and Deepseek.
>
> **My grateful acknowledgement to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, the Terra-Arcanum team, and Kababji.**
>
> This project is aimed at improving the vanilla Arcanum experience, and by design the changes it makes are modest.
>
> For a more ambitious and exciting project, may I recommend TechErrorCode's: Arcanum-CE-Restored.
> I am not in any way affiliated with that team, but there is imaginative and impressive work being done here (Discord Link):
> https://discord.com/channels/1533612094522658966/1533656825616928869

> [!IMPORTANT]
> **This is a modified version of [Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce)
> by Alexander Batalov.** It has been changed substantially: see
> [what changes](#what-it-changes) below and the release catalogue,
> [CHANGES.md](CHANGES.md). Upstream's own
> README is kept unchanged as [UPSTREAM-README.md](UPSTREAM-README.md).

Arcanum CE is a rebuilt engine for *Arcanum: Of Steamworks and Magick
Obscura*, made from the original program to play the same game. It runs over
GOG's Arcanum 1.0.7.4 with the Unofficial Arcanum Patch (UAP) 2.0.2, natively
on Linux and Windows, and there is an untested build for macOS.

Out of the box it plays Troika's game, with the bugs fixed and the quirks
kept. Everything beyond that is an optional mod, off until you turn it on,
and each can be turned off again.

**Release 2 (`r2-beta`) is a beta.** The engine has been checked against the
original a great deal. Please [report](#reporting-a-problem) anything odd.

## What is new in release 2

Release 1 made the game run. Release 2 asked a harder question: does it run
*the same*? Every function of the engine was set beside the original
program, one at a time, and that review found more than two hundred places
where the rebuilt code had drifted from what Troika shipped. A stun barely
stunned. Each random-encounter table only ever rolled its first line, so
whole bands of the Malochian Hand never appeared. A mistyped day length ran
the moon fourteen hours fast every month. Wall of Fire burned its own
caster's side. All of these are put right, so that a fight, a conversation
or a journey across the world map behaves as it did in 2001.

The other half of the work is optional. The Combat Rebalance grew from guns
and armour into followers who cast and heal as if they meant it, creatures
whose bodies decide what a critical does, traps you can see coming, grenades
that hurt whoever stands in the blast, and NPCs who hear through a closed
door only what a door lets through. Six smaller mods join it. And for the
first time there is a Mac package, built by GitHub's machines from the same
source: nobody making this release owns a Mac, so its first players are its
first testers.

## Playing

You need Arcanum from GOG and the Unofficial Arcanum Patch 2.0.2
(`UAP2.0.2.exe`). Download the package for your system from
[Releases](https://github.com/zsogg/arcanum-ce.combat/releases). Each comes with a README covering installation,
playing, the optional mods and removal; the same guides are here to read
first: [Windows](docs/INSTALL-WINDOWS.md), [Linux](docs/INSTALL-LINUX.md) and
[macOS](docs/INSTALL-MACOS.md) (**untested**: built on GitHub's runner, never
run by the maintainers).

- **Windows:** install GOG Arcanum and UAP: Do not install Arcanum into your Program Files directory, as this mod will be unable to write there.
- After installing Arcanum and the UAP patch, unzip this package - downloads below,
  and run `setup.bat`. Setup changes nothing outside the game folder.
- **Linux:** setup builds the game folder from GOG's offline installer and
  `UAP2.0.2.exe` with `innoextract` and `7z`. No Wine is used.

The original `Arcanum.exe` stays untouched and playable. Arcanum CE keeps its
own saves (`modules/Arcanum/SaveCE`) and settings (`arcanum-ce.cfg`).

## What it changes

Where nothing is said, the game plays as the original does, its quirks
included, on purpose. In short:

- **Always on:**
  - bugs of the original fixed (spells, fatigue, walking, crashes, saving,
    inventory, menus, drawing);
  - damaged game files refused rather than trusted;
  - saves written safely, so a failed save no longer damages the old one;
  - CE's own save folder and settings, and a log for bug reports;
  - varied combat music: a fight opens with the usual sting, then plays the
    game's own area music in random order, where the original looped one
    track through every fight;
  - NPCs give first aid only to living patients, not to robots.
- **The Unofficial Arcanum Patch's fixes**, all of them, and three of its
  four optional extras (levels to 100, Master Educator, no experience per
  hit; its fourth, no combat music, gives way to the varied music). CE makes
  each change itself rather than using UAP's patched executable.
- **Nine optional mods**, off unless turned on. Setup asks about each, and
  afterwards each is one line in `arcanum-ce.cfg`:
  - **Combat Rebalance (preview):** firearm reloading (**press Q to reload a
    gun**), armour by location and penetration, smarter opponents and
    followers, criticals by body type, magic costs, traps, grenades,
    fumbles, sight and hearing, poison and prowling;
  - **Real-Time Pacing:** each real-time action takes the time its action
    points would take in turn-based combat;
  - **Called Shots:** a panel for Arcanum's own aimed attacks;
  - **Followers:** alignment and level no longer stop an NPC joining you;
  - **Initiative and Awareness:** rolled turn order, and on the world map a
    chance to spot an encounter coming and avoid it;
  - **Better Shops:** schematics in stock, specialist components for sale,
    Conjure Spirit scrolls in every magic shop;
  - **Starting Gold:** three times the gold your background gives;
  - **Brighter Lanterns:** carried lights throw a wider, brighter circle;
  - **Encounter Variety (experimental):** more frequent and more varied
    random encounters, with rare creatures at night.

  The six after Called Shots are new in release 2, and only Starting Gold
  has been played with yet.

Every change, and every quirk of the original kept on purpose, is listed in
[CHANGES.md](CHANGES.md).

**Not supported:** the movies play only on Windows (they are skipped on Linux
and macOS), and multiplayer is not supported.

## How it is checked against the original

The original `Arcanum.exe` is the reference for every question of how the
game should behave, and the project keeps asking it:

- **Function by function.** Small engine functions are run twice on the same
  inputs: once as CE's C code, and once as the original program's own machine
  code, in an emulator. The two must agree on every input, or the difference
  has to be a recorded, deliberate fix.
- **Save by save.** A set of saves from real games is loaded and saved again
  by the original under Wine and by CE, and the two results are compared
  file by file.
- **Run by run.** The same save, run for a stretch of play with the same
  random seed, must come out byte for byte the same twice over, and the same
  on Linux as on Windows.
- **Fix by fix.** Every place where CE's code had gone wrong is numbered in a
  ledger and marked in the source, and every bug of the original that CE
  fixes is listed in [CHANGES.md](CHANGES.md). So is every quirk it keeps.

## Building from source

The libraries it needs are vendored under `third_party/`. Builds use CMake
presets:

```console
cmake --preset linux-x64-release
cmake --build --preset linux-x64-release        # out/build/linux-x64-release/arcanum-ce
```

The Windows build is cross-compiled with MinGW-w64
(`x86_64-w64-mingw32-gcc`):

```console
cmake --preset windows-x64-mingw
cmake --build --preset windows-x64-mingw        # out/build/windows-x64-mingw/arcanum-ce.exe
```

The unit tests build with `linux-x64-debug` (or `linux-x64-release-tests`)
and run as `tests/arcanum-tests` in the build folder. Android, iOS and ARM
presets come from upstream; they are not built or tested here.

The macOS build is experimental: our workflow
(`.github/workflows/macos-build.yml`) compiles it and runs the C suite on a
GitHub runner, and it has not been played. If you have a Mac and want to help,
[docs/MACOS-PORT.md](docs/MACOS-PORT.md) is the guide, written to be handed
to an AI coding assistant as well.

## Reporting a problem

Open an [issue](https://github.com/zsogg/arcanum-ce.combat/issues/new?template=bug_report.md)
(the bug-report form asks for these) with:

1. **The log**, `arcanum-ce.log` in the game folder. A new one is written
   each time the game starts, so copy it right after the problem. Its first
   line names your build.
2. **The save**, if the problem is in a game: the four files of its slot in
   `modules/Arcanum/SaveCE`.
3. **What happened**, and what you did just before.

## License

The source code in this repository is available under the
[Sustainable Use License](LICENSE.md), the license of the upstream project,
[Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce) by
Alexander Batalov. In upstream's words: "The source code is this repository
is available under the Sustainable Use License."

- **This is a modified version** of that software, as its license requires
  this notice to say. The modifications are under the same license.
- **Free, non-commercial distribution only:** the license permits
  distributing the software only free of charge, for non-commercial
  purposes.
- **Third-party libraries** keep their own licenses, in `third_party/`
  (SDL3, SDL3_mixer, zlib, fpattern).
- **Arcanum** is (c) Troika Games and Sierra. Nothing of the game is in this
  repository or its releases; you need your own copy.
