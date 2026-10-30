# Arcanum CE

> No pretensions to any originality in this work.
> Coding was done nearly entirely using LLM tools, Claude, Astra and Deepseek.
>
> **My grateful acknowledgement to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, the Terra-Arcanum team, and Kababji, who introduced me to this game's
> demo disc all those years ago.**
>
> This project is aimed at improving the vanilla Arcanum experience, and by design the changes it makes are modest.
>
> For a more ambitious and exciting project, may I recommend TechErrorCode's: Arcanum-CE-Restored.
> I am not in any way affiliated with that team, but there is imaginative and impressive work being done here (Discord Link):
> https://discord.com/channels/1533612094522658966/1533656825616928869

> [!IMPORTANT]
> **This is a modified version of [Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce)
> by Alexander Batalov.** It has been changed substantially. Changes summarized in
> [CHANGES.md](CHANGES.md).

Arcanum CE is a rebuilt engine for *Arcanum: Of Steamworks and Magick
Obscura*.

It runs over GOG's Arcanum 1.0.7.4 with the Unofficial Arcanum Patch (UAP)
2.0.2, natively on Linux and Windows, and there is an untested build for
macOS.

Base install can be bug fixes only, with mods enabled by user choice.

**Release 2 (`r2-beta`) is a beta.** Bug reports are welcome, though work on
this project is idiosyncratic to put it generously.

## What is new in release 2

Progress in release 2 focused on bugfixes to both vanilla Arcanum and to the
CE engine rebuild, and expanded combat overhaul features.

A Combat Rebalance covers:

- Guns and armor
- Follower AI fixes/extensions focused on magic casting and healing
- Critical combat events overhaul
- Traps and traps skill overhaul
- Grenade damage revision
- Prowling/stealth revision, vanilla implementation made sneaking very
  difficult

Other smaller mods, please see [CHANGES.md](CHANGES.md).

Mac package - as yet untested, I am working on it.

## Playing

You need Arcanum from GOG (Good Old Games) and the Unofficial Arcanum Patch
2.0.2 (`UAP2.0.2.exe`).

Download the mod package for your system from
[Releases](https://github.com/zsogg/arcanum-ce.combat/releases). Each comes with a README covering installation,
playing, the optional mods and removal; the same guides are below:

- **Windows:** install GOG Arcanum and UAP: **Do not install Arcanum into your Program Files directory**, as this mod will be unable to write there.
- After installing Arcanum and the UAP patch, unzip this package - downloads below,
  run `setup.bat`. Setup changes nothing outside the game folder.
- **Linux:** setup builds the game folder from GOG's offline installer and
  `UAP2.0.2.exe` with `innoextract` and `7z`. No Wine is used.

The original `Arcanum.exe` stays untouched and playable.

Arcanum CE keeps its own saves (`modules/Arcanum/SaveCE`) and settings
(`arcanum-ce.cfg`).

## What it changes

For an interesting read, please see the
[list of original Arcanum quirks](CHANGES.md#6-the-originals-quirks-kept-on-purpose)
left in for now after applying the mod package.

Mod summary:

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
  hit. The fourth, no combat music, replaced by varied music). CE makes each
  change itself rather than using UAP's patched executable.
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

Details in [CHANGES.md](CHANGES.md).

**Not supported:** movies play only on Windows (they are skipped on Linux
and macOS), multiplayer is not supported at present.

## How it is checked against the original

- **Function by function.** Small engine functions are run twice on the same
  inputs: once as CE's C code, and once as the original program's own machine
  code, in an emulator.
- **Save by save.** A set of saves from real games is loaded, and saved again
  by the original under Wine (Windows emulation) and by CE, and the two
  results are compared.
- **Run by run.** The same save, run for a stretch of play with the same
  random seed, must play each step the same.

## Building from source

The libraries the mod needs are vendored under `third_party/`. Builds use CMake
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
and run as `tests/arcanum-tests` in the build folder.

Android, iOS and ARM presets come from upstream; they are not built or tested
here.

The macOS build is experimental: our workflow
(`.github/workflows/macos-build.yml`) compiles it and runs the C suite on a
GitHub runner. If you have a Mac and want to help,
[docs/MACOS-PORT.md](docs/MACOS-PORT.md) is the guide, written to be handed
to an LLM assistant as well.

## Reporting a problem

Open an [issue](https://github.com/zsogg/arcanum-ce.combat/issues/new?template=bug_report.md)
with:

1. **The log**, `arcanum-ce.log` in the game folder. A new one is written
   each time the game starts, so copy it right after the problem. Its first
   line names your build.
2. **The save**, if the problem is in a game: the four files of its slot in
   `modules/Arcanum/SaveCE`.
3. **Please narrate what happened**, and what you did just before.

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
