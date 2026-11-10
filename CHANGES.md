# Arcanum CE - the catalogue of changes
<!-- Generated from the project's release documents by sync-public-docs.py; edit those, not this copy. -->

> No pretensions to any originality in this work.
> Coding was done nearly entirely using LLM tools, Claude, Astra and Deepseek.
>
> **My grateful acknowledgement to Messrs. Tim Cain, Leonard Boyarsky and Tim Anderson, Troika Games, Mr.
> Drog Black Tooth, Alex Batalov, the Terra-Arcanum team, and Kababji, who introduced me to this game's
> demo disc all those years ago.**

Arcanum CE is a rebuilt engine plus mods for *Arcanum: Of Steamworks and
Magick Obscura*.

- It runs over Arcanum 1.0.7.4 from GOG (Good Old Games) or Steam, with the
  Unofficial Arcanum Patch (UAP) 2.0.2 and its High Resolution Patch.
- It builds on and extends Arcanum Community Edition
  (github.com/alexbatalov/arcanum-ce), a decompilation of the original game.
- Bug fixes divide into 2 types:
  - **Bugfixes for bugs which shipped in the original 2001 game** (Section
    3.1)
  - **Bugfixes for Arcanum Community Edition's decompilation** (Sections 3.2
    and 3.3).
- The original's 'quirks' are kept on purpose. Section 6 lists reachable
  quirks.

**Release 4 in brief.** Release 3 came out on 2026-11-06; release 4 is a
beta. It adds Arcanum from Steam:

- The Windows setup finds Arcanum in a Steam library and installs into
  Steam's game folder. Steam's game files are the same as GOG's, so
  everything else works as it does over GOG.
- When a PC has more than one Arcanum, GOG's and Steam's say, setup and the
  uninstaller list them and ask which one.
- The Linux setup takes a Steam copy's game folder too.

The game, the engine and the mods are release 3's.

**Release 3 in brief.** Release 2 came out on 2026-10-29; release 3 is a
beta. It adds:

- Simultaneous Turns, a new optional mod (section 1.10): the enemy side of a
  turn-based round acts at once
- In the Combat Rebalance: new Dodge rules, floating text that tells a miss
  from a dodge and says when armour stopped a blow, and enemy casters that
  protect themselves and spare their own side
- Better Shops: the new stock shows from a shop's first visit
- Fixes: sorting and text comparison as the original does them, and a few
  more checks on damaged files

What release 3 adds is marked "new in release 3".

**Release 2 in brief.** Release 1 came out on 2026-10-18; release 2 is a
beta. Progress in release 2 focused on bugfixes to both vanilla Arcanum and
to the CE engine rebuild (section 3.2), and expanded combat overhaul
features. The Combat Rebalance (section 1.1) covers:

- Guns and armor
- Follower AI fixes/extensions focused on magic casting and healing
- Critical combat events overhaul
- Traps and traps skill overhaul
- Grenade damage revision
- Fumbles revision
- Sight and hearing revision
- Prowling/stealth revision, vanilla implementation made sneaking very
  difficult

Other smaller mods are in sections 1.2 to 1.9. Mac package - as yet
untested, I am working on it. What release 2 adds is marked "new since
release 1".

**Contents:**
1. The optional mods: Combat Rebalance, Real-Time Pacing, Called Shots,
   Followers, Initiative and Awareness, Better Shops, Starting Gold, Brighter
   Lanterns, Encounter Variety, Simultaneous Turns
2. Highlights: the bugs worth a story, and the quirks left alone
3. Bug fixes: the original game's bugs, the decompilation's mistakes, safety
4. Changes to game mechanics: the Unofficial Arcanum Patch, CE's own rules,
   what is built but in no mod
5. Running the game
6. The original's quirks, kept on purpose

Within each category, changes are listed from the largest effect on play to
the smallest. A number such as (#58) is the fix's entry in the project's
ledger of decompilation mistakes.

## 1. The optional mods

Ten optional mods come with Arcanum CE, all off unless you turn them on.
Six were new in release 2 (sections 1.4 to 1.9), and Simultaneous Turns
(section 1.10) is new in release 3.

- **Turning them on:** setup asks about each one. Afterwards each is one line
  in `arcanum-ce.cfg`: change its `0` to `1` and restart the game to turn it
  on, back to `0` to turn it off.
- **Saves:** keep the Combat Rebalance on for a save made with it. Every
  other mod can be turned on or off with any save.

### 1.1 Combat Rebalance (preview) - `mod combat-rebalance`

A rework of combat, built and tuned in a test arena.

**Press Q to reload a gun.** With the Combat Rebalance on, guns have a
magazine/cylinder mechanic and must be reloaded.

**Firearms**

- **Reloading.** A gun holds ready shots, and pressing '**Q**' reloads:
  - pistols and revolvers 6, rifles and muzzle-loaders 1, repeaters 7, heavy
    guns 2;
  - one shot reloads at a time, topping off, though some guns refill a whole
    magazine;
  - in turn-based combat, unfinished loading carries over to the next turn;
  - in real time you must stand still while loading;
  - followers and enemies reload, top up and switch to a melee backup if
    necessary
  - outside combat, press 'Q' to reload
- **Every gun revised by its class** (all 41 firearms): damage, firing and
  loading cost, range and price.
  - Muzzle-loaders hit hard and load slowly, shorter ranged
  - Rifles and repeaters stay accurate further out, most versatile
  - Pistols are quick, they lose accuracy and penetration with distance.
  - The rare guns each have a specialized role and effect.
- **The ammunition counter shows the gun's state** (new since release 1):
  ready shots over capacity, then spare rounds, such as `3/7 24`, or
  `JAM 24` while the gun is jammed.
- **Gun criticals by kind:** pistols critical more often, rifles penetrate
  armor, muzzle-loaders and heavy guns knock down enemies and cause fatigue
  damage.
- **Jams:** a firearm's fumble jams it. Clearing the jam costs reload work,
  the gun keeps its ammo. (In the original, every fumble shot the shooter)
- **An easier first shot** at an enemy that has not yet noticed you.

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
- **Enemy casters** (new in release 3) put up a protective spell first, keep
  an area spell off their own side unless it also catches two of yours, and
  leave alone a foe who is already disabled or has just recovered. A caster
  no longer loses its turn on a spell its target cannot take.

**Followers** (new since release 1)

*Casting*

- **Caster followers cast from the start of a fight.** In the original a
  follower rolled no chance at all to cast while it and you were above half
  health, unless its foe outranked it, so a mage such as Arronax (level 50,
  so almost nothing outranks him) or Raven punched through most fights. Now
  a follower that keeps its distance, carries no weapon, or has one that
  barely gets through the foe's armour summons, protects and attacks with
  its spells like any other caster. Fighters who know a spell or two keep
  the original rule.
- **A reserve while everyone is healthy:** such a follower stops casting
  below a fifth of its fatigue and never risks fainting. Once you or it are
  hurt, it may spend everything, as other casters do.
- **Arronax casts every spell he knows** - all seven, not only the highest
  of each college, as his map entry otherwise insists.
- **No wasted turns on spells that cannot reach:** a follower does not cast
  Jolt or Nightmare, which work only around the caster, at a foe out of
  their reach.
- **Protections go on you first** while an enemy is attacking you: Agility
  of Fire, Shield of Protection, the Water and Earth bodies and the like,
  unless you already have them or are too technological for them to work.
  Otherwise it protects itself, as before.
- A follower's area spells already spared you and the party in the
  original, so there is no friendly fire to avoid.

*Healing*

- **Every patient, every turn, once you are hurt:** with you at half health
  or below, a healer (Virgil, Raven, Dante and others) looks at everyone it
  could heal every turn. In the original a healer already in the fight
  looked at anyone above 30% health only one turn in five.
- **The worst hurt first,** rather than itself, then you, then the others.
- **A tired healer still heals you.** In the original a healer short of
  fatigue gave its patient a Fatigue Restorer, even a dying player, in place
  of a heal. Now it restores fatigue only to a patient who is the tired one,
  and only after healing a badly hurt one.
- **No heals that cannot work:** a White Necromancy heal, spell or item,
  that would fail three times in four or more on a strongly technological
  character is not tried. The original kept casting it into a dying
  technologist until the healer fainted.

*Orders and conduct*

- **An attack order holds:** a follower you order to attack goes after its
  target however far away it is. Before, the 8-tile leash called it back.
- **Wait Here is a post:** a follower left waiting defends itself and
  whatever comes within 8 tiles of where it stands, instead of measuring
  from you.
- **No running from other people's fights:** in the original a follower who
  saw a stranger attacked fled, then surrendered, and sat the rest of the
  fight out.
- **Back Off is not a vow of martyrdom:** a follower told to back off still
  fights anyone outside the party who attacks it, and is then back in the
  fight.
- **A refusal is said:** a good follower who will not fight a good attacker,
  or one of its own people or faction, now tells you so, once per attacker,
  instead of standing there.

*Gear*

- **Your gear choices stick:** what you put in a follower's hands or on its
  back stays there while it can use it. In the original it re-chose its
  weapon by damage, and its armour and rings by price, at every change to
  its pack and at every fight. To let it choose, empty the slot.
- **The gun comes back:** a gunman follower who switched to its melee
  backup when out of ammunition or with an enemy beside it has the gun back
  in its hands after the fight.
- **It leaves what you drop:** an idle follower no longer picks up nearby
  items.
- **Trained weapons first, for everyone:** an NPC, follower or enemy,
  prefers a weapon that reaches its target only if it is trained with it.

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
- **The Stun spell** stuns 35% longer (new in release 3).
- **A cast cut short** - by turn-based combat starting, or a new order - ends
  instead of sticking, which had kept the AI from ever casting that spell on
  that target again (new in release 3).

**Dodge** (new in release 3)

- **Armour and weight hinder it:** body armour takes 3 (leather), 6 (chain)
  or 12 (plate) off the dodge chance, and each level of encumbrance 2 more
  (16 when severely encumbered).
- **Guns are hard to dodge:** against a firearm only 30% of the chance
  applies. Bows and thrown weapons are dodged in full.
- **No Dodge while asleep or knocked down.**
- **Training floors the attacker:** instead of the original's rare critical
  dodge, a dodge knocks the attacker down 5%, 10% or 20% of the time, by the
  dodger's training (Apprentice, Expert, Master), whatever the attacker's
  own training. Getting up costs 6 AP.

**What the floats say** (new in release 3)

- **"Miss"** over a plain miss, so it reads apart from "Dodge!".
- **"Stopped"** when armour took all of a blow's damage, and **"Glancing"**
  when it took more than half, with any fatigue that got through.
- **At Verbose**, a hit's damage in one line: "4 HT, 7 FT".

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

**Fumbles** (new since release 1)

A fumble with a melee weapon, a bow, a thrown weapon or bare hands no
longer always strikes its wielder:

| what happens | chance | |
|---|---|---|
| **Knocked down** | 50% | you stumble and fall; getting up costs 6 AP in turn-based (other knockdowns, 5) |
| you hit yourself | 30% | for the weapon's full damage |
| **Weapon dropped** | 15% | it lands beside you; bare hands stumble instead |
| you hit yourself critically | 5% | with a critical's effects |

- **Training protects:** at Expert in the weapon's skill a self-hit becomes
  a stumble, and a Master of Melee, Bow or Throwing never fumbles.
- **A thrown weapon you drop** lands at your feet, unthrown.
- **A fumbled grenade** (or Molotov, or gas) flies a tile in a random
  direction and goes off there, with you a tile from the blast.
- **A bow's or thrown weapon's self-hit lands.** In the original it was
  aimed at your own feet and vanished, so a fumbled grenade never went off.
- **Followers and enemies** fumble by the same table.
- Firearms keep their own table (Jams, above).

**Grenades** (new since release 1)

Where a grenade lands now decides what it does. This covers the Explosive,
Electrocution and Concussion grenades and the Grenade Launcher's burst,
each a 2-tile blast.

- **Everyone in the blast is hurt, you and your followers included.** The
  original spared the thrower's party, whoever threw (an enemy's grenade
  already hurt its own side). The blast's knockback reaches you too, so a
  grenade at your own feet throws you back.
- **Damage falls off fast:** full on the tile where it lands, half one tile
  away and a fifth at the edge, for health and fatigue alike, before armour
  and resistance.
- **Each victim rolls its own critical:** 15% at the centre, 5% one tile out,
  never at the edge. A blast critical does 20% more damage and knocks the
  victim down; someone just back up from a critical resists it.
- **Your blast does not offend your followers.** A blast or spell that
  catches your own party counts as an accident, as a stray shot does, so a
  follower may complain but stays. In the original a follower caught in
  your grenade left the party after the fight.
- **Followers throw with care:** a follower will not throw a grenade where
  you or another ally would be inside the blast.
- **A missed throw scatters as before:** (20 - Throwing) / 5 + 1 tiles in a
  random direction, 5 at no skill and 1 at full, never further than the
  throw itself.
- Mustard gas, fire (the Molotov), the poison explosion and the placed bombs
  (dynamite, the time bomb) keep their original rules.

**Sight and hearing** (new since release 1)

NPCs notice you less easily, above all through walls. Quest and dialogue
scripts keep the original checks, so no quest changes.

- **Walls and closed doors stop ordinary noise:** walking, talking, a
  sword's or dagger's blow and a picked lock are not heard through them. In
  the original, hearing went through a wall at the cost of 2 tiles of range.
  Loud sounds - a blunt weapon's blow (the mace, hammers, clubs, staves) and
  any firearm but the hushed ones - carry a third of their range through.
  An open door is open air, and in the open hearing is as before.
- **Standing still makes no noise:** an NPC hears someone standing still no
  further than a prowler, about 2 tiles; walking or running, about 5, as
  before.
- **Being heard only draws a look.** In the original a heard target was
  treated as seen. Now an NPC that hears you but cannot see you walks to
  where the sound came from and looks, once per sound; it attacks you,
  defends a victim or catches you at a lock only once it sees you. In a
  turn-based fight a bystander only turns to look, and an NPC already
  fighting you keeps track of you, as before.
- **Windows, glass doors and fences don't hide you:** an NPC now sees
  through anything see-through, where the original treated them as walls.
  Shots are still stopped by them, walls and wooden doors still hide you,
  and you still cannot open a container you see only through a window.
- **The edge of a view sees half as far:** straight ahead and one step to
  either side an NPC sees its full range, at the two outer steps half of it,
  and behind it nothing, as before.

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
  since release 1), so many weak bites no longer build into a deadly dose; a
  poisoned weapon's hit, to eight times.
- **Backstab** (new since release 1) deals the weapon's damage twice plus 3
  per Backstab skill level, and a critical no longer multiplies the skill's
  part: a Master's dagger backstab averages about 66 instead of about 130, and a
  better weapon now matters. Its critical chance is never below +8 at one
  point and +14 at two, where the original could fall below an ordinary
  blow's against a stronger target.
- **Prowling:**
  - attacking from prowl ends it;
  - followers prowl with you only if they have some Prowling skill, and an
    attack on one reveals only that follower;
  - starting to prowl in combat needs Master training (prowling started
    before combat carries into it), and costs 4 AP (new since release 1), so
    a backstabber cannot hide again between strikes for nothing;
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
- Loading a turn-based save no longer ticks poison again or ends summoning
  sickness early (new in release 3).
- Getting up after a fumble costs at most a whole turn, so a slow critter is
  no longer stuck on the floor for the rest of a fight (new in release 3).
- Without `walls stop sound`, hearing through a wall is the original's again
  (new in release 3).
- A follower whose gun ran dry gets it back after the fight however many
  enemies swapped weapons in the session (new in release 3).

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

### 1.4 Followers - `mod followers`

New since release 1; as yet untested in play.

In the stock game almost every companion has alignment and level limits of its
own: Magnus joins only a player of alignment 0 or better and level 7 or more,
Geoffrey only one of 0 or worse, Raven only one of +9 or better and level 31.
Most other NPCs refuse when your alignment and theirs are about 100 points
apart, or when they are 2 or more levels above you, and a follower keeps
weighing all of this for as long as it follows you. With this mod:

- **Alignment and level** no longer stop an NPC joining you, or make a
  follower leave or complain about your conduct.
- **Low reaction** no longer stops one joining. A follower treated badly
  enough still warns you, and then leaves.
- **Unchanged:** a companion's own dialogue can still set terms (Weldo asks
  how he feels about you, Sogg Mead-Mug looks at your Charisma, Thorvald at
  your Persuasion), and Charisma still limits how many followers you lead.
- **Turned off,** the stock checks return, and a follower who would not have
  joined under them may leave at its next check.

### 1.5 Initiative and Awareness - `mod initiative-awareness`

New since release 1; as yet untested in play.

- **Initiative:** in the original turn-based game the party acts last in
  every round, however quick it is. With this mod every combatant rolls
  Dexterity plus 1 to 10 when the fight starts and keeps it for the fight;
  turns go highest first, and ties go to the party.
- **Awareness:** before a random encounter appears, the party's sharpest
  member (best Perception plus Intelligence, you or a conscious follower) may
  spot it coming: "Virgil spots Lesser Boar ahead." You then choose to
  **avoid** it, and travel on as if nothing happened, or **engage**, and the
  monsters appear about 12 tiles off instead of 6, with time to prepare. A
  party whose best member has Perception plus Intelligence 16 spots an easy
  encounter about 85% of the time, an average one 60%, a powerful one 35%.
- **Ambushes:** an encounter you did not spot starts about 4 tiles away, and
  in its opening round at most one enemy acts before your party.

### 1.6 Better Shops - `mod better-shops`

New since release 1; as yet untested in play. Three changes to what shops
sell, in one mod because they edit the same file:

- **Schematics in stock:** every schematic a shop sells is in stock at least
  30% of the time after a restock, where most were 3 to 10%.
- **Tech specialists:** components outside a schematic's own discipline need
  at most Associate, so an explosives expert need not become a Chemistry
  Professor to put Strong Poison in a grenade. The hard-to-make components
  are sold.
- **Conjure Spirit scrolls** in every magic shop, where only the Tarant
  magick shop carried them.

A shop shows the new stock from your first visit: one still holding the stock the
game's maps ship is restocked once when its map loads. After that it re-rolls
a day after you trade there, as before.
Measured over 200 restocks of every shop list in the game, every item came
in as often as the mod's model predicts, and Conjure Spirit scrolls were in every
magic shop's stock every time.

### 1.7 Starting Gold - `mod starting-gold`

New since release 1, and played. A new character starts with three times
the gold their background gives: 1,200 instead of 400 for most, and the
others in proportion, so each background keeps its place against the rest.
The seven backgrounds that start with nothing still do. Prices, rewards and
loot are unchanged, and an existing character keeps the gold it has.

### 1.8 Brighter Lanterns - `mod brighter-lanterns`

New since release 1; chosen from screenshots, as yet untested in play. With
this mod the Lantern, the Electric Light, the Helm of Light and the Miner's
Helmet light about four times the area at midnight, and the bright core about
ten times more, on anyone who carries them, NPCs included. Each keeps its own
warm colour, and the Electric Light's hard white becomes a warm cream.

- **With the Combat Rebalance:** darkness hides a prowler, and a carried
  light gives you away, so a brighter lantern gives you away further.
- **A light used against its aptitude** (a technological light in a mage's hand, say)
  is still smaller, as in the original.

### 1.9 Encounter Variety - `mod encounter-variety`

**Experimental:** a rough first draft, new since release 1 and as yet
untested in play. Its numbers come from a model of the encounter tables, not
from play. In the stock game many encounters stop
at level 10, 15 or 25 with nothing in their place, so at level 30 about two
checks in five find nothing, and along the main roads more than half. This
mod changes the encounter file in five ways:

- **No dry spells:** each table takes on its terrain's stronger encounters
  from the level where its own run out.
- **Rare creatures at night:** jungle, swamp, snow and powerful elven-forest creatures,
  which the main roads almost never cross, may appear in average forest and
  grassland after dark, up to level 21.
- **Dead spots mended:** a table around Stillwater held only a one-time
  quest encounter and silenced a region 4,800 tiles wide; it now covers 1,600
  tiles. The easy swamp, jungle, desert and snow tables, which no area used,
  feed their creatures into the average ones.
- **A little more often:** about one encounter every 4 days of travel on the
  main roads, instead of 6.
- **From level 22, the rare families anywhere:** Krags, gorillas and the
  Tiger, Slime Demons and the Swamp Ape, Polar Bears, Hell Beasts and the
  Lethe Wyvern among them, about a quarter of every table.

Quest encounters still need their story flag, and each encounter holds the
creatures it always did.

### 1.10 Simultaneous Turns - `mod simultaneous-turns`

New in release 3, and played. In the original turn-based game every critter
takes its turn in succession: six wolves against you and a follower is eight
turns, one after another, and the wolves' six are most of the wait. With
this mod **the enemy side takes one turn together**: everyone outside your
party moves and attacks at once, each on its own action points under the
usual costs, and your turn comes when the last of them has finished.

- **Your party keeps its own turns**, one at a time, as before: you, your
  followers and what either of you summoned.
- **The side acts when its first member would.** Without Initiative and
  Awareness the party acts last, so the enemy side goes first. With it, the
  side acts at its best initiative.
- **Ambushes stay forgiving:** with Initiative and Awareness, at most one
  enemy still acts before your party in an ambush's first round.
- **Nobody acts twice in a round**, and Fast Turn-Based still applies.
  Real-time combat is unchanged.

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
  critically, unless the Combat Rebalance is on.
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

- A corpse you take clothes from keeps its colours in coloured light; the
  original redrew it in a few grey and olive tones (new in release 3).
- The "ghosts" over dead critters that do not decay draw correctly.
- Summoned creatures keep their own colours.
- An effect's light is added once, not twice or not at all.

**Smaller fixes, with no ordinary effect on play**

- A mod that empties the sleeping text no longer crashes the game when a
  sleeping character's name is drawn (new in release 3).
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
the game now plays as the original here. Each line says what is right,
then what release 1 did.

**Combat**

- Stunned enemies lose their turns; before, a stun barely held them and they
  kept acting. (#77)
- A crippling arm critical makes the victim drop both weapon and shield;
  before, it tried to drop the weapon twice and the shield stayed on. (#61)
- Points in Backstab let you backstab without Apprentice training; before, you
  needed the training. (#94)
- Healing crippled legs cures the legs; before, it cured blindness instead and
  left the legs crippled. (#101)
- A critical hit with a magic melee weapon, after its magic took hold, rolls
  its extra damage and lasting effects from the original's odds; before, they
  came from a leftover value, often far too likely or never. Found by the first
  macOS build's compiler. (#373)
- A crushing blow to the head can no longer blind; before, it could when the
  helmet stayed on or there was none. (#103)
- In turn-based combat your followers act after the enemies; before, they were
  mixed in among them. (#106)
- Invisible NPCs become visible when they attack, while an invisible player
  stays hidden; release 1 had it backwards, revealing only the player on every
  attack. (#143)
- A stunned, paralysed or petrified player can no longer act by clicking
  targets; before, being stuck only stopped you if you were both petrified and
  paralysed. (#266)
- While aiming, you can click through corpses to whatever lies beneath; before,
  bodies always got in the way, and each aim wasted memory. (#306)
- Disarmed NPCs pick up the nearest weapon; before, they walked off to the
  farthest one in sight. (#79)
- NPCs who decide to back away in combat do so at once; before, they waited a
  pass, a whole turn in turn-based play. (#84)
- Armour with special critical-hit behaviour reacts to critical hits and
  misses; before, those reactions never ran. (#100)
- A stun's knockout chance is back to the original odds; before, it was one
  point lower. (#102)
- A blow that only tires the target counts as a hit and plays its hit effects;
  before, it did not. (#105)
- Attack damage lines show "0" only when both the lowest and highest damage are
  zero; before, only the highest was checked. (#275)

**Spells and magic**

- NPCs choose among equally good spells in the original's order; before,
  ties fell the other way, so an NPC knowing Unseen Force and Drain Will
  opened with Unseen Force where the original opens with Drain Will. The
  world map's equal encounter regions are ordered the same way. (#374, new
  in release 3)
- Effects that multiply or divide work, and carrying the Flow Reaper no longer
  crashes the game; before, every multiplier was zero. (#60)
- Wall of Fire, Squall of Ice and the fire grenade hurt enemies and spare their
  caster's party; before, they burned the caster instead. (#65)
- Wall of Fire, Wall of Stone and Wall of Force are five tiles long and lie
  properly on diagonals; before, they were only three tiles. (#302)
- The discount for mastering a college of magic is back to the original
  rounding; before, a one-point spell became free. (#114)
- Dispelling Stone turns the victim back to normal colour; before, it stayed
  grey. (#115)
- Only very hard locks resist a weak caster's unlocking magic; before, any lock
  could. (#116)
- Knockback spells push their target back; before, every knockback was blocked
  on its first step. (#129)
- Elementals and a few other monsters built fresh by CE get their innate
  magical traits; before, those 19 creatures were generated without them.
  (#283)
- A spell stops when its target is destroyed and finds its target again after
  loading; before, it could carry on. (#117)

**World, maps and travel**

- Random encounters draw from every line of their tables; before, each table
  only ever produced its first encounter, so the Malochian Hand assassins never
  appeared. (#58)
- Innkeepers and inn beds no longer quietly block or unblock a map area each
  time they check the hour, as they did in release 1. (#63)
- A new world map waypoint snaps to a known place nearby before the route is
  planned; before, routes went to the exact clicked spot. (#240)
- Area markers on the continent map appear at their own places; before, they
  could be drawn at the last note's position or not placed at all. (#244)
- Shooting traps find their source near the person who set them off; release 1
  looked near the trap. (#113)
- A trap that has been broken counts as broken; before, it never did. (#118)
- On the world map, Z does nothing while travelling and switches to an open
  town map from the continent view; before, these were mixed up. (#237)
- Pressing Delete on the world map removes the selected waypoint, or the last
  one; before, the key did nothing. (#238)
- Dragging the world map stops scrolling when the mouse leaves the map; before,
  it kept scrolling. (#239)
- Waypoint markers on the world map are exactly their drawn size to click;
  before, they reached one pixel too far right and down. (#242)
- The world map's "nearest known place" ignores a place exactly at the edge of
  its range; release 1 counted it. (#319)

**Dialogue, quests and scripts**

- Text comparisons treat accented letters as the original does; before, an e
  with an acute accent and one with a grave accent compared equal, which
  matters most in translated and modded text.
  (#375, new in release 3)
- Scripts that ask about a skill, including every gambling conversation, just
  ask; before, they went on to cast a stray spell. (#62)
- A character short of money sees the "I can't afford that" replies; before,
  they were always hidden, across 69 conversations. (#66)
- The "Roll the dice" options with gamblers such as Killian Drake appear;
  before, the dice game could never be offered. (#70)
- The original 2001 patch fixes for world objects apply: NPC scripts, names,
  training and Myrth's necklace; release 1 never applied any of them. (#298)
- When a hopbot, spider-bot or automaton is turned into an item it is handled
  cleanly; before, the change could disturb an unrelated object in the world.
  (#64)
- Dialogue lines that check your character's level work; before, they always
  failed (seen with the Boil guard). (#67)
- NPCs who tell rumours pick from their whole set; before, they repeated only
  the first rumour of each set after the first telling. (#68)
- NPCs who mention themselves by name say their own name; before, they said
  yours, as in "Thank you" followed by your name. (#69)
- Scripts that set how much an NPC likes you set the intended amount; before,
  they used an unrelated number. (#71)
- NPCs who greet you by reputation name one you actually have; before, they
  could pick an unrelated one. (#97)
- A map event meant to fire once removes itself as intended; before, it fired
  again every time you entered that area. (#303)
- A conversation started by a script opens with the NPC's correct story state;
  before, its first screen could show the wrong options. (#72)
- Scripts that ask what kind of weapon you hold get the right answer; before,
  they read the wrong picture and could misjudge it (three scripts). (#73)
- Male NPCs tell their stories in the wording meant for your character's
  gender; before, the male and female versions were swapped. (#74)
- When you cannot pay alone, your followers chip in only what is still owed;
  before, each was asked for the full remainder. (#75)
- A newspaper story that has already run can be printed again when the game
  asks for it; before, it was silently skipped. (#296)

**Characters, skills and followers**

- Your reputations change how NPCs react to you; before, the reputation file
  was misread and they had no effect. (#96)
- A follower told to wait waits for a charismatic leader and eventually gives
  up on an ordinary one; before, it was the other way round. (#80)
- A follower's Wait Here order that runs out ends that follower's wait; before,
  it named no follower, and could crash the game. (#372)
- Followers judge an enemy's strength by the original level bands; before, they
  treated any equal-level foe as stronger and cast spells at it. (#81)
- A master of Persuasion can no longer recruit beyond the follower limit set by
  Charisma, as was possible in release 1. (#82)
- Repair reports success whenever it repairs something; before, a failed roll
  still repaired but said it failed, and a master's fumble cost durability.
  (#89)
- Heal needs a healing item such as bandages or a kit; before, it worked with
  nothing in hand. (#90)
- Auto-levelled characters raise stats to their planned targets; before, race,
  background and other bonuses made them stop early or overshoot. (#91)
- On Hard, every experience award, quests included, gives three quarters of
  the experience; before, it gave half. (#377)
- Weldo's follower orders ("I need you to do something...") are offered while
  he follows you; before, they were hidden then. (#380)
- Killing or robbing good people can drive your alignment as low as the
  original allows; before, it stopped well short. (#93)
- When a follower volunteers to pick a lock, the follower does the picking;
  before, the player character picked it instead. (#216)
- Gambling for worn items or items a merchant won't sell checks your own
  gambling training; before, it checked the merchant's. (#262)
- Outside combat, followers refuse an order to attack one of your own party;
  before, they always obeyed and turned on their companions. (#299)
- Light levels match the original, so darkness penalties to skills and movement
  are right; release 1 could judge a spot one level brighter. (#347)
- A follower who leaves says its leaving line; before, it spoke a warning line
  instead. (#83)
- The original rule that undead or mechanical followers cannot use Heal is
  restored as an option; release settings keep the release 1 rule. (#85)
- An unconscious player can no longer start using a skill, as release 1
  allowed. (#98)
- The "skill already at its minimum" message appears on the character screen
  when it should; before, the condition was reversed. (#235)
- A sleeping character's spoken commands come out as sleepy mumbling; before,
  they were said, and obeyed, word for word. (#322)

**Crashes and hangs**

- Having a follower who has followers of their own no longer freezes the game;
  before, building the party list looped forever. (#177)
- The map-area cache shifts only its live entries when it drops one; release 1
  moved too many, eventually spilling past its memory as the cache filled.
  (#190)
- Skipping a movie with Shift, Ctrl, an arrow or an F-key is safe; before, it
  could crash the game, and Escape waited on X. (#369)
- If the roof art list fails to load, the game no longer reads from an empty
  table, which could crash; release 1 kept a stale roof count. (#172)

**Saving and loading**

- Switching to a different game module without restarting unloads the old one's
  files; release 1 kept the previous module's archives loaded underneath the
  new one. (#196)
- Objects in changed wilderness areas load where they belong; before, they
  could be placed near the corner of the world instead. (#305)
- A map whose saved list of moving objects is cut short fails to load; release
  1 loaded the damaged data as if it were good. (#157)
- The object consistency check repairs a mismatched inventory count from the
  actual item list and rejects broken entries; release 1 trusted the wrong
  count and kept them. (#175)
- When part of a changed object fails to load, the game finishes reading that
  record before giving up; release 1 stopped in the middle of it. (#180)
- Saved games of an incompatible version are refused instead of loaded; release
  1 loaded any version and could leave the file open on a failed read. (#195)
- Saves no longer record every light in outdoor areas as changed; before, they
  were written again into every save, making saves bigger. (#289)
- Moving between maps no longer adds your character and followers again to the
  old map's list of removed objects on every trip. (#292)
- Saves no longer write unchanged map scripts again after a reload; release 1
  kept re-saving them, making save data larger for no reason. (#295)
- A save with a damaged file index fails to load cleanly; release 1 could load
  it anyway and carry on with bad data. (#328)
- Removing a game folder applies to every data folder the game uses; release 1
  removed it from the first one only, or from the wrong place. (#330)

**Items, inventory and trade**

- Nice Clothes and similar effects make NPCs like you more; before, the effect
  was read from the NPC and did nothing. (#95)
- Items that cannot be dropped can no longer be stolen, traded or moved by
  scripts; release 1 allowed it. (#108)
- Smashing or destroying an unopened chest spills its contents; before, some
  such chests left nothing. (#109)
- A merchant's goods dropped outside the trade window go back to the merchant;
  before, they landed on the floor, yours for nothing. (#229)
- Throwing an item straight from the inventory works; before, the item fell off
  the cursor and the throw never happened. (#230)
- A quoted trade price is forgotten once the deal ends; before, it could carry
  over to the next item you dragged. (#265)
- Examining an item keeps its effect description; before, the skill bonus line
  overwrote it. (#276)
- Examined magic items show their remaining charges as a number, and "+"
  otherwise; before, the two were swapped. (#277)
- An item's "Uses" line shows the uses left; before, it showed the item's skill
  bonus instead. (#278)
- A potion with no spell is used up whoever drinks it; before, only the
  player's was used up. (#110)
- A character with even one crippled arm can no longer take up a shield, as
  release 1 allowed. (#111)
- Picking up an item checks the item is still there; release 1 checked the
  person picking it up instead. (#120)
- Dead or unconscious characters can no longer use items; release 1 stopped
  only owners that were switched off yet alive and conscious. (#215)
- If the item you are wearing cannot be removed, the item you dragged onto it
  returns to your pack; before, it stayed stuck on the cursor. (#231)
- Asking for a free service or item no longer gets the "not enough money"
  reply; before, the merchant complained about money anyway. (#232)
- When an item cannot be moved, the reason is shown to you; before, the message
  never appeared. (#257)
- Repairing an inventory with a duplicated item removes only the duplicate;
  before, it could also remove a genuine item. (#263)
- That same inventory repair tidies the owner's pack rather than the item being
  dropped (rare case). (#264)
- A hotkey whose item has been replaced by a matching one throws and drags
  based on the hotkeyed item. (#272)

**Movement and animation**

- Click-to-move stops at whatever blocks the path; before, your character
  walked into blockers. (#128)
- Characters closing in during combat no longer drop an extra step of their
  plan; release 1 discarded one step too many, which could cancel what came
  next. (#141)
- NPCs asked to step aside for the player swap places; before, they often
  refused. (#122)
- Characters at one particular movement speed animate at the intended pace;
  before, they used the art's own rate. (#123)
- Your character fidgets when idle, like everyone else; before, the player
  never fidgeted. (#124)
- Approaching an enemy in combat handles the move as the original does; release
  1 changed the wrong setting on each approach. (#125)
- A knocked-down character no longer stands up while inactive or off its turn
  in turn-based play, as release 1 allowed. (#126)
- A move ending next to a living creature stops when it reaches it; before, the
  step went on to further checks. (#130)
- The player can be revealed from concealment by the automatic animation;
  release 1 refused it. (#136)
- Jump animations start from their proper first frame; before, a jump could
  begin part-way through whatever animation the character was already playing.
  (#137)
- A character stepping aside to let someone pass checks each direction
  properly; release 1 judged its first choices using only the way it was
  already facing. (#138)
- Walking routes get the original length limit, a little more than requested;
  release 1 replaced the requested limit with the absolute maximum, allowing
  longer routes than intended. (#140)
- Characters fleeing or spreading out move one tile further away; release 1
  stopped them a tile short. (#142)
- Paralysed or stunned characters show their proper looping animation, and its
  sound plays; release 1 could show the wrong pose and play the sound
  differently. (#144)
- Characters walking up to someone plan their first route with the original
  settings, and try a third route when the second falls short; release 1
  accepted a too-short route. (#145)
- Objects and characters turned to stone keep their looping animation's
  nearby-player check; release 1 treated anything petrified as never near the
  player. (#146)
- Characters turning exactly half way round turn the same way the original
  does; release 1 spun them round in the opposite direction. (#147)
- When an animation adds a sub-step, the game tracks the new step; release 1
  kept pointing at the previous step's details. (#150)
- Each newly started animation begins with a clean slate; release 1 could carry
  over a leftover step and target tile from an earlier one. (#151)

**Interface and screens**

- Shortened text on menu screens ends with its dots after the cut-off words;
  release 1 drew the dots over the text's first letters instead. (#199)
- The button for a maintained spell updates and disappears when the spell ends;
  release 1 left the button showing. (#206)
- A follower's health bar shows its poisoned look at any level of poison;
  release 1 waited until poison passed 20. (#208)
- The combat bar in the full interface animates during NPCs' turns; release 1
  left it still whenever an NPC acted. (#210)
- Clicking to close the follower menu no longer also clicks whatever lies
  underneath; release 1 passed that click on to the window behind it. (#214)
- Clicking the portrait lens to leave the Options screen closes it; before, the
  click usually left the screen open. (#221)
- Dragging a scroll bar up by a single line works and overshooting stops at the
  end; before, a one-line drag was ignored and overshoots went through. (#224)
- Text too long for a menu text box, such as a save name, shows its end with
  the cursor; before, the whole text vanished. (#247)
- L opens the logbook, and game hotkeys no longer fire while the main menu is
  open; before, L did nothing. (#267)
- The chat and centre-view keys work when you are not typing; before, they
  worked only while a text box was being typed in. (#270)
- Settings are read and saved cleanly; before, arcanum.cfg gained a blank line
  per setting on every save and "key = value" lines were misread. (#300)
- The logbook records injuries dealt to you by creatures and people; before,
  none of those injuries ever appeared there. (#301)
- Without a HighRes config file the map scrolls at the original 60 steps a
  second; release 1 defaulted to 35, scrolling noticeably slower. (#325)
- Pressing Enter or Escape in a confirmation box affects only that box; before,
  the key also reached the save or load menu behind it. (#361)
- The brightness setting changes the screen; before, the slider was offered and
  saved but did nothing. (#366)
- After buying a newspaper the old "no thanks" reply disappears; before, it
  stayed on screen. (#76)
- The screen updates when your fate points change; before, the shown count
  could lag behind. (#88)
- The level-up message lists spells learned automatically; before, they were
  left out. (#92)
- Floating damage numbers read number first; release 1 showed them back to
  front, like "HT 12". (#104)
- A sprung trap shows "Trap!" over the victim, as it did in the original game.
  (#112)
- The progress bar while saving and loading advances once for every step;
  release 1 moved it only for some steps, so it jumped. (#197)
- Scrolling the world map up and down stops at the map's real edge; release 1
  worked out the bottom limit from the wrong measurement. (#200)
- Books and notes begin each new page on a word; release 1 started pages with a
  stray space or blank line. (#201)
- Text typed into a box too long to show stays within the box; release 1 let
  the text and its cursor run past the edge. (#202)
- The combat indicator sits in its right place from the start; release 1 drew
  it two pixels too high until the screen was resized. (#209)
- The compact interface's health bar has a see-through background; release 1
  drew a black box behind it. (#211)
- The compact interface's health bar fills to its last row; release 1 stopped
  one row short. (#212)
- The logbook lists a blessing before a curse received at the same moment or
  later; before, the later of the two came first. (#217)
- With both arms crippled, the logbook's injury page shows both arm injuries as
  current; before, it found the same entry twice and marked one healed. (#218)
- The logbook keeps one scar entry marked current for each scar you carry;
  before, only the most recent scar was shown as current. (#219)
- A logbook entry exactly as tall as the space left on a page moves to the next
  page; before, it was squeezed onto the current one. (#220)
- Turning the mouse wheel over a scroll bar no longer leaks through to whatever
  lies beneath it, even when nothing scrolls. (#225)
- Clicking a scroll bar's track just above the handle pages up correctly;
  before, the clickable area was measured from the wrong place on some lists.
  (#226)
- Clicks inside the inventory are kept by the inventory; before, they also fell
  through to other checks behind it. (#233)
- Lowering a technological degree on the character screen no longer also clicks
  whatever lies beneath the button, matching how raising one behaves. (#234)
- Menu text that is too wide is trimmed correctly and never trimmed to nothing;
  before, text with tabs was misjudged. (#248)
- Using an item that closes the inventory no longer lets the same right click
  reach the game world behind it. (#254)
- Hovering over the rest button is handled by the button, like the other
  buttons (minor). (#268)
- The inventory button press is kept only when the inventory actually opens
  (minor). (#269)
- Closing a loot or theft window drops any item being dragged and restores your
  interface setting; before, the full interface stayed forced on. (#271)
- The moon on the clock follows the calendar; before, a mistyped day length
  made it run about fourteen hours ahead each lunar month. (#273)
- Hovering off an object keeps the description of the object you were examining
  and clears stale highlights correctly. (#274)
- Long lines in the message window are shortened to at least one letter instead
  of disappearing completely. (#279)
- The "cannot scroll further" cursor points the way you tried to scroll,
  including diagonals, and down-left sits in the corner. (#288)
- An NPC who shares your portrait shows its own or none; before, it could show
  your portrait instead. (#318)
- Releasing a mouse button that was never pressed in a window no longer counts
  as a click there; release 1 reported it as one. (#355)
- The pixel just past a window's right or bottom edge no longer counts as
  inside it; release 1 counted it. (#359)

**Drawing and sound**

- Footsteps sound different depending on the ground underfoot; before, every
  tile in the game used the same footstep sound type. (#171)
- Invisible and translucent characters and objects fade; release 1 applied
  their see-through look only when another blend was already set. (#181)
- Small and medium sounds in the world play at 40% and 70% of full volume;
  before, they could play at full volume. (#310)
- The back half of two-part effects such as Reflection Shield is drawn behind
  the body; before, it covered the character. (#326)
- Spell glows made only of light animate and end properly; before, they ended
  at once or never. (#132)
- Fire-damage spell glows made only of light animate and end properly. (#133)
- Reverse fire-damage spell glows animate and end properly; before, some never
  ended. (#134)
- Blood pools form under the wounded creature; before, they sat at the corner
  of its tile. (#135)
- When the view moves, the map's parts update in the original order, scrolling
  first; release 1 updated them in reverse order on every view change. (#159)
- Creatures blown apart use the ordinary explosion art; release 1 treated every
  exploding creature as female when looking up the animation. (#163)
- Building-front art is always looked up; release 1 refused any building front
  whose art type did not match, which could leave it undrawn. (#165)
- One outdoor ground-tile variety table is filled to its own length; release 1
  used another table's count, filling it too far or leaving it short. (#169)
- The first piece of a damaged wall uses its left-side damage art; release 1
  drew the right-side damage art there. (#170)
- The object lighting setting takes effect; before, the game always behaved as
  if it were switched off, whatever the setting said. (#178)
- Effects drawn over objects at full size are no longer put through a resize
  step; release 1 resized every one, which could shift them slightly. (#179)
- Scaled characters wading in water refresh their part of the screen; release 1
  skipped that refresh, which could leave leftover pixels. (#182)
- Rotating lights keep their correct position each frame; release 1 kept adding
  the frame's offset, so the light drifted. (#183)
- Shadows cast by several lights are ordered as in the original; release 1
  sorted equal shadows into a different order. (#184)
- Lights marked both indoor and outdoor use their indoor colour; release 1
  could take either colour. (#185)
- The game works out correctly which tiles fall within the screen; release 1
  was one tile off for much of the screen. (#186)
- Scroll bar handles are drawn at their full height; before, they were drawn
  about one seventh as tall as they should be. (#227)
- The background behind world map notes is tiled across its area; before, every
  tile was piled at the first one's spot. (#245)
- Starting to drag an item shows the correct cursor under it; before, whatever
  cursor was showing was carried along. (#255)
- Every completed trade plays its sound, haggled, priced or free; before, only
  a plain sale played it. (#256)
- Portraits, panels and the follower's figure in the inventory screen are drawn
  at their proper height; before, their width was used as the height. (#258)
- The figure overlay in the inventory screen sits three pixels right of its
  panel. (#259)
- Slot labels in the inventory screen sit at the bottom of their art; before,
  they were drawn at the top. (#260)
- Creature sounds go quiet whenever your character is out of the world; before,
  a hidden creature silenced only itself. (#287)
- The volume limit for quiet, small sounds is read correctly from the sound
  settings; release 1 filed it under medium sounds and left small unlimited.
  (#309)
- Distance to a sound is rounded, so its volume fades evenly; release 1 could
  misjudge it by one step at the edge of hearing. (#311)
- Background ambience plays one sound per area per moment; release 1 played
  them all at once, making ambience busier than intended. (#312)
- Blood splatters and weapon effects stop at their small limit; before, they
  piled up without end during a long fight. (#315)
- Spell effects missing a back or ground layer no longer try to draw the
  missing part; release 1 placed it anyway. (#316)
- Scaled sprites are positioned with rounding; release 1 could place them a
  pixel off. (#342)
- A scaled sprite cut off by the screen edge shows the right part of its
  picture; before, it could show the wrong slice. (#343)
- The see-through checkerboard on mirrored sprites matches the original;
  release 1 drew the opposite half of the pattern. (#344)
- The see-through checkerboard on scaled sprites is even; before, its pattern
  came out irregular. (#345)
- Averaged blending of sprites uses the right strength; release 1 garbled the
  red and green of the blend. (#346)
- The character view lens repaints exactly the area it drew; release 1
  repainted a wrongly shaped area. (#358)
- Screen fades start from the current screen and finish fully on their colour;
  release 1 restarted them and stopped just short. (#365)
- Floating text over characters fades out smoothly; before, it stayed at full
  strength until it vanished. (#367)
- Scaled images cut by the screen edge line up; release 1 could draw them a
  pixel off. (#368)

**Fixes with no effect in ordinary play** (103), listed for completeness:

- **The world editor and modding** (16): saving a map's objects, terrain codes,
  the sector grid, wall lines and deleting parts of long walls in the editor;
  modded text files with a stray carriage return; two-part light colours in
  modded effects. (#155, #161, #162, #166, #187, #188, #191, #192, #291, #297,
  #320, #321, #323, #324, #337, #339)
- **Screens and menus** (10): buttons and options with missing art or values,
  scroll-bar bookkeeping, a race-list search, a stray Escape after quitting,
  and windows that fail to open. (#203, #204, #205, #207, #213, #222, #223,
  #228, #249, #252)
- **Drawing and sound** (7): partial world-map redraws, menu animations,
  equipment slots whose art fails, sound balance, looping overlay sounds,
  effect sizes and sound cross-fades, none of which shipped data reaches.
  (#246, #251, #261, #313, #314, #317, #364)
- **The world and travel** (4): map preparation without a module, the town map
  link after switching modules, an unused town-map scroll, and a large scenery
  object built without its data file. (#154, #241, #243, #280)
- **Crash guards no play reaches** (3): text boxes without a change handler, a
  drawing failure on the save list, and windows without an input handler.
  (#236, #250, #356)
- **Spells, saving, items and characters** (9): two spell targeting rules no
  shipped spell uses, two save consistency checks, stack weights and generated
  grenade prices, an NPC's second weapon check, and the default character name.
  (#307, #308, #193, #198, #107, #281, #78, #282)
- **Game text** (1): text files containing the Russian letter "ya" are no
  longer cut short there; no English file has one. (#293)
- **The engine's own workings** (54): routines nothing calls, multiplayer-only
  paths, debug messages, error codes, file and folder helpers, memory counting
  and message bookkeeping, each as the original has it. (#59, #86, #87, #99,
  #119, #121, #127, #131, #139, #148, #149, #152, #153, #156, #158, #160, #164,
  #167, #168, #173, #174, #176, #189, #194, #253, #284, #285, #286, #290, #294,
  #304, #327, #329, #331, #332, #333, #334, #335, #336, #338, #340, #341, #348,
  #349, #350, #351, #352, #353, #354, #357, #360, #362, #363, #370)

### 3.3 Mistakes in the decompilation, fixed before release 1

The same kind of fix, found earlier, while the code was being named and
tested; release 1 already had these. Each line says what is right, then
what was wrong before.

**Combat**

- Dodge gives two points per skill level; before, it gave five, so a skilled
  dodger avoided nearly every hit. (#49)
- NPCs choose their weapon by its total damage; before, they judged by fatigue
  damage alone, so a guard would draw a mace over a fine sword. (#52)
- In turn-based combat a knocked-down character gets up on its own turn, with
  the animation and its action-point cost; before, it rose free during others'
  turns. (#55)
- When an NPC makes peace, a character in the middle of an attempted attack
  stops; before, only some kinds of attack were called off. (#16)
- Running during combat costs one point of fatigue each interval; before,
  running in combat was free. (#30)
- In combat mode a plain click on a corpse does nothing and Alt-click loots it;
  before, the plain click started an unpredictable action. (#47)
- Attacking your own follower in combat asks for confirmation, with Alt to
  force it; before, a click simply attacked them. (#48)
- NPCs pick the best of the weapons that can reach their target; before, they
  took the last such weapon in their pack. (#53)
- NPCs deciding whether to flee weigh each party member's own level; before,
  they counted the leader's level once per member. (#54)
- Carrying a small light reduces the darkness penalty by the original amount;
  before, it was reduced slightly too much, by up to two points. (#50)

**Spells and magic**

- Spell projectiles light up and play their sound where they hit; before, that
  impact light and sound never happened at all. (#13)
- Items that act only on their user take effect directly; before, they were
  routed through the spell-casting process instead. (#19)
- Magic items that pay for a spell with fatigue charge that spell's cost;
  before, they charged another spell's cost and the cast could fail. (#29)
- Each spell starts with its caster's reflection settings cleared; before, the
  previous caster's could carry over (effect unmeasured). (#44)

**World, maps and travel**

- Random encounters respect their maximum character level; before, that limit
  was never read, the minimum being read twice instead. (#24)
- Monsters in a random encounter group spread over neighbouring tiles in the
  original's pattern; before, each one was shifted diagonally. (#25)
- On Linux, mod archives with folders whose names begin another folder's name
  find every file; before, some existing files were reported missing. (#34)

**Dialogue, quests and scripts**

- Dialogue replies can be clicked with the mouse; before, their click areas
  were placed outside the window, all on one row, and did nothing. (#9)
- Game scripts divide numbers; before, every division in every shipped script
  added the two numbers together instead. (#45)
- NPCs such as H.T. Parnell and Nasrudin pick the special greetings their
  dialogue files ask for; before, those greeting choices were misread and
  skipped. (#22)
- Low-intelligence characters read the simple-language quest log written for
  them; before, they saw the normal text and the dumb version was never used.
  (#39)
- A newspaper headline repeated in dialogue no longer gains a stray full stop
  after its own '!', '?' or '.'; before, every headline got one. (#18)

**Crashes and hangs**

- Reaching the experience needed for the level cap stops at the cap; before, it
  crashed the game outright. (#56)
- A message file line of 2,000 or more characters, as a mod might contain, is
  cut off; before, it could crash the game. (#11)

**Saving and loading**

- Saves record how often each random encounter has been met; before, those
  counts were lost and every save carried 1.7 KB of junk. (#31)
- Reading an empty part of a file no longer leaves it unusable for later reads;
  before, the file was stuck afterwards. (#36)
- On Linux, deleting and renaming game files finds them whatever the capital
  letters, as opening does; before, such files were left behind. (#46)
- On Linux, the game recognises its own module archive, so map caches and saves
  record which module wrote them; before, they recorded nothing. (#57)

**Items, inventory and trade**

- The last-listed shop stock, the Multi-Magic (Dark) set on the stock game,
  restocks with goods; before, any merchant or container using it stayed empty.
  (#12)
- Characters with exactly zero magical or technological aptitude can use magic
  and tech items; before, some such items refused them. (#20)
- Winning a gamble gives you the item with the winner's message; before, a win
  was handled the same way as a loss. (#26)
- Alt-click dragging an item onto your tile leaves items held inside an
  inventory alone; before, the check meant to skip them never did. (#37)

**Movement and animation**

- Characters no longer fidget in the background while the main menu has the
  game paused; before, their idle animations kept playing behind it. (#7)

**Interface and screens**

- Moving a window moves its buttons with it; before, a ghost copy of the
  interface bar appeared at any screen height but 600. (#10)
- Six early copying mistakes match the original: a floating-text effect,
  wandering in the dark, the skill button under the mouse, an overwrite prompt,
  lock files, a map check. (#1-#6)
- Main-menu screen text containing a tab is shown in full; before, every such
  line was cut off where the tab began. (#8)
- The recent-actions button for a used item gets its art; before, it was given
  no art at all. (#27)
- The last line of a message file without a line ending no longer gains junk
  characters; before, 60 shipped message files were affected. (#35)

**Drawing and sound**

- Lights stored with a map area shine where they were placed; before, they were
  put at tiny coordinates near the map's corner. (#23)
- Spell effects play their sounds; before, every sound attached to a visual
  spell effect was silent, through three separate mistakes. (#33)
- Door and portal art takes its colour set from the direction it faces; before,
  the requested colour set was kept. (#32)
- Pictures clipped at the left or top of the screen or window are cut off
  cleanly; before, they drew a shifted part of the image. (#41)
- On Linux, the few sounds with no file assigned simply stay silent; before,
  the game tried to load a folder as a sound. (#51)

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
    active spells, timed events, world-map notes (town notes too, new in
    release 3) and visited areas;
  - game archives with impossible sizes, overlong names or names that never
    end;
  - art with impossible frame sizes or frame counts (counts new in release
    3), or pixels that run past the frame;
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

### 4.3 Built, but in no mod yet

- **Aimed throws** (`called shot throws`): built, but in no mod and not yet
  playtested.

## 5. Running the game

**Platform**

- **Native on Linux and Windows**, through SDL3 instead of DirectX.
- **macOS, as yet untested** (new since release 1): a package built on
  GitHub's macOS machines from the same source. I am working on it; reports
  from Mac players are welcome.
- **A resizable window or full screen**, at the High Resolution Patch's
  resolution.
- **Save thumbnails** are centred and smoothed at any resolution.
- **The screen is scaled smoothly** when the window is larger than the game's
  resolution (new in release 3); `scale mode` in `arcanum-ce.cfg` picks 0
  nearest, 1 linear (the default) or 2 pixel art.
- **Busy scenes draw faster** (new in release 3): art lookups, portraits and
  NPCs' searches for targets are remembered instead of repeated.
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

- **Movies play only on Windows**, through the game's own Bink library. On
  Linux and macOS the logos, the intro, the ending and the in-game
  cinematics are skipped.
- **Multiplayer** is not supported at present.

## 6. The original's quirks, kept on purpose

CE keeps the original's behaviour unless this catalogue says otherwise. The
source records over 600 places where the original does something odd and CE
does the same. These are the ones a player can meet.

### 6.1 Kept in every configuration

**Combat**

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
- **Every melee, bow and thrown fumble hits its wielder**, half the time
  critically. Combat Rebalance (weapon fumbles).
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
