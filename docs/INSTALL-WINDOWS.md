# Arcanum CE for Windows
<!-- Generated from the project's release documents by sync-public-docs.py; edit those, not this copy. -->

> No pretensions to any originality in this work. My grateful acknowledgement
> to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, and Kababji - who introduced this lonely
> schoolboy to Arcanum all those years ago.

Arcanum CE is a rebuild of *Arcanum: Of Steamworks and Magick
Obscura*.

- It builds upon the Unofficial Arcanum Patch (UAP) 2.0.2 and its High
  Resolution Patch, which you install first.
- It installs beside the original game: `Arcanum.exe`. Arcanum saves are left
  as they are, and CE keeps its own.

## What you need

- **Arcanum from GOG**, installed.
  - GOG's offline installer puts it in `C:\GOG Games\Arcanum`, which is
    ideal.
  - **Not under Program Files**: CE keeps its saves and settings in the game
    folder, and Windows does not let a program write there.
- **The Unofficial Arcanum Patch 2.0.2** (`UAP2.0.2.exe`), installed into that
  folder, with the High Resolution Patch selected.
- Windows 10 or 11, 64-bit.

## Installing

1. Unzip this package anywhere.
2. Double-click **`setup.bat`**.
   - Windows SmartScreen may warn that the program is unrecognized, because
     CE is not code-signed. Choose **More info**, then **Run anyway**.
3. Setup then:
   - finds the game folder, or asks for it;
   - checks that the game and UAP are there;
   - shows a table: what is always on, what each
     mod turns on, and what setup never turns on;
   - asks yes or no for each of the three optional mods: Called Shots,
     Real-Time Pacing and the Combat Rebalance.
   - installs Arcanum CE into the folder and runs a check.

**Setup changes nothing outside the game folder**: no registry value and no
file anywhere else, unless you ask for a shortcut.

**Options**, from a PowerShell or command window in the package folder:

- `.\setup.bat -StartMenuShortcut` adds **Arcanum CE** to the Start menu.
- `-DesktopShortcut` puts it on the desktop.
- `-AllMods` turns every optional mod on without asking.
- `-Mod combat-rebalance` turns on one.
- `-GameDir "C:\GOG Games\Arcanum"` names the game folder.

**Running setup again** updates CE and keeps your settings. It shows the
tables and asks the questions again; pressing Enter keeps each mod as it is.

## Playing

- Start `arcanum-ce.exe` in the game folder, or the **Arcanum CE** shortcut if
  you asked setup for one.
- Settings are in `arcanum-ce.cfg`.
- The screen size and the High Resolution Patch's options are in
  `HighRes\config.ini`.
- The original `Arcanum.exe` still runs, unchanged, with its own saves.

## Removing

Double-click **`arcanum-ce-uninstall.bat`** in the game folder, or
**`uninstall.bat`** in this package's folder (it finds the game folder as
setup does). It then:

- asks whether to uninstall Arcanum CE;
- waits for Enter before closing.

## License

- **Arcanum CE is a modified version of Arcanum Community Edition** by
  Alexander Batalov (https://github.com/alexbatalov/arcanum-ce).
- Arcanum CE is under the Sustainable Use License (`LICENSE.md`).
- The libraries it is built with are listed in THIRD-PARTY-NOTICES.md.
- Arcanum is (c) Troika Games and Sierra. No game materials are contained in
  this package.

## What Arcanum CE changes

Arcanum CE is a rebuild of the game's engine from the original program, made
to play the same game. Where it differs, it is on purpose:

- **Bugs of the original, fixed:**
  - a mage who mastered a college was refused spells for "not enough
    Energy" they did have;
  - magic resistance from worn items was ignored in some spell checks;
  - a charm's reaction bonus could stick;
  - the player ran straight through obstacles when a plotted path met an
    item's owner;
  - quitting from inside a conversation crashed the game;
  - dragging items glitched;
  - several drawing errors.
- **Safe saving.** A failed or interrupted save no longer damages the save it
  was replacing.
- **The Unofficial Arcanum Patch's fixes**, all of them, and its four
  options:
  - levels up to 100;
  - the Master Educator;
  - no experience per hit (a kill gives its full worth instead);
  - no combat music.
- **Your own save folder, specific to CE** (see Saves, below).
- **Native on Linux and Windows**, using the High Resolution Patch's resolution,
  in a window or full screen.

Every change is listed in **CHANGES.md** (in the game folder,
arcanum-ce-CHANGES.md), with what each optional mod does.

## Optional mods

Three mods come with Arcanum CE.

- **Turning them on:** setup asks about each one. Afterwards each corresponds to one line
  in `arcanum-ce.cfg`.
  - Change its `0` to `1` and restart the game to turn it on.
  - Change it back to `0` to turn it off (needs restart).

**The three mods:**

- `mod combat-rebalance`, the **Combat Rebalance (preview)**:
  - **press Q to reload a gun.** Guns hold a few ready shots and must be
    reloaded; in real time you must stand still while loading;
  - reworked weapons and armor: armor that matters by location and
    penetration;
  - smarter opponents: pick weapons they can afford, cast spells, choose
    targets and drink potions;
  - criticals and fumbles, more variety for both;
  - magic costs, and spells that no longer stack;
  - poison per turn, and prowling.

  It is a preview: the balance is still being tuned.
- `mod real-time-pacing`, **Real-Time Pacing**: in real-time combat, every
  action takes the time its action points would take in turn-based combat.
  This mod aims to fix the infamous 'Harm' and high level bow issues
  during real time combat.
- `mod called-shots`, **Called Shots**: a panel beside the combat bar (Body,
  Head, Arms, Legs) for the aimed attacks Arcanum always had on the comma,
  period and slash keys.
  - Aimed attacks are harder to land, and more likely to leave a lasting
    effect when they are critical.

**Saves and mods:**

- **Keep the Combat Rebalance on for a save made with it.** The save
  remembers its items and rules, and loading it with the mod off can go
  wrong.
- Real-Time Pacing and Called Shots save nothing, so they can be turned on or
  off with any save.

## Saves

- Arcanum CE keeps its saves in `modules/Arcanum/SaveCE` in the game folder.
- The first time it starts, it copies your saves from the original game
  there, once.
- After that the two are separate:
  - CE's saves cannot be loaded by the original `Arcanum.exe`, because the
    save format differs;
  - the original keeps using its own `Save` folder, which CE never changes.

## Known limitations

- **A beta.** The engine has been tested a great deal. Please report anything odd.
- **No multiplayer.**
- **The Unofficial Arcanum Patch's other optional content** (the beta areas,
  the Riddlers) is untested with Arcanum CE.
- **The original's own quirks** are mostly kept on purpose; only those listed
  as fixed are changed, please see CHANGES.md for the curious.

## Reporting a problem

Open an issue at https://github.com/zsogg/arcanum-ce.combat/issues with (ideally):

1. **The log**, `arcanum-ce.log` in the game folder.
   - A new one is written each time the game starts, so copy it right after
     the problem, before starting the game again.
   - Its first line names your build.
2. **The save**, if the problem is in a game: the four files of its slot in
   `modules/Arcanum/SaveCE` (for example every `Slot0003...` file).
3. **What happened**, and what you did just before.
