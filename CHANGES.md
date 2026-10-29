# Arcanum CE - the catalogue of changes
<!-- Generated from the project's release documents by sync-public-docs.py; edit those, not this copy. -->

> No pretensions to any originality in this work.
> Coding was done nearly entirely using LLM tools, Claude, Astra and Deepseek.
>
> **My grateful acknowledgement to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, the Terra-Arcanum team, and Kababji - who introduced this lonely
> schoolboy to Arcanum all those years ago.**

Arcanum CE is a rebuilt engine for *Arcanum: Of Steamworks and Magick
Obscura*.

- It runs over GOG's Arcanum 1.0.7.4 with the Unofficial Arcanum Patch (UAP)
  2.0.2 and its High Resolution Patch.
- It is built on Arcanum Community Edition
  (github.com/alexbatalov/arcanum-ce), a decompilation of the original game.
  The bug fixes come in two kinds, kept apart below:
  - **bugs of the original 2001 game**, which CE fixes (section 3.1);
  - **mistakes in the decompilation**, places where its code did not do what
    the original game does. These are put right so that the game plays as
    the original (sections 3.2 and 3.3).
- The original's own quirks are kept on purpose. The ones a player can meet
  are listed in section 6.
- Release 1 came out on 2026-10-18. Everything marked "in release 1" below
  is what that release did; the rest of this catalogue is what the game does
  now.

**Contents:**
1. The combat mods: Combat Rebalance, Real-Time Pacing, Called Shots
2. Highlights: the bugs worth a story, and the quirks left alone
3. Bug fixes: the original game's bugs, the decompilation's mistakes, safety
4. Changes to game mechanics: the Unofficial Arcanum Patch, CE's own rules,
   mods built since release 1
5. Running the game
6. The original's quirks, kept on purpose

Within each category, changes are listed from the largest effect on play to
the smallest. A number such as (#58) is the fix's entry in the project's
ledger of decompilation mistakes.

## 1. The combat mods

Three optional mods come with Arcanum CE, all off unless you turn them on.

- **Turning them on:** setup asks about each one. Afterwards each is one line
  in `arcanum-ce.cfg`: change its `0` to `1` and restart the game to turn it
  on, back to `0` to turn it off.
- **Saves:** keep the Combat Rebalance on for a save made with it. Real-Time
  Pacing and Called Shots save nothing, so they can be turned on or off with
  any save.

### 1.1 Combat Rebalance (preview) - `mod combat-rebalance`

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
- **The ammunition counter shows the gun's state** (new since release 1):
  ready shots over capacity, then spare rounds, such as `3/7 24`, or
  `JAM 24` while the gun is jammed.
- **Gun criticals by kind:** pistols critical often, rifles find gaps in
  armor, muzzle-loaders and heavy guns knock down.
- **Jams:** a firearm's fumble jams it. Clearing the jam costs reload work,
  and the gun keeps its rounds. (In the original, every fumble shot its
  shooter.)
- **An easier first shot** at a critter that has not noticed you.

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

**Creatures** (new since release 1)

- **A critical hit does what suits the body it lands on.** Six kinds of body:
  living, skeletal undead, fleshy undead, mechanical, spirit and phantom. The
  original allowed nothing at all for shapeless creatures and only a
  knockdown for undead.
- **Phantoms are hurt only by critical hits, spells, explosions and traps.**
  Ordinary blows pass through them; a weapon's on-hit spell still goes off.
  Hovering over a phantom tells you what hurts it, in place of its reaction.
- **Opponents look for another way against a phantom**, such as a spell,
  rather than swinging uselessly.
- **A crushing critical on a skeleton** does 10 more damage.
- **The creature sheet** sets named creatures' body type, natural protection
  (in place of the usual armour calculation) and resistance to gunfire.

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

**Traps** (new since release 1)

- **Traps are easier to see.** Spot Trap now rises on the same curve as
  Melee and Bow: 75% at 10 skill points, where the original gave 40%.
- **Training sees without rolling:** an Apprentice always sees an ordinary
  trap of the easiest kind, and an Expert a magical one too.
- **The party's best spotter looks ahead** three tiles along your path, once
  per trap, and checks a trapped door or chest for whoever opens it. The
  message names who saw it.
- **A trap's own difficulty makes it harder to spot.** The original counted
  it only against disarming.

**Weapons**

- **Swapping weapons** costs 4 AP.
- **Exertion:** you may act **1 AP short**, at a cost of 2 fatigue. The
  action bar shows such an action's knob in yellow, with "+2 fatigue" above
  it (new since release 1).
- **A throw you cannot pay for is refused** before it starts, and the item
  stays in your pack (new since release 1). Before, at 1 or 2 AP every
  grenade you carried could be thrown for free.
- **A weapon's on-hit spell** costs no AP. (In the original it was charged as
  a cast, so in turn-based the Grenade Launcher's explosion never landed.)
- **Bows** are quicker: 3 to 5 AP, 1 less from Apprentice.
- **The Throwing Dagger**, wielded and thrown, keeps its target and can hurt
  it. (In the original a wielded thrown weapon injured nobody.)
- **A throw under way** plays out.

**The opening, poison and prowling**

- **The opening:** at most one enemy acts before your first turn.
- **Poison** ticks every turn in combat (every 2 seconds in real time), for
  1 plus a hundredth of the poison level, so it bites during the fight. In
  the original it ticked once in fifteen turn-based rounds, for 1 to 3. (New
  since release 1; release 1 ticked every second turn.)
- **Poison wears off quickly after a fight** (new since release 1): out of
  combat each tick lowers the poison level by your Constitution. In the
  original a poisoning carried on for minutes after the fight, often for
  more damage than the fight itself.
- **A poisoned hit raises poison to at most four times its own dose** (new
  since release 1), so many weak bites no longer build into a deadly dose.
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

**Fixes to the mod itself since release 1**

- Enemies' memory of who is desperate, armoured or carrying potions forgets
  the dead. After 64 NPCs in one session, enemies had begun to share one
  record and make odd choices.
- Poison that ticks per turn frees the record of dead or cured critters, so a
  long fight can no longer stop poison working.
- With potion costs on and exertion off, the action bar no longer offers a
  potion you cannot afford and then refuses the drink.

### 1.2 Real-Time Pacing - `mod real-time-pacing`

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

### 1.3 Called Shots - `mod called-shots`

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

## 2. Highlights

**Mistakes in the decompilation, now fixed**

- **Random encounters were always the same.** Each encounter table only ever
  produced its first line, so whole groups, such as the Malochian Hand's
  ruffians, never appeared. (#58)
- **Wall of Fire burned its own side.** Wall of Fire, Squall of Ice and the
  fire grenade hurt the caster's party instead of the enemies. (#65)
- **Every effect multiplier was zero**, and carrying the Flow Reaper crashed
  the game. (#60)
- **The 2001 patch never applied.** The fixes that Troika's own patches make
  to world objects (NPC scripts, names, training, Myrth's necklace) were
  never loaded. (#298)
- **A follower with followers froze the game.** A summoner's summons or a
  recruited leader's men sent the party list round in circles for ever.
  (#177)
- **Healing crippled legs cured blindness** and left the legs crippled.
  (#101)
- **The moon ran fast.** A mistyped day length (84,600 seconds instead of
  86,400) put the moon about 14 hours ahead every lunar month. (#273)
- **Free goods.** A merchant's item dropped outside the trade window landed
  on the floor, yours for nothing. (#229)
- **Every script division added instead.** Each division in every shipped
  script returned the sum of the two numbers. (#45)
- **Dodge was worth five points a level** instead of two, so a skilled
  dodger avoided nearly every hit. (#49)
- **Stunned enemies kept fighting.** A stun barely held them. (#77)
- **The dice game vanished.** Gamblers could never offer "Roll the dice".
  (#70)
**Bugs of the original game, now fixed**

- **A failed save could destroy the one it replaced.** A failed or
  interrupted save damaged the save it was overwriting. CE writes saves
  aside and commits them whole.
- **Quitting during a conversation crashed the game.**
- **Walking through walls.** Walking a plotted path while the game checked
  whether you could see an item's owner turned the player on the spot, so
  they ran straight through obstacles.
- **Masters were refused spells they could afford.** The Energy check forgot
  the mastery discount that the charge applied.
- **Charm could leave a permanent mark.** Its reaction bonus could stick: a
  gnome gained 10 for every charm.
- **Spell glows could stay on for good** after an earlier effect was
  removed.

**Quirks of the original, kept on purpose**

- **The called-shot cursor shows the wrong limb.** Aim at the arms and you
  see the leg cursor. The blow still goes where you aimed.
- **The Grenade Launcher's explosion can hit you** and anyone beside you.
- **Every melee, bow and thrown fumble hits its wielder**, half the time
  critically.
- **Everyone swings high at dwarves and halflings**, because every character
  counts as human-sized.
- **Mirror reflects no damage.**
- **Hexed items can always be taken off.**
- **Two ghouls and a soul are not undead** to the engine: the Savage Ghoul,
  the Stygian Ghoul and the Ensanguine Soul.
- **Spells are learned on Willpower**, whatever the spell list says about
  Intelligence.

## 3. Bug fixes

### 3.1 Bugs of the original game, fixed

These were wrong in the original 2001 game itself. CE fixes them, always.

**Saving**

- A failed or interrupted save no longer damages the save it was replacing.
  Saves are written aside and committed whole.
- An interrupted save is rolled back at the next start.
- A save that could not be fully written, for example on a full disk, is
  reported as failed and the old save is kept. The original said "saved"
  while some map areas kept their older state.
- Saves survive a mod adding or removing a map area. In the original, adding
  one area broke every existing save.

**Crashes**

- Quitting from inside a conversation no longer crashes the game.
- The follower bar refreshing after its window has closed no longer crashes
  the game.

**Spells and magic**

- A mage who had mastered a college was refused spells for "Not enough
  Energy" they did have. The check now applies the same mastery discount as
  the charge.
- Spell glows and other effects come off a character when they end. The
  original could leave one stuck on after an earlier effect was removed.
- Spells interrupted by a Dweomer Shield now end cleanly.
- Spells cast just before a Reflection Shield went up now end cleanly.
- Magic resistance from worn items now counts in the three spell checks that
  ignored it.
- Charm's reaction bonus is removed exactly as it was applied. In the
  original it could stick for good: a gnome gained 10 per charm.
- Teleport's exit picks the map area the caster is really standing in, not a
  neighbouring one.

**Fatigue**

- Effects that set a character's fatigue adjustment now set it. The original
  wrote the current fatigue instead.
- A caster knocked unconscious by Teleport's fatigue cost now falls down,
  instead of staying on their feet.

**Walking**

- Walking a plotted path no longer breaks when the game checks whether you
  can see an item's owner. The original turned the player on the spot, so
  they ran straight through obstacles at the target.
- Paths that stop beside a target no longer pick odd routes from a leftover
  cost.

**Inventory**

- Dragging items no longer glitches or makes the cursor jump.
- A drag that ends outside the hotkey bar no longer leaves a slot marked as
  dragged.
- Stealing no longer flips between the inventory and paper-doll views.

**Prototypes**

- Prototypes the original looked up under one name and saved under another
  are no longer recreated at every start.

**Menus and screens**

- The name field's text cursor sits where you type, not at the end of the
  field.
- A new character's name no longer sticks when you go back and forth.
- The schematics screen marks which list you are on (learned or found).
- World-map notes from the ninth on are shown or hidden by their own type.
- The Options screen's empty slots use the right artwork, not a stretch of
  wood texture.
- The character editor uses its own background art (UAP makes the same
  correction).
- Books and notes meet the interface bar exactly; the original left a
  one-pixel gap.
- Reaction underlays on shrunk critters are the same size as the hover
  underlay.
- Teleport's destination click no longer delivers a stray mouse click.
- Waking from sleep redraws the world, ambient light included.

**Drawing**

- The "ghosts" over dead critters that do not decay draw correctly.
- Summoned creatures keep their own colours.
- An effect's light is added once, not twice or not at all.

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
- Three basic NPC prototypes start with clean script counters; the original
  stored leftover memory there.
- A module that adds its own object or faction names leaves the existing
  names intact; the original could garble them.
- An effect overlay on its first frame keeps its art; the original could
  draw it corrupted.
- A character with a negative portrait number gets the default portrait
  instead of garbage.

### 3.2 Mistakes in the decompilation, fixed since release 1

Each of these was a place where the code CE is built on did not do what the
original game does. From 2026-10-20 to 10-24 every function was compared with
the original's, one by one, and these are what that found. All are fixed:
the game now plays as the original here.

**Combat**

- Stunned enemies now actually lose their turns; in release 1 a stun barely
  held them and they kept acting. (#77)
- A crippling arm critical now makes the victim drop both weapon and shield;
  in release 1 it tried to drop the weapon twice and the shield stayed on.
  (#61)
- Points in Backstab now let you backstab without Apprentice training; in
  release 1 you needed the training. (#94)
- Healing crippled legs now cures the legs; in release 1 it cured blindness
  instead and left the legs crippled. (#101)
- A crushing blow to the head can no longer blind; in release 1 it could when
  the helmet stayed on or there was none. (#103)
- In turn-based combat your followers act after the enemies again; in release
  1 they were mixed in among them. (#106)
- Invisible NPCs now become visible when they attack, while an invisible
  player stays hidden; release 1 had it backwards, revealing only the player
  on every attack. (#143)
- A stunned, paralysed or petrified player can no longer act by clicking
  targets; in release 1 being stuck only stopped you if you were both
  petrified and paralysed. (#266)
- While aiming, you can now click through corpses to whatever lies beneath; in
  release 1 bodies always got in the way, and each aim wasted memory. (#306)
- Disarmed NPCs now pick up the nearest weapon; in release 1 they walked off
  to the farthest one in sight. (#79)
- NPCs who decide to back away in combat now do so at once; in release 1 they
  waited a pass, a whole turn in turn-based play. (#84)
- Armour with special critical-hit behaviour now reacts to critical hits and
  misses; in release 1 those reactions never ran. (#100)
- A stun's knockout chance is back to the original odds; in release 1 it was
  one point lower. (#102)
- A blow that only tires the target now counts as a hit and plays its hit
  effects; in release 1 it did not. (#105)
- Attack damage lines now show "0" only when both the lowest and highest
  damage are zero; in release 1 only the highest was checked. (#275)

**Spells and magic**

- Effects that multiply or divide now work, and carrying the Flow Reaper no
  longer crashes the game; in release 1 every multiplier was zero. (#60)
- Wall of Fire, Squall of Ice and the fire grenade now hurt enemies and spare
  their caster's party; in release 1 they burned the caster instead. (#65)
- Wall of Fire, Wall of Stone and Wall of Force are now five tiles long and
  lie properly on diagonals; in release 1 they were only three tiles. (#302)
- The discount for mastering a college of magic is back to the original
  rounding; in release 1 a one-point spell became free. (#114)
- Dispelling Stone now turns the victim back to normal colour; in release 1 it
  stayed grey. (#115)
- Only very hard locks now resist a weak caster's unlocking magic; in release
  1 any lock could. (#116)
- Knockback spells now push their target back; in release 1 every knockback
  was blocked on its first step. (#129)
- Elementals and a few other monsters built fresh by CE now get their innate
  magical traits; in release 1 those 19 creatures were generated without them.
  (#283)
- A spell now stops when its target is destroyed and finds its target again
  after loading; in release 1 it could carry on. (#117)

**World, maps and travel**

- Random encounters now draw from every line of their tables; in release 1
  each table only ever produced its first encounter, so the Malochian Hand
  assassins never appeared. (#58)
- Innkeepers and inn beds no longer quietly block or unblock a map area each
  time they check the hour, as they did in release 1. (#63)
- A new world map waypoint now snaps to a known place nearby before the route
  is planned; in release 1 routes went to the exact clicked spot. (#240)
- Area markers on the continent map now appear at their own places; in release
  1 they could be drawn at the last note's position or not placed at all.
  (#244)
- Shooting traps now find their source near the person who set them off, as in
  the original; release 1 looked near the trap. (#113)
- A trap that has been broken now counts as broken; in release 1 it never did.
  (#118)
- On the world map, Z now does nothing while travelling and switches to an
  open town map from the continent view; in release 1 these were mixed up.
  (#237)
- Pressing Delete on the world map now removes the selected waypoint, or the
  last one; in release 1 the key did nothing. (#238)
- Dragging the world map stops scrolling when the mouse leaves the map; in
  release 1 it kept scrolling. (#239)
- Waypoint markers on the world map are now exactly their drawn size to click;
  in release 1 they reached one pixel too far right and down. (#242)
- The world map's "nearest known place" now ignores a place exactly at the
  edge of its range, as the original does; release 1 counted it. (#319)

**Dialogue, quests and scripts**

- Scripts that ask about a skill, including every gambling conversation, now
  just ask; in release 1 they went on to cast a stray spell. (#62)
- A character short of money now sees the "I can't afford that" replies again;
  in release 1 they were always hidden, across 69 conversations. (#66)
- The "Roll the dice" options with gamblers such as Killian Drake appear
  again; in release 1 the dice game could never be offered. (#70)
- The original 2001 patch fixes for world objects now apply: NPC scripts,
  names, training and Myrth's necklace; release 1 never applied any of them.
  (#298)
- When a hopbot, spider-bot or automaton is turned into an item it is handled
  cleanly; in release 1 the change could disturb an unrelated object in the
  world. (#64)
- Dialogue lines that check your character's level now work; in release 1 they
  always failed (seen with the Boil guard). (#67)
- NPCs who tell rumours now pick from their whole set; in release 1 they
  repeated only the first rumour of each set after the first telling. (#68)
- NPCs who mention themselves by name now say their own name; in release 1
  they said yours, as in "Thank you" followed by your name. (#69)
- Scripts that set how much an NPC likes you now set the intended amount; in
  release 1 they used an unrelated number. (#71)
- NPCs who greet you by reputation now name one you actually have; in release
  1 they could pick an unrelated one. (#97)
- A map event meant to fire once now removes itself as intended; in release 1
  it fired again every time you entered that area. (#303)
- A conversation started by a script now opens with the NPC's correct story
  state; in release 1 its first screen could show the wrong options. (#72)
- Scripts that ask what kind of weapon you hold now get the right answer; in
  release 1 they read the wrong picture and could misjudge it (three scripts).
  (#73)
- Male NPCs now tell their stories in the wording meant for your character's
  gender; in release 1 the male and female versions were swapped. (#74)
- When you cannot pay alone, your followers now chip in only what is still
  owed; in release 1 each was asked for the full remainder. (#75)
- A newspaper story that has already run can now be printed again when the
  game asks for it; in release 1 it was silently skipped. (#296)

**Characters, skills and followers**

- Your reputations now change how NPCs react to you; in release 1 the
  reputation file was misread and they had no effect. (#96)
- A follower told to wait now waits for a charismatic leader and eventually
  gives up on an ordinary one; in release 1 it was the other way round. (#80)
- Followers now judge an enemy's strength by the original level bands; in
  release 1 they treated any equal-level foe as stronger and cast spells at
  it. (#81)
- A master of Persuasion can no longer recruit beyond the follower limit set
  by Charisma, as was possible in release 1. (#82)
- Repair now reports success whenever it repairs something; in release 1 a
  failed roll still repaired but said it failed, and a master's fumble cost
  durability. (#89)
- Heal again needs a healing item such as bandages or a kit; in release 1 it
  worked with nothing in hand. (#90)
- Auto-levelled characters now raise stats to their planned targets; in
  release 1 race, background and other bonuses made them stop early or
  overshoot. (#91)
- Killing or robbing good people can now drive your alignment as low as the
  original allows; in release 1 it stopped well short. (#93)
- When a follower volunteers to pick a lock, the follower now does the
  picking; in release 1 the player character picked it instead. (#216)
- Gambling for worn items or items a merchant won't sell now checks your own
  gambling training; in release 1 it checked the merchant's. (#262)
- Outside combat, followers now refuse an order to attack one of your own
  party; in release 1 they always obeyed and turned on their companions.
  (#299)
- Light levels now match the original, so darkness penalties to skills and
  movement are right; release 1 could judge a spot one level brighter. (#347)
- A follower who leaves now says its leaving line; in release 1 it spoke a
  warning line instead. (#83)
- The original rule that undead or mechanical followers cannot use Heal is
  restored as an option; release settings keep the release 1 rule. (#85)
- An unconscious player can no longer start using a skill, as release 1
  allowed. (#98)
- The "skill already at its minimum" message now appears on the character
  screen when it should; in release 1 the condition was reversed. (#235)
- A sleeping character's spoken commands now come out as sleepy mumbling; in
  release 1 they were said, and obeyed, word for word. (#322)

**Crashes and hangs**

- Having a follower who has followers of their own no longer freezes the game;
  in release 1 building the party list looped forever. (#177)
- The map-area cache now shifts only its live entries when it drops one;
  release 1 moved too many, eventually spilling past its memory as the cache
  filled. (#190)
- Skipping a movie with Shift, Ctrl, an arrow or an F-key is now safe; in
  release 1 it could crash the game, and Escape waited on X. (#369)
- If the roof art list fails to load, the game no longer reads from an empty
  table, which could crash; release 1 kept a stale roof count. (#172)

**Saving and loading**

- Switching to a different game module without restarting now unloads the old
  one's files; release 1 kept the previous module's archives loaded underneath
  the new one. (#196)
- Objects in changed wilderness areas now load where they belong; in release 1
  they could be placed near the corner of the world instead. (#305)
- A map whose saved list of moving objects is cut short now fails to load, as
  originally; release 1 loaded the damaged data as if it were good. (#157)
- The object consistency check now repairs a mismatched inventory count from
  the actual item list and rejects broken entries; release 1 trusted the wrong
  count and kept them. (#175)
- When part of a changed object fails to load, the game now finishes reading
  that record before giving up; release 1 stopped in the middle of it. (#180)
- Saved games of an incompatible version are now refused instead of loaded;
  release 1 loaded any version and could leave the file open on a failed read.
  (#195)
- Saves no longer record every light in outdoor areas as changed; in release 1
  they were written again into every save, making saves bigger. (#289)
- Moving between maps no longer adds your character and followers again to the
  old map's list of removed objects on every trip. (#292)
- Saves no longer write unchanged map scripts again after a reload; release 1
  kept re-saving them, making save data larger for no reason. (#295)
- A save with a damaged file index now fails to load cleanly; release 1 could
  load it anyway and carry on with bad data. (#328)
- Removing a game folder now applies to every data folder the game uses;
  release 1 removed it from the first one only, or from the wrong place.
  (#330)

**Items, inventory and trade**

- Nice Clothes and similar effects now make NPCs like you more; in release 1
  the effect was read from the NPC and did nothing. (#95)
- Items that cannot be dropped can no longer be stolen, traded or moved by
  scripts; release 1 allowed it. (#108)
- Smashing or destroying an unopened chest now spills its contents; in release
  1 some such chests left nothing. (#109)
- A merchant's goods dropped outside the trade window now go back to the
  merchant; in release 1 they landed on the floor, yours for nothing. (#229)
- Throwing an item straight from the inventory works again; in release 1 the
  item fell off the cursor and the throw never happened. (#230)
- A quoted trade price is now forgotten once the deal ends; in release 1 it
  could carry over to the next item you dragged. (#265)
- Examining an item now keeps its effect description; in release 1 the skill
  bonus line overwrote it. (#276)
- Examined magic items now show their remaining charges as a number, and "+"
  otherwise; in release 1 the two were swapped. (#277)
- An item's "Uses" line now shows the uses left; in release 1 it showed the
  item's skill bonus instead. (#278)
- A potion with no spell is now used up whoever drinks it; in release 1 only
  the player's was used up. (#110)
- A character with even one crippled arm can no longer take up a shield, as
  release 1 allowed. (#111)
- Picking up an item now checks the item is still there; release 1 checked the
  person picking it up instead. (#120)
- Dead or unconscious characters can no longer use items; release 1 stopped
  only owners that were switched off yet alive and conscious. (#215)
- If the item you are wearing cannot be removed, the item you dragged onto it
  now returns to your pack; in release 1 it stayed stuck on the cursor. (#231)
- Asking for a free service or item no longer gets the "not enough money"
  reply; in release 1 the merchant complained about money anyway. (#232)
- When an item cannot be moved, the reason is now shown to you; in release 1
  the message never appeared. (#257)
- Repairing an inventory with a duplicated item now removes only the
  duplicate; in release 1 it could also remove a genuine item. (#263)
- That same inventory repair now tidies the owner's pack rather than the item
  being dropped (rare case). (#264)
- A hotkey whose item has been replaced by a matching one now throws and drags
  based on the hotkeyed item, as in the original game. (#272)

**Movement and animation**

- Click-to-move now stops at whatever blocks the path; in release 1 your
  character walked into blockers. (#128)
- Characters closing in during combat no longer drop an extra step of their
  plan; release 1 discarded one step too many, which could cancel what came
  next. (#141)
- NPCs asked to step aside for the player now swap places as the original
  does; in release 1 they often refused. (#122)
- Characters at one particular movement speed now animate at the intended
  pace; in release 1 they used the art's own rate. (#123)
- Your character now fidgets when idle, like everyone else; in release 1 the
  player never fidgeted. (#124)
- Approaching an enemy in combat now handles the move as the original does;
  release 1 changed the wrong setting on each approach. (#125)
- A knocked-down character no longer stands up while inactive or off its turn
  in turn-based play, as release 1 allowed. (#126)
- A move ending next to a living creature now stops when it reaches it; in
  release 1 the step went on to further checks. (#130)
- The player can now be revealed from concealment by the automatic animation,
  as the original allows; release 1 refused it. (#136)
- Jump animations now start from their proper first frame; in release 1 a jump
  could begin part-way through whatever animation the character was already
  playing. (#137)
- A character stepping aside to let someone pass now checks each direction
  properly; release 1 judged its first choices using only the way it was
  already facing. (#138)
- Walking routes now get the original length limit, a little more than
  requested; release 1 replaced the requested limit with the absolute maximum,
  allowing longer routes than intended. (#140)
- Characters fleeing or spreading out now move one tile further away, as in
  the original; release 1 stopped them a tile short. (#142)
- Paralysed or stunned characters now show their proper looping animation, and
  its sound plays as originally; release 1 could show the wrong pose and play
  the sound differently. (#144)
- Characters walking up to someone now plan their first route with the
  original settings, and try a third route when the second falls short;
  release 1 accepted a too-short route. (#145)
- Objects and characters turned to stone keep their looping animation's
  nearby-player check, as in the original; release 1 treated anything
  petrified as never near the player. (#146)
- Characters turning exactly half way round now turn the same way the original
  does; release 1 spun them round in the opposite direction. (#147)
- When an animation adds a sub-step, the game now tracks the new step; release
  1 kept pointing at the previous step's details. (#150)
- Each newly started animation now begins with a clean slate, as originally;
  release 1 could carry over a leftover step and target tile from an earlier
  one. (#151)

**Interface and screens**

- Shortened text on menu screens now ends with its dots after the cut-off
  words; release 1 drew the dots over the text's first letters instead. (#199)
- The button for a maintained spell now updates and disappears when the spell
  ends; release 1 left the button showing. (#206)
- A follower's health bar now shows its poisoned look at any level of poison;
  release 1 waited until poison passed 20. (#208)
- The combat bar in the full interface now animates during NPCs' turns;
  release 1 left it still whenever an NPC acted. (#210)
- Clicking to close the follower menu no longer also clicks whatever lies
  underneath; release 1 passed that click on to the window behind it. (#214)
- Clicking the portrait lens to leave the Options screen now closes it; in
  release 1 the click usually left the screen open. (#221)
- Dragging a scroll bar up by a single line now works and overshooting stops
  at the end; in release 1 a one-line drag was ignored and overshoots went
  through. (#224)
- Text too long for a menu text box, such as a save name, now shows its end
  with the cursor; in release 1 the whole text vanished. (#247)
- L opens the logbook again, and game hotkeys no longer fire while the main
  menu is open; in release 1 L did nothing. (#267)
- The chat and centre-view keys now work when you are not typing; in release 1
  they worked only while a text box was being typed in. (#270)
- Settings are read and saved cleanly again; in release 1 arcanum.cfg gained a
  blank line per setting on every save and "key = value" lines were misread.
  (#300)
- The logbook now records injuries dealt to you by creatures and people; in
  release 1 none of those injuries ever appeared there. (#301)
- Without a HighRes config file the map now scrolls at the original 60 steps a
  second; release 1 defaulted to 35, scrolling noticeably slower. (#325)
- Pressing Enter or Escape in a confirmation box now affects only that box; in
  release 1 the key also reached the save or load menu behind it. (#361)
- The brightness setting now actually changes the screen; in release 1 the
  slider was offered and saved but did nothing. (#366)
- After buying a newspaper the old "no thanks" reply now disappears; in
  release 1 it stayed on screen. (#76)
- The screen now updates when your fate points change; in release 1 the shown
  count could lag behind. (#88)
- The level-up message now lists spells learned automatically; in release 1
  they were left out. (#92)
- Floating damage numbers now read number first, as in the original; release 1
  showed them back to front, like "HT 12". (#104)
- A sprung trap now shows "Trap!" over the victim, as it did in the original
  game. (#112)
- The progress bar while saving and loading now advances once for every step;
  release 1 moved it only for some steps, so it jumped. (#197)
- Scrolling the world map up and down now stops at the map's real edge;
  release 1 worked out the bottom limit from the wrong measurement. (#200)
- Books and notes now begin each new page on a word; release 1 started pages
  with a stray space or blank line. (#201)
- Text typed into a box too long to show now stays within the box; release 1
  let the text and its cursor run past the edge. (#202)
- The combat indicator now sits in its right place from the start; release 1
  drew it two pixels too high until the screen was resized. (#209)
- The compact interface's health bar now has a see-through background; release
  1 drew a black box behind it. (#211)
- The compact interface's health bar now fills to its last row; release 1
  stopped one row short. (#212)
- The logbook now lists a blessing before a curse received at the same moment
  or later; in release 1 the later of the two came first. (#217)
- With both arms crippled, the logbook's injury page now shows both arm
  injuries as current; in release 1 it found the same entry twice and marked
  one healed. (#218)
- The logbook now keeps one scar entry marked current for each scar you carry;
  in release 1 only the most recent scar was shown as current. (#219)
- A logbook entry exactly as tall as the space left on a page now moves to the
  next page; in release 1 it was squeezed onto the current one. (#220)
- Turning the mouse wheel over a scroll bar no longer leaks through to
  whatever lies beneath it, even when nothing scrolls. (#225)
- Clicking a scroll bar's track just above the handle now pages up correctly;
  in release 1 the clickable area was measured from the wrong place on some
  lists. (#226)
- Clicks inside the inventory are now kept by the inventory; in release 1 they
  also fell through to other checks behind it. (#233)
- Lowering a technological degree on the character screen no longer also
  clicks whatever lies beneath the button, matching how raising one behaves.
  (#234)
- Menu text that is too wide is now trimmed correctly and never trimmed to
  nothing; in release 1 text with tabs was misjudged. (#248)
- Using an item that closes the inventory no longer lets the same right click
  reach the game world behind it. (#254)
- Hovering over the rest button is now handled by the button, like the other
  buttons (minor). (#268)
- The inventory button press is now kept only when the inventory actually
  opens (minor). (#269)
- Closing a loot or theft window drops any item being dragged and restores
  your interface setting; in release 1 the full interface stayed forced on.
  (#271)
- The moon on the clock now follows the calendar; in release 1 a mistyped day
  length made it run about fourteen hours ahead each lunar month. (#273)
- Hovering off an object now keeps the description of the object you were
  examining and clears stale highlights correctly. (#274)
- Long lines in the message window are now shortened to at least one letter
  instead of disappearing completely. (#279)
- The "cannot scroll further" cursor now points the way you tried to scroll,
  including diagonals, and down-left sits in the corner. (#288)
- An NPC who shares your portrait now shows its own or none; in release 1 it
  could show your portrait instead. (#318)
- Releasing a mouse button that was never pressed in a window no longer counts
  as a click there; release 1 reported it as one. (#355)
- The pixel just past a window's right or bottom edge no longer counts as
  inside it; release 1 counted it. (#359)

**Drawing and sound**

- Footsteps now sound different depending on the ground underfoot; in release
  1 every tile in the game used the same footstep sound type. (#171)
- Invisible and translucent characters and objects now fade as in the
  original; release 1 applied their see-through look only when another blend
  was already set. (#181)
- Small and medium sounds in the world play at 40% and 70% of full volume
  again; in release 1 they could play at full volume. (#310)
- The back half of two-part effects such as Reflection Shield is drawn behind
  the body again; in release 1 it covered the character. (#326)
- Spell glows made only of light now animate and end properly; in release 1
  they ended at once or never. (#132)
- Fire-damage spell glows made only of light now animate and end properly, as
  in the original. (#133)
- Reverse fire-damage spell glows now animate and end properly; in release 1
  some never ended. (#134)
- Blood pools now form under the wounded creature; in release 1 they sat at
  the corner of its tile. (#135)
- When the view moves, the map's parts now update in the original order,
  scrolling first; release 1 updated them in reverse order on every view
  change. (#159)
- Creatures blown apart now use the ordinary explosion art; release 1 treated
  every exploding creature as female when looking up the animation. (#163)
- Building-front art is now always looked up as in the original; release 1
  refused any building front whose art type did not match, which could leave
  it undrawn. (#165)
- One outdoor ground-tile variety table is now filled to its own length;
  release 1 used another table's count, filling it too far or leaving it
  short. (#169)
- The first piece of a damaged wall now uses its left-side damage art, as
  originally; release 1 drew the right-side damage art there. (#170)
- The object lighting setting now takes effect; in release 1 the game always
  behaved as if it were switched off, whatever the setting said. (#178)
- Effects drawn over objects at full size are no longer put through a resize
  step; release 1 resized every one, which could shift them slightly. (#179)
- Scaled characters wading in water now refresh their part of the screen as
  originally; release 1 skipped that refresh, which could leave leftover
  pixels. (#182)
- Rotating lights now keep their correct position each frame; release 1 kept
  adding the frame's offset, so the light drifted. (#183)
- Shadows cast by several lights are now ordered as in the original; release 1
  sorted equal shadows into a different order. (#184)
- Lights marked both indoor and outdoor now use their indoor colour, as
  originally; release 1 could take either colour. (#185)
- The game now works out correctly which tiles fall within the screen; release
  1 was one tile off for much of the screen. (#186)
- Scroll bar handles are drawn at their full height again; in release 1 they
  were drawn about one seventh as tall as they should be. (#227)
- The background behind world map notes is now tiled across its area; in
  release 1 every tile was piled at the first one's spot. (#245)
- Starting to drag an item now shows the correct cursor under it; in release 1
  whatever cursor was showing was carried along. (#255)
- Every completed trade now plays its sound, haggled, priced or free; in
  release 1 only a plain sale played it. (#256)
- Portraits, panels and the follower's figure in the inventory screen are
  drawn at their proper height; in release 1 their width was used as the
  height. (#258)
- The figure overlay in the inventory screen now sits three pixels right of
  its panel, as in the original game. (#259)
- Slot labels in the inventory screen now sit at the bottom of their art; in
  release 1 they were drawn at the top. (#260)
- Creature sounds now go quiet whenever your character is out of the world, as
  in the original; in release 1 a hidden creature silenced only itself. (#287)
- The volume limit for quiet, small sounds is now read correctly from the
  sound settings; release 1 filed it under medium sounds and left small
  unlimited. (#309)
- Distance to a sound is now rounded, so its volume fades evenly; release 1
  could misjudge it by one step at the edge of hearing. (#311)
- Background ambience plays one sound per area per moment again; release 1
  played them all at once, making ambience busier than intended. (#312)
- Blood splatters and weapon effects now stop at their small limit; in release
  1 they piled up without end during a long fight. (#315)
- Spell effects missing a back or ground layer no longer try to draw the
  missing part; release 1 placed it anyway. (#316)
- Scaled sprites are now positioned with rounding, as in the original; release
  1 could place them a pixel off. (#342)
- A scaled sprite cut off by the screen edge now shows the right part of its
  picture; in release 1 it could show the wrong slice. (#343)
- The see-through checkerboard on mirrored sprites now matches the original;
  release 1 drew the opposite half of the pattern. (#344)
- The see-through checkerboard on scaled sprites is now even; in release 1 its
  pattern came out irregular. (#345)
- Averaged blending of sprites now uses the right strength; release 1 garbled
  the red and green of the blend. (#346)
- The character view lens now repaints exactly the area it drew; release 1
  repainted a wrongly shaped area. (#358)
- Screen fades now start from the current screen and finish fully on their
  colour; release 1 restarted them and stopped just short. (#365)
- Floating text over characters now fades out smoothly; in release 1 it stayed
  at full strength until it vanished. (#367)
- Scaled images cut by the screen edge now line up as in the original; release
  1 could draw them a pixel off. (#368)

**Fixes with no effect in ordinary play** (103), listed for completeness:

- **The world editor and modding** (16): saving a map's objects, terrain
  codes, the sector grid, wall lines and deleting parts of long walls in the
  editor; modded text files with a stray carriage return; two-part light
  colours in modded effects. (#155, #161, #162, #166, #187, #188, #191, #192,
  #291, #297, #320, #321, #323, #324, #337, #339)
- **Screens and menus** (10): buttons and options with missing art or values,
  scroll-bar bookkeeping, a race-list search, a stray Escape after quitting,
  and windows that fail to open. (#203, #204, #205, #207, #213, #222, #223,
  #228, #249, #252)
- **Drawing and sound** (7): partial world-map redraws, menu animations,
  equipment slots whose art fails, sound balance, looping overlay sounds,
  effect sizes and sound cross-fades, none of which shipped data reaches.
  (#246, #251, #261, #313, #314, #317, #364)
- **The world and travel** (4): map preparation without a module, the town
  map link after switching modules, an unused town-map scroll, and a large
  scenery object built without its data file. (#154, #241, #243, #280)
- **Crash guards no play reaches** (3): text boxes without a change handler,
  a drawing failure on the save list, and windows without an input handler.
  (#236, #250, #356)
- **Spells, saving, items and characters** (9): two spell targeting rules no
  shipped spell uses, two save consistency checks, stack weights and
  generated grenade prices, an NPC's second weapon check, and the default
  character name. (#307, #308, #193, #198, #107, #281, #78, #282)
- **Game text** (1): text files containing the Russian letter "ya" are no
  longer cut short there; no English file has one. (#293)
- **The engine's own workings** (54): routines nothing calls, multiplayer-only
  paths, debug messages, error codes, file and folder helpers, memory
  counting and message bookkeeping, each now as the original has it. (#59,
  #86, #87, #99, #119, #121, #127, #131, #139, #148, #149, #152, #153, #156,
  #158, #160, #164, #167, #168, #173, #174, #176, #189, #194, #253, #284,
  #285, #286, #290, #294, #304, #327, #329, #331, #332, #333, #334, #335,
  #336, #338, #340, #341, #348, #349, #350, #351, #352, #353, #354, #357,
  #360, #362, #363, #370)

### 3.3 Mistakes in the decompilation, fixed before release 1

The same kind of fix, found earlier, while the code was being named and
tested; release 1 already had these.

**Combat**

- Dodge gives two points per skill level, as in the original; before the fix
  it gave five, so a skilled dodger avoided nearly every hit. (#49)
- NPCs choose their weapon by its total damage; before the fix they judged by
  fatigue damage alone, so a guard would draw a mace over a fine sword. (#52)
- In turn-based combat a knocked-down character gets up on its own turn, with
  the animation and its action-point cost; before the fix it rose free during
  others' turns. (#55)
- When an NPC makes peace, a character in the middle of an attempted attack
  now stops; before the fix only some kinds of attack were called off. (#16)
- Running during combat costs one point of fatigue each interval, as in the
  original; before the fix running in combat was free. (#30)
- In combat mode a plain click on a corpse does nothing and Alt-click loots
  it; before the fix the plain click started an unpredictable action. (#47)
- Attacking your own follower in combat asks for confirmation again, with Alt
  to force it; before the fix a click simply attacked them. (#48)
- NPCs pick the best of the weapons that can reach their target; before the
  fix they took the last such weapon in their pack. (#53)
- NPCs deciding whether to flee now weigh each party member's own level;
  before the fix they counted the leader's level once per member. (#54)
- Carrying a small light reduces the darkness penalty by the original amount;
  before the fix it was reduced slightly too much, by up to two points. (#50)

**Spells and magic**

- Spell projectiles now light up and play their sound where they hit; before
  the fix that impact light and sound never happened at all. (#13)
- Items that act only on their user now take effect directly, as in the
  original; before the fix they were routed through the spell-casting process
  instead. (#19)
- Magic items that pay for a spell with fatigue now charge that spell's cost;
  before the fix they charged another spell's cost and the cast could fail.
  (#29)
- Each spell now starts with its caster's reflection settings cleared, as in
  the original; before the fix the previous caster's could carry over (effect
  unmeasured). (#44)

**World, maps and travel**

- Random encounters now respect their maximum character level; before the fix
  that limit was never read, the minimum being read twice instead. (#24)
- Monsters in a random encounter group now spread over neighbouring tiles in
  the original's pattern; before the fix each one was shifted diagonally.
  (#25)
- On Linux, mod archives with folders whose names begin another folder's name
  find every file; before the fix some existing files were reported missing.
  (#34)

**Dialogue, quests and scripts**

- Dialogue replies can be clicked with the mouse again; before the fix their
  click areas were placed outside the window, all on one row, and did nothing.
  (#9)
- Game scripts divide numbers again; before the fix every division in every
  shipped script added the two numbers together instead. (#45)
- NPCs such as H.T. Parnell and Nasrudin pick the special greetings their
  dialogue files ask for; before the fix those greeting choices were misread
  and skipped. (#22)
- Low-intelligence characters read the simple-language quest log written for
  them; before the fix they saw the normal text and the dumb version was never
  used. (#39)
- A newspaper headline repeated in dialogue no longer gains a stray full stop
  after its own '!', '?' or '.'; before the fix every headline got one. (#18)

**Crashes and hangs**

- Reaching the experience needed for the level cap now stops at the cap;
  before the fix it crashed the game outright. (#56)
- A message file line of 2,000 or more characters, as a mod might contain, is
  cut off as in the original; before the fix it could crash the game. (#11)

**Saving and loading**

- Saves now record how often each random encounter has been met; before the
  fix those counts were lost and every save carried 1.7 KB of junk. (#31)
- Reading an empty part of a file no longer leaves it unusable for later
  reads, as in the original; before the fix the file was stuck afterwards.
  (#36)
- On Linux, deleting and renaming game files now finds them whatever the
  capital letters, as opening does; before the fix such files were left
  behind. (#46)
- On Linux, the game recognises its own module archive again, so map caches
  and saves record which module wrote them; before the fix they recorded
  nothing. (#57)

**Items, inventory and trade**

- The last-listed shop stock, the Multi-Magic (Dark) set on the stock game,
  restocks with goods again; before the fix any merchant or container using it
  stayed empty. (#12)
- Characters with exactly zero magical or technological aptitude can use magic
  and tech items as in the original; before the fix some such items refused
  them. (#20)
- Winning a gamble now gives you the item with the winner's message; before
  the fix a win was handled the same way as a loss. (#26)
- Alt-click dragging an item onto your tile now leaves items held inside an
  inventory alone; before the fix the check meant to skip them never did.
  (#37)

**Movement and animation**

- Characters no longer fidget in the background while the main menu has the
  game paused; before the fix their idle animations kept playing behind it.
  (#7)

**Interface and screens**

- Moving a window now moves its buttons with it; before the fix a ghost copy
  of the interface bar appeared at any screen height but 600. (#10)
- Six early copying mistakes now match the original: a floating-text effect,
  wandering in the dark, the skill button under the mouse, an overwrite
  prompt, lock files, a map check. (#1-#6)
- Main-menu screen text containing a tab is shown in full again; before the
  fix every such line was cut off where the tab began. (#8)
- The recent-actions button for a used item now gets the same art as in the
  original; before the fix it was given no art at all. (#27)
- The last line of a message file without a line ending no longer gains junk
  characters; before the fix 60 shipped message files were affected. (#35)

**Drawing and sound**

- Lights stored with a map area now shine where they were placed; before the
  fix they were put at tiny coordinates near the map's corner. (#23)
- Spell effects play their sounds again; before the fix every sound attached
  to a visual spell effect was silent, through three separate mistakes. (#33)
- Door and portal art now takes its colour set from the direction it faces, as
  in the original; before the fix the requested colour set was kept. (#32)
- Pictures clipped at the left or top of the screen or window are cut off
  cleanly; before the fix they drew a shifted part of the image. (#41)
- On Linux, the few sounds with no file assigned now simply stay silent;
  before the fix the game tried to load a folder as a sound. (#51)

**Fixes with no effect in ordinary play** (8): unused routines,
multiplayer-only paths and the editor's start-up. (#14, #15, #17, #21, #28,
#38, #40, #43)

### 3.4 Safety by design

- **A save is never reported saved when it was not.** If any part fails to
  write, the save fails and the old one stays.
- **Damaged or unusual game files are refused, not trusted.** These no
  longer hang or crash the game:
  - prototypes, map object files and jump points with impossible values;
  - scripts with impossible entry counts or unsafe instructions;
  - message (`.mes`) files with a duplicated number (the original looped
    forever) or a very large number of entries (the original took minutes);
  - saves with impossible lists, counts or sizes, for running animations,
    active spells, timed events, world-map notes and visited areas;
  - game archives with impossible sizes, overlong names or names that never
    end;
  - art with impossible frame sizes, or pixels that run past the frame;
  - terrain and building walk grids claiming impossible sizes, and map files
    naming impossible animation slots;
  - a dialogue line with a stray `@` marker, or a flag list ending in a
    separator.
- **Damage in one place stays in one place.** A damaged terrain row is drawn
  blank and logged; the original quit the game on the spot. A sound scheme
  with more than 24 sounds ignores the extra ones; the original crashed.
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

## 4. Changes to game mechanics

### 4.1 The Unofficial Arcanum Patch

All of UAP 2.0.2's fixes to the game program are part of Arcanum CE, and on.
(CE does not use UAP's patched `Arcanum.exe`; it makes each change itself.)

**Rules**

- **Throwing** costs 4 AP for everyone. In the original the player threw for
  free in turn-based combat, and NPCs paid 2.
- **Dragging an item** in combat costs 4 AP.
- **Resistances** of monsters, animals, undead and machines are capped at
  95%, like everyone else's.
- **Unspent character points** no longer cap at 56.
- **Constitution 1** no longer means a heal rate of 0.
- **Ageing:** everyone ages a year every 360 days, and the age floor of 20 is
  gone.
- **Half-elves** take the elven Beauty cap as well as the elven Dexterity
  cap.

**UAP's four optional extras**, three on:

- **Levels up to 100** (the original stops at 50).
- **No experience per hit:** a kill gives its full experience instead.
- **Master Educator:** the Educator background trains followers for any
  character. A follower one training step below you is raised to your level.
- **No combat music** is off: CE's combat music shuffle (below) is on in its
  place.

**Followers**

- **Spend a follower's character points yourself:** turn off their automatic
  levelling. Their sheet opens editable, from the follower menu or from
  dialogue, with arrows to step between followers.
- **A ninth follower command** opens their schematics.

**Data**

- Effect numbers up to 1,000, for UAP's added effects.
- UAP's three added monster prototypes get their stats.
- Two shipped inventory lists that name a valid item are no longer emptied;
  the original rejected the item and logged errors at every start.

**Keys**

- **Quick Load is F9**, away from Quick Save on F7 (it was F8).
- Hotkeys and the number row do nothing while a dialog is open (the number
  keys pick dialogue options).
- The magick/tech button is checked again when a game is loaded.
- `-cheatmode` turns on Troika's cheat mode.

**Display**

- No "HT 0" over a blow that did nothing, or over a door or chest that took
  no damage. A hit that only cost fatigue still shows its "FT".
- Walking critters keep their shields (UAP supplies the art).
- Half-elf player characters use elven art, as half-elf NPCs always did.
- Dark Elves, Ogres and Orcs get their own race icons in the message window;
  the original reused the elf, half-ogre and half-orc icons.
- Monster armor art 4 shows as 6.
- Monsters and unique characters fall back to the ordinary NPC generic name.
- Skill messages show the original skill-group icons (combat, thieving,
  social, technological) instead of one generic icon.
- An item of neutral complexity gets the magickal targeting cursor.
- A **Shadows** control on the Options screen's Video tab.
- The version banner reads "Unofficial Patch 2.0.2", top right, and the
  banner and the character, editor and shop screens use UAP's replacement
  font art.
- The combat bar, the hotkey bar's background and the skill icons move by a
  pixel or two; the lens frames of the written-item and schematics screens
  match their backgrounds, and the technology tab's lists sit a few pixels
  lower, matching UAP's tab art.

### 4.2 CE's own rules, always on

- **Varied combat music** (`combat music shuffle`, new since release 1). The
  original loops one track through every fight, a copy of the dwarven theme.
  Now a fight opens with the usual sting, then plays the game's area music in
  random order: never the track the area was playing, never the same track
  twice running. Release 1 turned combat music off instead (UAP's option),
  which is now off.

- **NPCs heal only living patients** (`heal living patients`, new since
  release 1). An NPC's first aid, on itself, on its allies or when it offers
  to bandage you, skips undead and mechanical patients, and any NPC may heal.
  The original instead stopped undead and mechanical healers, and let a
  living one heal robots. Your own Heal skill is unchanged.

### 4.3 Built since release 1, not in this package yet

These mods are built and checked outside the game. They are not part of this
release; most are not yet tested in a game.

- **Initiative and Awareness:** turn-based fights follow rolled initiative
  (Dexterity plus 1 to 10) instead of the party always acting last; on the
  world map the party may spot a random encounter coming and avoid it, or
  meet it further off; an unspotted one is an ambush, in which at most one
  enemy acts first.
- **Followers:** alignment and level no longer stop an NPC joining you or
  make a follower leave, and low reaction no longer stops one joining; a
  follower treated badly enough still leaves.
- **Encounter Variety** (a rough first draft): more frequent and more varied
  random encounters, rare families at night, and from level 22 the creatures
  geography otherwise hides.
- **Tech Specialists:** components outside a schematic's own discipline need
  at most Associate, so an explosives expert need not become a Chemistry
  Professor to put Strong Poison in a grenade.
- **Schematics in Shops:** every schematic a shop sells is in stock at least
  30% of the time after a restock, where most were 3 to 10%.
- **Starting Gold:** a new character starts with three times the gold their
  background gives.
- **Conjure Spirit Scrolls:** the Scroll of Conjure Spirit in every magic
  shop, where only the Tarant magick shop carried it.
- **Brighter Lanterns:** lanterns, the Electric Light and light helmets throw
  a wider, brighter circle, and the Electric Light glows warm.
- **Aimed throws** (`called shot throws`): built, but in no mod and not yet
  playtested.

## 5. Running the game

**Platform**

- **Native on Linux and Windows**, through SDL3 instead of DirectX.
- **A resizable window or full screen**, at the High Resolution Patch's
  resolution.
- **Save thumbnails** are centred and smoothed at any resolution.
- **Left Alt** turns object highlighting on and off.

**How the interface behaves**

- **Escape** first closes an open mode, such as the inventory or targeting,
  and returns to the normal view; the original opened the game menu from any
  mode. On a screen opened from the main menu, Escape goes back to the menu.
- **The map scrolls** when the mouse is within eight pixels of the screen's
  edge; the original needed the very edge pixel.
- **The world is drawn across the whole screen**, under the interface bars;
  with the full interface the original drew only the strip between them.
- **The interface no longer flashes** over the main menu when the window is
  resized.
- **Any key closes** an OK or Cancel message box, and its letter shortcuts
  work on the key press.
- **The game counts as active** while the mouse pointer is over its window.

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

**Test switches, never turned on by setup**

- `debug criticals`, `debug fumbles`, `debug character points`: force
  criticals, force fumbles and give free character points.
- `determinism`: a test setting for reproducible runs.

**Not supported**

- **Movies do not play on Linux.** The logos, the intro, the ending and the
  in-game cinematics are skipped.
- **Multiplayer** is not supported.

## 6. The original's quirks, kept on purpose

CE keeps the original's behaviour unless this catalogue says otherwise. The
source records over 600 places where the original does something odd and CE
does the same. These are the ones a player can meet.

### 6.1 Kept in every configuration

**Combat**

- **Every melee, bow and thrown fumble hits its wielder**, half the time
  critically. (With the Combat Rebalance, a gun's fumble jams it instead.)
- **A fumble's critical effects are rolled twice** on the wielder.
- **Dodge is rolled after every hit**, by the target, at twice their Dodge.
- **The Grenade Launcher's explosion can hit you and your followers**: you
  when you stand within 2 tiles of the target, and a follower near it. Do not
  fire it at anyone beside you or a companion.
- **The called-shot cursor shows the wrong limb:** aiming at the arms shows
  the leg cursor, and the legs the arm cursor. The attack goes where you
  aimed; only the cursor is crossed.
- **Everyone swings high at dwarves and halflings.** The low attack is meant
  for short targets, but every character counts as human-sized, so only a
  fallen or shrunken target gets it.
- **Two ghouls and a soul are not undead.** The Savage Ghoul, the Stygian
  Ghoul and the Ensanguine Soul are living creatures to the engine, wherever
  undead are treated differently.
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
- **A second run order ignores encumbrance.** From significant encumbrance
  (60% of the carry limit) up, an order to run makes you walk; order it
  again while you are still walking and you run, however heavy the load.
  Encumbrance stops at its severe level (90%): carrying more than the limit
  is no worse than carrying 90% of it.
- **Cone-shaped targeting** covers a square around its origin, not a cone.

**World and interface**

- **At high resolutions some monster generators spawn less often.** They wait
  until they are off screen, and a wider screen keeps more of the map in
  view.
- **The two recent-action buttons swap places** every time you load a game.
- **The round tab at the right end of the hotkey bar does nothing.** The
  manual's "destruction tab" was never built; drag a slot off the bar to
  clear it.

**Saves**

- **Overwriting a save under a new description** leaves the old description
  behind, so the slot appears twice in the save and load lists. Both entries
  open the same game.
- **When CE first copies your saves to its own folder**, a file whose size is
  an exact multiple of 32 KB is reported as not copied, although the copy is
  complete.

### 6.2 Kept unless an optional mod is on

These are as the original has them in a default install. The mod named
changes each one (section 1).

- **An NPC may wield a weapon that costs more than its turn**, and then never
  attacks in turn-based combat. Combat Rebalance.
- **A weapon's on-hit spell is charged as a cast**, so in turn-based combat
  the Grenade Launcher's explosion never lands. Combat Rebalance.
- **Every firearm fumble shoots its wielder.** Combat Rebalance.
- **Poison ticks once in fifteen turn-based rounds.** Combat Rebalance.
- **A wielded thrown weapon injures nobody**: in shipped data, the Throwing
  Dagger. Combat Rebalance.
- **Prowling:** anyone can prowl in combat, followers crouch whatever their
  skill, and open ground costs a flat 50. Combat Rebalance.
- **A critical can do nothing to a shapeless creature**, and only knock down
  an undead one. Combat Rebalance (creature profiles).
- **In real time, actions repeat as fast as their animations**, so a fast
  click outpaces the turn-based cost. Real-Time Pacing.
- **A melee enemy chasing you swings at the air** as you run past. Real-Time
  Pacing (in part).
- **Aimed attacks are only on the comma, period and slash keys.** Called
  Shots.
