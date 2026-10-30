# Arcanum CE for macOS (untested)
<!-- Generated from the project's release documents by sync-public-docs.py; edit those, not this copy. -->

> No pretensions to any originality in this work.
> Coding was done nearly entirely using LLM tools, Claude, Astra and Deepseek.
>
> **My grateful acknowledgement to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, the Terra-Arcanum team, and Kababji, who introduced me to this game's
> demo disc all those years ago.**

Arcanum CE runs *Arcanum: Of Steamworks and Magick Obscura* natively on
macOS, as one app for Apple Silicon and Intel Macs.

- **Untested.** This app is built on GitHub's macOS machines from the same
  source as the Windows and Linux packages, and nobody making this release
  has run it on a Mac. Please report what happens, good or bad ("Reporting
  a problem", below).
- It builds upon the Unofficial Arcanum Patch (UAP) 2.0.2 and its High
  Resolution Patch.
- This package contains Arcanum CE only: you supply the game.
- No Wine is used.

## What you need

- **Arcanum from GOG**: the offline installer, `setup_arcanum_*.exe`, from
  your GOG library (and its `.bin` file, if GOG gives you one).
- **The Unofficial Arcanum Patch 2.0.2**: `UAP2.0.2.exe`.
- **innoextract** and **7-Zip**, from Homebrew (https://brew.sh):
  `brew install innoextract sevenzip`
- macOS 10.13 or later, on Apple Silicon or Intel.
- About 2 GB free.

## Installing

In Terminal:

```console
tar xf arcanum-ce-*-macos.tar.gz
cd arcanum-ce-*-macos
./setup.sh --gog ~/Downloads/setup_arcanum_*.exe --uap ~/Downloads/UAP2.0.2.exe
```

Setup then:

- asks yes or no for each of the nine optional mods, from Called Shots to
  Encounter Variety ("Optional mods" below lists them). The answer is no
  unless you say yes;
- unpacks both installers;
- lays down UAP's standard components and the High Resolution Patch;
- puts **Arcanum Community Edition.app** in the game folder, beside the game
  files, and checks the result. The app must stay in that folder: it reads
  the game from the folder it is in.

**Options:**

- `--dir FOLDER` installs into a new or empty folder of your choice. The
  default is `~/Games/arcanum-ce`.
- `--all-mods` turns every optional mod on without asking.
- `--mod combat-rebalance` turns on one (you can repeat it).

## The first launch

The app is signed ad hoc, not notarized by Apple, so macOS does not know who
made it. Setup clears the app's quarantine flag, which is usually enough. If
macOS still refuses to open it:

- open **System Settings > Privacy & Security**, scroll to the message about
  Arcanum Community Edition, and choose **Open Anyway**; or
- in Terminal: `xattr -dr com.apple.quarantine ~/Games/arcanum-ce/"Arcanum Community Edition.app"`

If the game quits at once or says files are missing, the app was probably run
from a copy macOS made elsewhere. Clear the flag as above and open it from
the game folder.

## Playing

- Open **Arcanum Community Edition** in `~/Games/arcanum-ce`. You can drag
  it to the Dock; keep the app itself in the game folder.
- From Terminal, with output and in a window:
  `~/Games/arcanum-ce/"Arcanum Community Edition.app"/Contents/MacOS/* -window`
- Settings are in `arcanum-ce.cfg`.
- The screen size and the High Resolution Patch's options are in
  `HighRes/config.ini`.
- The same program with `-check-install` reports on the installation.
- Movies do not play on macOS.

## Removing

```console
~/Games/arcanum-ce/arcanum-ce-uninstall.sh          # Arcanum CE only; the game and saves stay
~/Games/arcanum-ce/arcanum-ce-uninstall.sh --all    # the whole folder, saves included
```

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
- **The Unofficial Arcanum Patch's fixes**, all of them, and three of its
  options:
  - levels up to 100;
  - the Master Educator;
  - no experience per hit (a kill gives its full worth instead).
- **Varied combat music.** A fight plays the game's area music in random
  order instead of looping one track.
- **Your own save folder, specific to CE** (see Saves, below).
- **Native on Linux and Windows**, using the High Resolution Patch's resolution,
  in a window or full screen.

Every change is listed in **CHANGES.md** (in the game folder,
arcanum-ce-CHANGES.md), with what each optional mod does.

## Optional mods

Nine mods come with Arcanum CE, all off unless you turn them on.

- **Turning them on:** setup asks about each one. Afterwards each corresponds to one line
  in `arcanum-ce.cfg`.
  - Change its `0` to `1` and restart the game to turn it on.
  - Change it back to `0` to turn it off (needs restart).

**The combat mods:**

- `mod combat-rebalance`, the **Combat Rebalance (preview)**:
  - **press Q to reload a gun.** Guns hold a few ready shots and must be
    reloaded; in real time you must stand still while loading;
  - reworked weapons and armor: armor that matters by location and
    penetration;
  - smarter opponents: pick weapons they can afford, cast spells, choose
    targets and drink potions;
  - followers who heal the worst hurt first, cast while the party is
    healthy, and hold their orders and their gear;
  - grenades that hurt everyone near the blast;
  - sight and hearing: walls stop sound, enemies see less to the sides;
  - criticals and fumbles, more variety for both;
  - magic costs, and spells that no longer stack;
  - poison that bites in a fight and wears off after it; prowling and
    backstab reined in.

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

**The other mods** (new in this release; apart from Starting Gold, not yet
tested in a game):

- `mod followers`, **Followers**: alignment and level no longer stop an NPC
  joining you or make a follower leave, and low reaction no longer stops one
  joining.
- `mod initiative-awareness`, **Initiative and Awareness**: turn-based fights
  in initiative order, random encounters you can spot and avoid, and
  ambushes in which at most one enemy acts first.
- `mod better-shops`, **Better Shops**: schematics in stock more often,
  specialist components sold and easier to use, and Conjure Spirit scrolls
  in every magic shop.
- `mod starting-gold`, **Starting Gold**: a new character starts with three
  times the gold.
- `mod brighter-lanterns`, **Brighter Lanterns**: carried lights light a
  wider, brighter circle.
- `mod encounter-variety`, **Encounter Variety**: an experimental draft with
  more frequent and more varied random encounters.

**Saves and mods:**

- **Keep the Combat Rebalance on for a save made with it.** The save
  remembers its items and rules, and loading it with the mod off can go
  wrong.
- Every other mod can be turned on or off with any save.

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
- **macOS is untested.** The macOS package is built on GitHub's machines and
  nobody making this release has run it; please report what happens.
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
4. **On a Mac**, the macOS version, the chip (Apple Silicon or Intel), and
   the crash report if there is one (Console, or
   `~/Library/Logs/DiagnosticReports`).
