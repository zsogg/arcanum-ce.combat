# Arcanum CE - release 1: the catalogue of changes
<!-- Generated from the project's release documents by sync-public-docs.py; edit those, not this copy. -->

> No pretensions to any originality in this work. My grateful acknowledgement
> to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, and Kababji - who introduced this lonely
> schoolboy to Arcanum all those years ago.

Arcanum CE is a rebuilt engine for *Arcanum: Of Steamworks and Magick
Obscura*.

- It runs over GOG's Arcanum 1.0.7.4 with the Unofficial Arcanum Patch (UAP)
  2.0.2 and its High Resolution Patch.
- The original's own quirks are kept on purpose. The ones a player can meet
  are listed in section 4.

**Contents:**
1. Always on: the original's bugs fixed, safety, running the game
2. The Unofficial Arcanum Patch
3. The optional mods: Combat Rebalance, Real-Time Pacing, Called Shots
4. The original's quirks, kept on purpose

## 1. Always on

### 1.1 Bugs of the original game, fixed

**Spells and magic**

- A mage who had mastered a college was refused spells for "Not enough
  Energy" they did have. The check now applies the same mastery discount as
  the charge.
- Spells interrupted by a Dweomer Shield now end cleanly.
- Spells cast just before a Reflection Shield went up now end cleanly.
- Magic resistance from worn items now counts in the three spell checks that
  ignored it.
- Charm's reaction bonus is removed exactly as it was applied; it could stick.

**Fatigue**

- Effects that set a character's fatigue adjustment now set it. The original
  wrote the current fatigue instead.
- A caster knocked unconscious by Teleport's fatigue cost now falls down,
  instead of staying on their feet.

**Walking**

- Walking a plotted path no longer breaks when the game checks whether you
  can see an item's owner. The original turned the player on the spot, so
  they ran straight through obstacles at the target.

**Crashes**

- Quitting from inside a conversation no longer crashes the game.
- The follower bar refreshing after its window has closed no longer crashes
  the game.

**Saving**

- A failed or interrupted save no longer damages the save it was replacing.
  Saves are written aside and committed whole.
- An interrupted save is rolled back at the next start.
- Saves survive a mod adding or removing a map area. In the original, adding
  one area broke every existing save.

**Prototypes**

- Prototypes the original looked up under one name and saved under another
  are no longer recreated at every start.

**Inventory**

- Dragging items no longer glitches or makes the cursor jump.
- A drag that ends outside the hotkey bar no longer leaves a slot marked as
  dragged.
- Stealing no longer flips between the inventory and paper-doll views.

**Menus and screens**

- The name field's text cursor sits where you type, not at the end of the
  field.
- A new character's name no longer sticks when you go back and forth.
- The schematics screen marks which list you are on (learned or found).
- The Options screen's empty slots use the right artwork, not a stretch of
  wood texture.
- The character editor uses its own background art (UAP makes the same
  correction).
- Reaction underlays on shrunk critters are the same size as the hover
  underlay.
- Teleport's destination click no longer delivers a stray mouse click.
- Waking from sleep redraws the world, ambient light included.

**Drawing**

- The "ghosts" over dead critters that do not decay draw correctly.
- Summoned creatures keep their own colours.
- The mouse cursor no longer leaves marks at the window's edge or outside
  it.

**Smaller fixes, with no ordinary effect on play**

- A turn-based timer was scaled a thousand times too long.
- A script call returned the script's number instead of its result.
- An animation timer used the wrong event type.
- A message check read the mouse button before the message type.
- The quantity roller broke on negative numbers.
- The character editor's plus and minus buttons were wired in a muddle
  (both reached the same skills, so play was unaffected).
- The debug log wedged after 1,024 characters.
- Map buffers had only their first entry cleared.
- The sound file cache did not search its own entries.
- Seeking to the end of a file was treated as an error.
- Some file reads compared a count of bytes with a count of items.

### 1.2 Safety by design

- **Damaged or unusual game files are refused, not trusted.** These no
  longer hang or crash the game:
  - prototypes, map object files and jump points with impossible values;
  - scripts with impossible entry counts or unsafe instructions;
  - message (`.mes`) files with a duplicated number (the original looped
    forever) or a very large number of entries (the original took minutes);
  - saves with impossible lists or sizes.
- **Leaks and uninitialized values** are fixed in more than a dozen places.
  Messages no longer print missing values, and the game exits with proper
  error codes.
- **`tig.dat` is required.** The original tried to open its own program file
  as a game archive when `tig.dat` was missing.
- **Saves carry no stray memory.** The original wrote 22 bytes of leftover
  memory into every save made in turn-based combat, and 4 beside every
  running animation. CE writes zeros, so the same game makes the same save on
  Linux and Windows.
- **The original's arithmetic, exactly.** CE is built so that numbers
  overflow the way the original's did, at any compiler optimization.

### 1.3 Running the game

**Platform**

- **Native on Linux and Windows**, through SDL3 instead of DirectX.
- **A resizable window or full screen**, at the High Resolution Patch's
  resolution.
- **Save thumbnails** are centred and smoothed at any resolution.
- **Left Alt** turns object highlighting on and off.

**The High Resolution Patch's settings**, read from `HighRes/config.ini` at
every start (the original's patch fixed them when it was installed):

- `Width` and `Height`: the screen size.
- `MenuPosition`: where the large panels sit (top, centre, bottom).
- `DialogFont`: larger dialogue text.
- `LogbookFont`: larger logbook text.
- `Options`: a compact Options panel.
- `SpriteMirroring`: mirrored critter frames (off while UAP's fixes are on,
  because UAP ships the real frames).
- `PreloadLimit`: how far from you NPCs keep acting.
- `BroadcastLimit`: how far a party command carries.
- **Not read:** `MapUI` (the patch's other world-map layout) and `MainMenuBG`
  (its full-screen main menu).

**CE's own files, beside the original's**

- **Its own saves**, in `modules/Arcanum/SaveCE`. The first start copies your
  original saves there, once. The original game and its `Save` folder are
  never changed, and CE's saves cannot be loaded by the original
  `Arcanum.exe`.
- **Its own settings**, in `arcanum-ce.cfg`. The original's `arcanum.cfg` is
  left alone.
- **Its own data folder**, `ce-data`, read above `data`. Mod files there are
  invisible to the original game.
- **A log**, `arcanum-ce.log`, written fresh at each start for bug reports.
  Its first line names the build.

**Tools**

- `arcanum-ce -check-install` reports on the installation.
- `arcanum-ce -check-install-fix` moves aside old loose prototype files that
  would hide the Unofficial Arcanum Patch's fixes. They can be moved back.

**Not supported**

- **Movies do not play on Linux.** The logos, the intro, the ending and the
  in-game cinematics are skipped.
- **Multiplayer** is not supported.

## 2. The Unofficial Arcanum Patch

All of UAP 2.0.2's fixes to the game program are part of Arcanum CE, and on.
(CE does not use UAP's patched `Arcanum.exe`; it makes each change itself.)

**Rules**

- **Throwing** costs 4 AP for everyone. In the original the player threw for
  free in turn-based combat, and NPCs paid 2.
- **Dragging an item** in combat costs 4 AP.
- **Resistances** of monsters, animals, undead and machines are capped at
  95%, like everyone else's.
- **Constitution 1** no longer means a heal rate of 0.
- **Ageing:** everyone ages a year every 360 days, and the age floor of 20 is
  gone.
- **Unspent character points** no longer cap at 56.
- **Half-elves** take the elven Beauty cap as well as the elven Dexterity
  cap.

**Followers**

- **Spend a follower's character points yourself:** turn off their automatic
  levelling. Their sheet opens editable, from the follower menu or from
  dialogue, with arrows to step between followers.
- **A ninth follower command** opens their schematics.

**Display**

- No "HT 0" over a blow that did nothing, or over a door or chest that took
  no damage. A hit that only cost fatigue still shows its "FT".
- Walking critters keep their shields (UAP supplies the art).
- Half-elf player characters use elven art, as half-elf NPCs always did.
- Monster armor art 4 shows as 6.
- Monsters and unique characters fall back to the ordinary NPC generic name.
- Skill messages show the original skill-group icons (combat, thieving,
  social, technological) instead of one generic icon.
- An item of neutral complexity gets the magickal targeting cursor.
- A **Shadows** control on the Options screen's Video tab.
- The version banner reads "Unofficial Patch 2.0.2", top right.
- The combat bar, the hotkey bar's background and the skill icons move by a
  pixel or two; the lens frames of the written-item and schematics screens
  match their backgrounds.

**Keys**

- **Quick Load is F9**, away from Quick Save on F7 (it was F8).
- Hotkeys and the number row do nothing while a dialog is open (the number
  keys pick dialogue options).
- The magick/tech button is checked again when a game is loaded.
- `-cheatmode` turns on Troika's cheat mode.

**Data**

- Effect numbers up to 1,000, for UAP's added effects.
- UAP's three added monster prototypes get their stats.

**UAP's four optional extras**, all on:

- **Levels up to 100** (the original stops at 50).
- **Master Educator:** the Educator background trains followers for any
  character. A follower one training step below you is raised to your level.
- **No experience per hit:** a kill gives its full experience instead.
- **No combat music.**

## 3. The optional mods

Three mods come with Arcanum CE, all off unless you turn them on.

- **Turning them on:** setup asks about each one. Afterwards each is one line
  in `arcanum-ce.cfg`: change its `0` to `1` and restart the game to turn it
  on, back to `0` to turn it off.
- **Saves:** keep the Combat Rebalance on for a save made with it. Real-Time
  Pacing and Called Shots save nothing, so they can be turned on or off with
  any save.

### 3.1 Combat Rebalance (preview) - `mod combat-rebalance`

A rework of combat, built and tuned piece by piece. **A preview:** the balance
is still being tuned.

**Press Q to reload a gun.** With the Combat Rebalance on, guns hold only a
few ready shots and must be reloaded.

**Firearms**

- **Reloading.** A gun holds ready shots, and **Q** reloads:
  - pistols and revolvers 6, rifles and muzzle-loaders 1, repeaters 7, heavy
    guns 2;
  - one shot at a time, though some guns refill a whole magazine;
  - in turn-based combat, unfinished loading carries over to the next turn;
  - in real time you stand still while loading;
  - followers and enemies reload, top up and switch to a melee backup too;
  - outside combat, guns load themselves.
- **Every gun retuned by its class** (all 41 firearms): damage, firing and
  loading cost, range and price.
  - Muzzle-loaders hit hard and load slowly.
  - Rifles and repeaters stay accurate further out.
  - Pistols are quick, and lose accuracy and penetration with distance.
  - The rare guns each have a role: the Acid Gun eats armor, the Tranquilizer
    Gun knocks out, and the Grenade Launcher's explosion lands.
- **Gun criticals by kind:** pistols critical often, rifles find gaps in
  armor, muzzle-loaders and heavy guns knock down.
- **Jams:** a firearm's fumble jams it. Clearing the jam costs reload work,
  and the gun keeps its rounds. (In the original, every fumble shot its
  shooter.)
- **An easier first shot** at a critter that has not noticed you.

**Weapons**

- **Swapping weapons** costs 4 AP.
- **Exertion:** you may act **1 AP short**, at a cost of 2 fatigue.
- **A weapon's on-hit spell** costs no AP. (In the original it was charged as
  a cast, so in turn-based the Grenade Launcher's explosion never landed.)
- **Bows** are quicker: 3 to 5 AP, 1 less from Apprentice.
- **The Throwing Dagger**, wielded and thrown, keeps its target and can hurt
  it. (In the original a wielded thrown weapon injured nobody.)
- **A throw under way** plays out.

**Armor**

- **Armor stops damage where it covers**, against each hit's penetration, on
  a curve: every weapon and 222 armor pieces carry their own values.
- **Criticals and called shots find gaps** in armor.
- **Shields block.**
- **Natural hide and magical protection** count as armor.
- **A blow the armor stops still tires the target:** a monster's partly, a
  mace's 40%, a staff's or club's 20%.

**Opponents**

- **They pick weapons they can use:** suited to your armor, and ones they can
  afford to swing. (In the original an NPC could pick a weapon that cost more
  than its turn, and never attack.)
- **Casters cast**, and summon (up to 3 creatures).
- **Targets are chosen better:** undead hold their ground, and hunters go for
  gunners and casters.
- **Followers on Defensive** fight within 8 tiles of you.
- **Potions:** an enemy drinks at most two a fight, and drinking costs 6 AP.

**Criticals**

- **More criticals.**
- **Each has a temporary effect by where it struck:** Winded, Numbed arm,
  Hobbled, Staggered or Dazzled. Injuries rarely last.
- **A full turn safe from another stun** after recovering from a stun,
  knockdown or knockout.

**Magic**

- **Per-spell costs:** 4 to 7 AP by power, 1 less in a mastered college.
- **The aptitude damage bonus** is 30% smaller.
- **Spell criticals:** damage spells add damage and a stun or knockdown;
  Weaken, Drain Will, Plague of Insects, Shrink and Congeal Time leave their
  target Exposed, Unfocused, Dazzled or Hobbled for two turns; other spells
  return their casting fatigue.
- **No stacking:** a second casting of a buff does not add to the first.
- **Summoning sickness:** a creature summoned in combat waits a turn before
  acting.

**The opening, poison and prowling**

- **The opening:** at most one enemy acts before your first turn.
- **Poison** ticks every second turn in combat (every 4 seconds in real
  time). In the original it ticked once in fifteen turn-based rounds.
- **Prowling:**
  - attacking from prowl ends it;
  - followers prowl with you only if they have some Prowling skill, and an
    attack on one reveals only that follower;
  - starting to prowl in combat needs Master training (prowling started
    before combat carries into it);
  - darkness helps against being seen, up to 30, unless you carry a light or
    the watcher sees in the dark;
  - open ground halves your prowling below Expert, instead of a flat 50
    penalty.

### 3.2 Real-Time Pacing - `mod real-time-pacing`

In real-time combat, **every action takes the time its action points would
take in turn-based combat.** Speed and the weapon or spell you choose then
matter in real time as they do turn by turn.

- **Timed:** attacks, throws and grenades, skills, and every spell.
- **The pace:** a whole turn is worth 2 seconds (the `real time pace ms`
  setting in `arcanum-ce.cfg`). At Speed 11:
  - a Bow fires about once a second, rather than twice;
  - Harm takes three-quarters of a second, rather than a click.
- **Never faster than its animation.**
- **Instant actions stay instant:** doors, containers, picking things up,
  plain items, pickpocketing.
- **Moving costs fighting time:** recovery pauses while you walk or run, as
  in turn-based (`real time recovery pauses while moving`).
- **A melee swing whose target stepped out of reach** costs nothing.
- **Chasing:** a melee enemy swings only at a target that has stopped, not at
  one running past.
- **Potions take time:** drinking one in combat means standing still for its
  cost, and the effect comes at the end.
- **Turn-based combat is unchanged.**

### 3.3 Called Shots - `mod called-shots`

- **A panel beside the combat bar:** Body, Head, Arms, Legs. Choose one, and
  your attacks aim there until you choose again or leave combat mode.
- **Where:** in turn-based and real-time combat, and while prowling. Outside
  combat mode the panel is hidden.
- **Arcanum's own aimed attacks:** holding comma (head), period (arms) or
  slash (legs) has always aimed an attack. The keys still work, and win over
  the panel while held.
- **Arcanum's own rules:** an aimed attack is harder to land (-50 for the
  head, -30 for a limb) and, when it is a critical, more likely to leave a
  lasting effect (+10). The panel changes no number.

### 3.4 Never turned on by setup

- `called shot throws`: aimed throws. Built, but in no mod and not yet
  playtested.
- `debug criticals`, `debug fumbles`, `debug character points`: test
  switches that force criticals, force fumbles and give free character
  points.
- `determinism`: a test setting for reproducible runs.

## 4. The original's quirks, kept on purpose

CE keeps the original's behaviour unless this catalogue says otherwise. The
source records over 500 places where the original does something odd and CE
does the same. These are the ones a player can meet.

### 4.1 Kept in every configuration

**Combat**

- **The called-shot cursor shows the wrong limb:** aiming at the arms shows
  the leg cursor, and the legs the arm cursor. The attack goes where you
  aimed; only the cursor is crossed.
- **The Grenade Launcher's explosion can hit you and your followers**: you
  when you stand within 2 tiles of the target, and a follower near it. Do not
  fire it at anyone beside you or a companion.
- **Every melee, bow and thrown fumble hits its wielder**, half the time
  critically. (With the Combat Rebalance, a gun's fumble jams it instead.)
- **A fumble's critical effects are rolled twice** on the wielder.
- **Dodge is rolled after every hit**, by the target, at twice their Dodge.
- **Companions do not drink potions out of combat.**

**Magic**

- **Mirror reflects no damage.**
- **Magickal aptitude never lengthens a spell.** Only college mastery
  doubles a spell's duration.
- **Maintained spells are not ended for range**, and NPC casters count no
  maintained spells, so they may choose one the game will not keep up.
- **Spells are learned on Willpower.** The "IQ" a spell lists is checked
  against Willpower, and every spell's Intelligence minimum is 5.

**Items and characters**

- **Hexed items can always be taken off.** The "cannot remove a hexed item"
  message is never shown.
- **Heavy encumbrance never stops you running.**
- **Cone-shaped targeting** covers a square around its origin, not a cone.

**Saves**

- **Overwriting a save under a new description** leaves the old description
  behind, so the slot appears twice in the save and load lists. Both entries
  open the same game.

### 4.2 Kept unless an optional mod is on

These are as the original has them in a default install. The mod named
changes each one (section 3).

- **A wielded thrown weapon injures nobody**: in shipped data, the Throwing
  Dagger. Combat Rebalance.
- **A weapon's on-hit spell is charged as a cast**, so in turn-based combat
  the Grenade Launcher's explosion never lands. Combat Rebalance.
- **An NPC may wield a weapon that costs more than its turn**, and then never
  attacks in turn-based combat. Combat Rebalance.
- **Every firearm fumble shoots its wielder.** Combat Rebalance.
- **Poison ticks once in fifteen turn-based rounds.** Combat Rebalance.
- **Prowling:** anyone can prowl in combat, followers crouch whatever their
  skill, and open ground costs a flat 50. Combat Rebalance.
- **In real time, actions repeat as fast as their animations**, so a fast
  click outpaces the turn-based cost. Real-Time Pacing.
- **A melee enemy chasing you swings at the air** as you run past. Real-Time
  Pacing (in part).
- **Aimed attacks are only on the comma, period and slash keys.** Called
  Shots.
