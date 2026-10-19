# Arcanum CE

> No pretensions to any originality in this work.
> Coding was done nearly entirely using LLM tools, Claude, Astra and Deepseek.
>
> **My grateful acknowledgement to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, the Terra-Arcanum team, and Kababji - who introduced this lonely
> schoolboy to Arcanum all those years ago.**
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
on Linux and Windows.

**Release 1 is a beta.** The engine has been checked against the original a
great deal. Please [report](#reporting-a-problem) anything odd.

## Playing

You need Arcanum from GOG and the Unofficial Arcanum Patch 2.0.2
(`UAP2.0.2.exe`). Download the package for your system from
[Releases](https://github.com/zsogg/arcanum-ce.combat/releases). Each comes with a README covering installation,
playing, the optional mods and removal; the same guides are here to read
first: [Windows](docs/INSTALL-WINDOWS.md) and [Linux](docs/INSTALL-LINUX.md).

- **Windows:** install GOG Arcanum and UAP the usual way, unzip the package,
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
  - CE's own save folder and settings, and a log for bug reports.
- **The Unofficial Arcanum Patch's fixes**, all of them, and its four
  optional extras (levels to 100, Master Educator, no experience per hit,
  no combat music). CE makes each change itself rather than using UAP's
  patched executable.
- **Three optional mods**, off unless turned on:
  - **Combat Rebalance (preview):** firearm reloading (**press Q to reload a
    gun**), armor by location and penetration, smarter opponents, criticals,
    magic costs, poison and prowling;
  - **Real-Time Pacing:** each real-time action takes the time its action
    points would take in turn-based combat;
  - **Called Shots:** a panel for Arcanum's own aimed attacks.

Every change, and every quirk of the original kept on purpose, is listed in
[CHANGES.md](CHANGES.md).

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
and run as `tests/arcanum-tests` in the build folder. macOS, Android, iOS
and ARM presets come from upstream; they are not built or tested here.

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
