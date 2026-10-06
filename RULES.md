# Evolving Life: Rule Set

Oct 5, 2026 · @Device

## Overview

Evolving Life is a grid world, descended from Conway's Game of Life, in which no species, predator, parasite or multicellular body is written into the rules. Every cell carries 20 genes, and those roles have to evolve from them.

The rules follow four principles:

- **Local only.** A cell senses at most 2 sites away and acts only on the 8 sites around it.
- **No roles in the rules.** The rules never mention a hunter, a host, a plant or a species. They only define flows of matter that genes can tilt.
- **A sealed world.** Like a terrarium, the world holds a fixed amount of matter that only changes form: food, minerals and living cells. Nothing enters but light.
- **Replayable.** All randomness comes from one seed, so the same seed always produces the same history.

The document sets out the rules first, and they do not depend on any programming language. The last two sections describe the finished application and recommend one way to build it.

Every number below is an untested starting value. Expect to tune them once an engine exists; the structure of the rules should not need to change.

## The world

The world is a 128 × 128 grid of sites whose edges wrap around, so there are no borders or corners.

- **Neighbors.** Each site has 8 neighbors: the sites touching it by an edge or a corner.
- **Food.** Each site holds an amount of food A and an amount of food B, with no upper limit.
- **Minerals.** Each site also holds an amount of minerals: the used-up form of food. Cells cannot eat minerals, but light can turn them back into food.
- **Occupancy.** Each site is empty or holds exactly one cell.
- **Temperature and light.** Every site has a temperature between 0 and 30 °C and a light level between 0 and 1. Both depend on its row and on the season.
- **Time.** Time advances in ticks, and each tick runs a fixed sequence of phases. When two cells want the same empty site, the seeded random source picks the winner.

## The cell

The unit of life is the cell: one cell occupies one site and carries a genome of 20 genes that never change during its life.

### State

- **Stores.** An amount of A and an amount of B, each from 0 to 50. These are its energy. Anything it receives beyond 50 falls onto its own site as food.
- **Body mass.** 4 A and 4 B locked into its structure at birth and released as food when it dies.
- **Age.** Ticks since birth.
- **Cooldown.** Ticks until it may divide again.
- **Stress.** A value from 0 to 1 that rises when it is attacked, infected or underfed and fades over time.
- **Bonds.** The neighboring cells it is physically attached to.
- **Identity.** A unique ID and the ID of its parent, or of both parents.

* **Infection.** A cell is healthy or carries one virus.

### Genome

| Group | Gene | Range | What it controls |
| --- | --- | --- | --- |
| Identity | Tag | 0 to 1, circular | Identity marker, shown as the cell's color. It has no effect by itself. |
| Identity | Tolerance | 0 to 0.5 | How different another cell's tag may be before it stops counting as kin. |
| Feeding | Harvest | 0 to 1 | How much it feeds on food lying around it. |
| Feeding | Diet | 0 to 1 | What it feeds on: 1 is only A, 0 is only B, 0.5 is both, poorly. |
| Feeding | Photosynthesis | 0 to 1 | How much food it makes for itself from light and minerals. 0 means none. |
| Feeding | Preferred temperature | 0 to 30 °C | The temperature at which it works best. |
| Conflict | Attack | 0 to 1 | Strength used to drain other cells. |
| Conflict | Defense | 0 to 1 | Resistance to being drained. |
| Conflict | Resistance | 0 to 1 | How well it avoids catching a virus and how fast it clears one. |
| Body | Adhesion | 0 to 1 | Chance that a daughter stays attached to its mother after division. |
| Body | Share | 0 to 1 | Fraction of its surplus it passes to the cells it is bonded to. |
| Body | Role split | -1 to 1 | How differently it behaves on the outside of a body versus the inside. 0 means no difference. |
| Behavior | Motility | 0 to 1 | Chance per tick that a free cell moves. |
| Behavior | Appetite | 0 to 1 | Pull toward food when choosing where to move. |
| Behavior | Caution | 0 to 1 | Push away from cells that could drain it. |
| Behavior | Boldness | 0 to 1 | Pull toward cells it could drain. |
| Behavior | Sociability | -1 to 1 | Pull toward kin (positive) or away from them (negative). |
| Behavior | Dormancy | 0 to 1 | How readily it shuts down when it is too cold, too hot or there is nothing to eat. 0 means never. |
| Heredity | Mutability | 0 to 1 | How strongly its stress raises the mutation chance of its offspring. |
| Heredity | Mating | 0 to 1 | Chance that a released daughter is made with a partner instead of as a clone. 0 means never. |

The tag is circular: 0.98 and 0.02 are 0.04 apart. The largest possible distance between two tags is 0.5.

### Bodies

A body is a set of cells connected by bonds; a cell with no bonds is a free cell.

- Bonds form only at division, when a daughter stays attached. They break only when one of the two cells dies.
- If a death splits a body into disconnected pieces, each piece becomes a body of its own.
- Bonded cells are anchored and cannot move. Only free cells move.
- Every cell of a body descends from one founding cell, so a body is a clone apart from mutations.

## One tick

Every tick runs the same ten phases in this order.

1. **Environment.** The season advances, sparks turn minerals into food, old food spoils, disasters apply.
2. **Sense.** Each cell reads its temperature and supply, decides whether it is dormant, and classifies its neighbors.
3. **Move.** Free cells may step to a neighboring empty site.
4. **Feed.** Cells take food from their surroundings or make it from light, and leak some of it to neighbors.
5. **Attack.** Cells drain their prey.
6. **Share.** Cells pass surplus along their bonds.
7. **Infect.** Viruses arise, spread between neighboring cells and are cleared.
8. **Upkeep.** Cells pay their running costs and age. Those that cannot pay die and become food.
9. **Divide.** Ready cells divide.
10. **Record.** Births and deaths are written to the lineage record.

## The rules

Eight rules govern everything a cell does. Each one is a flow of food, or a choice, that the genes can tilt.

### 1. Sense

Each cell first works out its own condition and what surrounds it.

- **Reach.** A cell's reach is its own site and the empty sites next to it.
- **Thermal efficiency** = 1 - ((temperature - preferred temperature) / 15)², never below 0. It is 1 at the preferred temperature and 0 when the site is 15 °C or more away from it.
- **Supply** = (the food within reach that its diet lets it eat + the minerals within reach × light × its photosynthesis) / 2, at most 1.
- **Dormancy.** A cell is dormant in any tick in which its thermal efficiency or its supply is below its dormancy gene. A dormant cell does not move, feed, attack, share or divide. It does not age, its upkeep drops to one tenth and its defense doubles.
- **Kin.** A cell regards another as kin when the distance between their tags is at most its own tolerance. Recognition is one-way.
- **Prey and threats.** A cell treats a neighbor as prey when it does not regard it as kin and its own attack is higher than the neighbor's defense. It never treats a cell it is bonded to as prey. A threat is any cell that treats it as prey.
- **Role.** A cell is an **inner** cell when it is bonded to a cell on each of its 8 neighboring sites. Otherwise it is an **outer** cell. A free cell is always an outer cell.

The role split gene (r) then adjusts three genes for the rest of the tick:

| Gene | Outer cell | Inner cell |
| --- | --- | --- |
| Harvest | × (1 - r) | × (1 + r) |
| Attack and Defense | × (1 + r) | × (1 - r) |

A positive r makes outer cells fight and inner cells feed. A negative r does the reverse. Every later rule uses these adjusted values.

### 2. Move

An awake free cell moves with a chance equal to its motility each tick, stepping to the empty neighboring site with the highest score.

Score of a site = appetite × food + boldness × prey - caution × threats + sociability × kin

- **food** is the food on that site the cell can eat (diet × A + (1 - diet) × B), divided by 20, at most 1.
- **prey**, **threats** and **kin** are each the number of such cells next to that site, divided by 8.
- Ties are broken at random, so a cell that senses nothing wanders at random.
- With no empty neighboring site, it stays where it is.

This is how real microbes navigate: a random walk, biased by what they sense.

### 3. Feed and leak

An awake cell feeds from its own site and from the empty sites next to it, and its diet decides what it can take.

- Capacity for A per tick = 2 × harvest × diet² × thermal efficiency.
- Capacity for B per tick = 2 × harvest × (1 - diet)² × thermal efficiency.
- It takes from its own site first, then equally from the empty sites in reach.
- When several cells ask one site for more than it holds, it is split in proportion to what each asked for.
- **Photosynthesis.** An awake cell also turns minerals within its reach into food, up to 2 × photosynthesis × light × thermal efficiency units per tick. A share equal to its diet becomes A and the rest becomes B.
- **Leak.** A cell keeps 80% of what it takes or makes. The other 20% passes into the stores of the cells next to it, split evenly, kin or not. With no cell next to it, it keeps everything.

The squares make specialists efficient: a pure A eater can take 2 units a tick, while a half-and-half eater takes 0.5 of each. The leak is what lets two lineages feed each other, and what a freeloader can live on.

### 4. Attack

An awake cell drains every neighbor it treats as prey.

- Drain per tick = 3 × (attack - defense) × the attacker's thermal efficiency, taken from the victim's A and B in proportion to what it holds.
- The attacker keeps half of what it drains. The other half falls onto the victim's site as scraps.
- **Satiation.** An attacker never drains more than twice the room left in its stores. A cell with full stores does not attack.
- If several attackers together would take more than the victim holds, each drain is scaled down by the same factor.

### 5. Share

An awake cell passes part of its surplus to the cells it is bonded to, and to no one else.

- For A and B separately: if a store is above 10, it gives away share × (store - 10).
- The gift is split evenly among its bonded cells.

Sharing is what lets a body feed inner cells that cannot reach food. A cell whose share gene mutates to 0 keeps receiving without giving, which is the rule set's version of cancer.

### 6. Infect

Viruses spread only between neighboring cells whose tags they match, which makes them the check on whatever lineage becomes common and crowded.

- **Virus.** A virus is nothing but a tag. It can infect a cell, free or bonded, when the distance between the virus tag and the cell's tag is at most 0.05.
- **Outbreak.** Each tick, every healthy awake cell has a 1 in 1,000,000 chance that a new virus arises in it, carrying the cell's own tag.
- **Spread.** Each tick, an infected cell passes its virus to each neighboring healthy cell that the virus matches, with a chance of 20% × (1 - that neighbor's resistance).
- **Drift.** Each time a virus passes to a new cell, its tag has a 5% chance to shift by up to ±0.02. This is how a virus follows a host lineage whose tag is changing.
- **Burden.** An infected cell pays an extra upkeep cost for as long as it carries the virus.
- **Recovery.** Each tick, an infected cell clears its virus with a chance of 5% × its resistance. It can be infected again later.
- A cell carries at most one virus. A daughter is born healthy. Dormant cells neither catch nor pass a virus.

Free cells keep moving and rarely stay beside a match, so viruses barely touch them. A body is a block of clones in permanent contact, so one outbreak can run through all of it. A lineage escapes when its tag mutates out of the virus's reach, and the virus dies out when it runs out of hosts.

### 7. Upkeep, stress and death

Every tick a cell pays a running cost, and it dies in the tick it cannot pay in full.

| Cost item | Per tick |
| --- | --- |
| Being alive | 0.2 |
| Harvest | 0.2 × harvest |
| Attack | 0.5 × attack |
| Defense | 0.3 × defense |
| Crowding | 0.03 for each occupied neighboring site beyond 3 |
| Aging | 0.001 × age |
| Moving | 0.2 if it moved this tick |
| Infection | 0.3 while it carries a virus |
| Photosynthesis | 0.3 × photosynthesis |
| Resistance | 0.2 × resistance |

- **Temperature.** The total is multiplied by 0.5 + temperature / 30, so life runs cheaper in the cold and dearer in the heat.
- **Dormancy.** A dormant cell pays one tenth of the total.
- The cost is paid from the larger store first, then from the other. A cell can survive on A or B alone; only division needs both.
- **Waste.** Everything a cell pays turns into minerals on its own site.
- **Death.** A cell that cannot pay dies. Its remaining stores and its body mass of 4 A and 4 B fall onto its site as food, and its bonds break.

Stress is updated for awake cells after the cost is paid:

- First it fades: stress is multiplied by 0.9.
- **Predator threat.** It rises by 0.2 if the cell was drained this tick.
- **Disease.** It rises by 0.1 if the cell carries a virus.
- **Poor environment.** It rises by 0.05 if the cell took in less than half its feeding capacity this tick.
- It never exceeds 1.

### 8. Divide

Every cell reproduces by cloning. Mating is an option that has to evolve; the ancestor cannot do it.

A cell is **ready** when it is awake, aged 10 or more, has no cooldown, holds at least 12 A and 12 B, and has an empty neighboring site. A ready cell picks one such site at random and then:

1. **Attach or release.** With a chance equal to its adhesion, the daughter will stay attached. Otherwise she will be released as a free cell.
2. **Attached daughter.** She is a clone. She bonds to her mother and to every neighbor of hers that is bonded to her mother. This is how a body grows.
3. **Released daughter.** She is normally a clone. With a chance equal to her mother's mating gene, the mother first looks for a second parent: another ready cell next to the same empty site, not bonded to her, with each of the two regarding the other as kin. If there is one, the daughter mixes the genes of both. If not, she is a clone after all.

Costs and inheritance:

- **Cost.** A cloning mother pays 10 A and 10 B. In a mating, each parent pays 5 A and 5 B.
- **Daughter.** She starts with 6 A and 6 B in store and 4 A and 4 B as body mass, so nothing is lost.
- **Cooldown.** Each parent waits 5 ticks before dividing again.
- **Mixing.** In a mating, each gene is copied from one parent or the other with equal chance. A clone copies all genes from her mother.
- **Mutation.** Each gene then has a chance to shift by a random amount of up to ±10% of its range (tag: ±0.05, wrapping around).

The mutation chance is 5% × (1 + 4 × p), where p is the parent's mutability × stress, averaged over both parents in a mating. Calm parents, or parents with mutability 0, give 5%. Fully stressed parents with mutability 1 give 25%.

A released daughter is the spore of this rule set: a free single cell that can wander before her own daughters start attaching. She carries her mother's adhesion gene, so she grows into the same kind of body. This is how anchored bodies spread to new ground.

## Environment

The world is sealed: matter only changes form, and the light and warmth that drive it rise and fall over a year of 2,000 ticks.

- **Season.** A value that swings smoothly from -1 at midwinter to +1 at midsummer and back, once per year. A run starts in spring, at 0 and rising.
- **Latitude.** A value that runs smoothly from +1 on row 0, the warm bright belt, to -1 on row 64, the cold dim belt.
- **Temperature** at a site = 15 + 10 × season + 5 × latitude, in °C. The range is 0 to 30 °C.
- **Light** at a site = 0.5 + 0.3 × season + 0.2 × latitude. The range is 0 to 1. There is no day and night; the year is the light cycle.
- **Sparks.** Each tick, 375 + 250 × season random sites are struck: 125 at midwinter, 625 at midsummer. At a struck site, up to 4 units of minerals turn into A or into B, with equal chance. This is food forming without life, and it is the only food until cells evolve photosynthesis.
- **Spoilage.** Each tick, 1% of the food lying on every site turns into minerals.
- **Disasters.** Every 5,000 ticks all cells in a 24 × 24 square at a seeded random position die. Their remains stay as food.

Matter therefore moves in a circle: minerals become food through sparks and photosynthesis, food becomes cells, and cells return it as minerals when they burn it and as food when they die.

## Starting conditions

Every run starts from 50 identical free cells, so all later diversity is evolved.

- Every site starts with 4 A, 4 B and 2 minerals. Together with what the cells carry, this is all the matter the world will ever hold.
- The 50 cells are placed at seeded random positions.
- Each starts with 15 A and 15 B, age 10, healthy, with no stress and no cooldown.

| Gene | Ancestor value |
| --- | --- |
| Tag | 0.5 |
| Tolerance | 0.1 |
| Harvest | 0.8 |
| Diet | 0.5 |
| Preferred temperature | 15 °C |
| Attack | 0 |
| Defense | 0 |
| Adhesion | 0 |
| Share | 0 |
| Role split | 0 |
| Motility | 0.5 |
| Appetite | 0.5 |
| Caution | 0 |
| Boldness | 0 |
| Sociability | 0 |
| Dormancy | 0 |
| Mutability | 0 |
| Mating | 0 |
| Photosynthesis | 0 |
| Resistance | 0 |

The ancestor is a free-swimming, single-celled generalist that clones itself and follows food. Photosynthesis, fighting, armor, virus resistance, bodies, sharing, roles, sex, dormancy and every other behavior start at zero and have to arise by mutation.

## Lineage record

Every birth and death is logged, which is enough to rebuild the full family tree and replay when each strategy first appeared.

- **Birth:** tick, ID, parent ID or IDs, site, full genome, and how it happened: attached clone, released clone or mating.
- **Death:** tick, ID, age, cause. The cause is disaster, drained (it was attacked in its final tick) or starved.
- **Census:** every 100 ticks, the season, the number of cells, the number of bodies by size, the number of infected cells, the share of producers, and the average of each gene.

## What keeps it in balance

No lineage can grow without limit, because every way of living carries a check that tightens as that lineage succeeds.

| Check | What it holds back | Why it tightens with success |
| --- | --- | --- |
| Finite matter | The total number of cells | The world holds a fixed amount of matter, so cells can only multiply until all of it is in use |
| Loss at every step | Predators | A predator keeps half of what it drains and pays for its attack, so prey can only ever support a much smaller number of predators |
| Satiation | Over-hunting | A full predator stops attacking |
| Starvation of hunters | Predators | When prey thin out, predators starve first and prey recover |
| Reach | Body size | Inner cells cannot reach food and must be fed by outer cells, so each added layer costs more than it earns |
| Anchoring | Bodies | A body cannot flee, so small predators and parasites can feed on its outer cells for as long as it lives |
| Viruses | Whatever is most common and most crowded | A virus needs matching neighbors, so it spreads best through dense clones and barely at all through rare or scattered cells |
| Cancer | Large, long-lived bodies | More cells and more divisions mean more chances for a cheating mutant |
| Aging | Every cell | Upkeep rises with age until the cell cannot pay |
| Winter and disasters | Everything | Light and sparks drop each winter, and a disaster clears a region every 5,000 ticks |
| Mutual dependence | Producers and consumers alike | Producers can only grow on the minerals that other cells release, and consumers can only grow on the food that producers make |
| Cost of resistance | Virus-proof lineages | Resistance costs upkeep every tick, so it only pays while viruses are common, and fades when they are rare |

The enemy of a large body is not a larger body. It is the small things: grazers on its surface, viruses running through its clones, and cancer from within.

## What can emerge

None of these strategies is named in the rules, but each is reachable from the ancestor by mutation. The table says how to recognize one if it appears.

| Strategy | Gene signature | How to recognize it |
| --- | --- | --- |
| Species | Tags form separate clusters | Gaps between tag clusters are wider than their tolerance, so they no longer mate |
| Predator | High attack, boldness and motility, low harvest | Most of its intake comes from draining |
| Armored grazer | High defense and harvest | Survives next to predators without fleeing |
| Runner | High caution and motility, low defense | Survives predators by staying away from them |
| Parasite | Attack only slightly above its host's defense, low harvest | Lives on a slow drain and on leak, without killing the host |
| Scavenger | Low attack, high appetite | Feeds mostly on scraps and corpses near predators |
| Partners (symbiosis) | Two lineages, diet near 1 and near 0 | They live side by side; each gets its second food mainly from the other's leak |
| Body | High adhesion, share above 0 | Bonded clones whose stores even out |
| Body with tissues | High share, role split far from 0 | Outer and inner cells of one body behave differently: one side fights, the other feeds |
| Cancer | Share near 0 inside a body | A mutant cell line that takes from the body and gives nothing |
| Sexual species | Mating above 0 | Released offspring mix the genes of two parents, which helps a lineage outrun its viruses |
| Migrant | High motility | A lineage whose position follows the warm belt through the year |
| Hibernator | Dormancy above 0 | Shuts down in winter and resumes in spring |
| Cold or heat specialist | Preferred temperature far from 15 °C | Lineages that settle in one belt or one season |
| Stress mutator | Mutability above 0 | Offspring vary more when the lineage is hunted or underfed |
| Producer | High photosynthesis, low harvest | Makes most of its food from light and minerals, thrives in the bright belt and in summer |
| Grazer | Moderate attack, boldness toward producers | Lives by draining producers, and returns their matter as minerals |
| Producer partnership | A producer and a consumer lineage side by side | The consumer lives on the producer's leak, and its waste minerals feed the producer |
| Resistant lineage | Resistance above 0 | Catches viruses less often and recovers, at a standing cost |

## What a balanced run looks like

A run counts as balanced when it meets all four of these targets, which give tuning something to aim at.

- **It lasts.** At least one lineage is alive after 50,000 ticks, which is 25 years.
- **It stays diverse.** At least three separate tag clusters are alive at the end. A tag cluster is a group of living cells whose tags are more than 0.1 away from every cell outside the group.
- **Both sides exist.** Producers and consumers are both present at the end. A producer is a cell whose photosynthesis gene is higher than its harvest gene; every other cell is a consumer.
- **It cycles without crashing.** From the fifth year on, each year's peak population is within a factor of 3 of the previous year's.

## Where it departs from reality, and open questions

The rules simplify real biology in four places, and eight questions can only be settled by running them.

**Known simplifications**

- **A fixed genome.** Evolution here can tune 20 traits but can never invent a 21st. Real genomes can.
- **Anchored bodies.** No multicellular body can move. This matches plants, fungi and sponges, not animals.
- **One copy of each gene.** There are no dominant or recessive variants.
- **Minimal viruses and immunity.** A virus is only a tag with no genome of its own, and a cell's whole immune system is one resistance gene.

**Open questions for tuning**

- [ ] Does the starting food let 50 cells establish without an explosion and crash?
- [ ] Is winter harsh enough to reward dormancy and migration without causing extinction?
- [ ] Can a large body feed itself from its outer cells alone?
- [ ] Does a drain factor of 3 let predators persist, or do they wipe out their prey and starve?
- [ ] Is a leak of 20% enough for partnerships to beat generalists?
- [ ] Do viruses hold back the most common lineage without wiping it out?
- [ ] Do producers appear at all, and once they do, do they crowd out every consumer?
- [ ] If every cell dies, should the run end, or should a few ancestor cells drift in again?

## The application

The finished product is one desktop window that anyone can download and run: the live world in the middle, controls on one side and statistics on the other.

| Area | Position | Contents |
| --- | --- | --- |
| World view | Center, largest area | The 128 × 128 grid drawn live, one square per site |
| Controls | Left side | Start and pause, speed, end world, restart, seed, view mode |
| Statistics | Right side | Live numbers and a population graph |

**Controls**

- **Start / Pause.** Runs or freezes the world.
- **Speed.** A slider for ticks per frame.
- **End world.** Stops the run and clears every cell.
- **Restart.** Begins a new world from the starting conditions, with the same seed or a new one.
- **Seed.** A field showing the current seed, so a run can be repeated or shared.
- **View mode.** Chooses what the colors show: lineage (tag), stored energy, feeding type (producer or consumer), infection, or the ground layer of food and minerals.

There is no rewind. The world only runs forward.

**Statistics**

- Tick, year and season, with the current temperature and light range.
- Number of cells, split into free cells and cells in bodies.
- Number of bodies and the size of the largest.
- Number of distinct lineages (tag clusters).
- Share of producers and consumers.
- Number of infected cells.
- Where the world's matter is: in cells, as food, as minerals.
- A graph of population over time.

Clicking a cell in the world view shows its genes, stores, age and stress in the statistics panel.

**Distribution**

- The project lives on GitHub, with an animated GIF of a run at the top of its description page.
- Ready-to-run downloads are attached to the project's Releases page, not stored among the source files.
- **Linux.** A single AppImage file.
- **Windows.** A zip holding two files: the program and the library with the C++ core. Godot keeps a C++ extension as a separate file, so a lone .exe would need a custom build of Godot itself.
- The downloads are unsigned, so Windows shows an unknown-publisher warning on first run. The description page says so.
- The source code and this rule set sit in the same repository.

## Building the engine

The recommended build is a simulation core written in C++ and shown through the Godot game engine, running as a live model: the world is computed and drawn as it happens, never recorded and played back.

| Option | Strength | Weakness |
| --- | --- | --- |
| C++ core inside Godot (recommended) | Fastest; no limits on data structures, so bonds, viruses and the lineage record are easy to write; Godot supplies the window, drawing, buttons and sliders | Needs a C++ build setup and a small wrapper to connect the core to Godot; a browser version needs extra build work |
| C++ with a small graphics library | Fastest, with the least setup | Every control and menu has to be written by hand |
| Python with JAX | Whole-grid arithmetic compiled to fast CPU or GPU code; rules stay quick to change | Arrays must keep a fixed shape, so bookkeeping such as the lineage record is awkward |
| Python with NumPy | Simplest to write and debug | Slowest; may be too slow for a smooth live view |

The build has two parts that should stay separate: a core that knows nothing about Godot, and a thin Godot wrapper around it.

- **Core.** A plain C++ library with no Godot code in it. It holds the state and offers one function that advances the world by one tick. It can be run and tested with no display at all.
- **State as arrays.** One array of 16,384 entries per cell property and per gene, plus arrays for food A, food B and minerals. Bonds are 8 flags per cell, one for each direction.
- **Two copies of the state.** Each phase reads from the current copy and writes to the next, then the two swap. This is what makes all cells act at once.
- **One random source.** The core owns a single seeded generator and draws from it in a fixed order on a single thread, so the same seed gives the same history.
- **Godot wrapper.** A thin extension class exposes the tick function and the grid to Godot. A Godot scene calls the tick function several times per frame and draws the grid as an image. The speed control is the number of ticks per frame.
- **Lineage record.** The core appends births and deaths to a file in batches as they happen.
