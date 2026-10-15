# Banjo & Kong: Jiggy Jungle, the whole game

DK64's own game stays the backbone: its worlds, Kongs, colored bananas, B. Lockers and
K. Rool are all still there, and still completable. Each world is fused with a
Banjo-Kazooie world: that world's characters, enemies, quests and collectibles move in,
placed in DK64's map on ground the player can reach, with Banjo-Kazooie's models,
animations, sounds and text from the player's own ROM.

## Worlds

| DK64 world | Fused with | Banjo-Kazooie characters and quests | Reward ids |
|---|---|---|---|
| Training Grounds | Spiral Mountain | Mumbo (four Mumbo Tokens for a Jiggy) | 0x90-0x9F |
| DK Isles (hub) | Gruntilda's Lair, Spiral Mountain | Bottles' hints, Brentilda, Cheato, Gruntilda's taunts | 0x00-0x0F |
| Jungle Japes | Mumbo's Mountain | Bottles, Conga, Chimpy, Grublins, Jinjos | 0x10-0x1F |
| Angry Aztec | Gobi's Valley | Gobi, Jinxy, Trunker, Grabba, Ancient Ones | 0x20-0x2F |
| Frantic Factory | Rusty Bucket Bay | Boss Boom Box (0x31), Snorkel and the whistles (0x32), Flotsams; Jinjo Jiggy 0x30 | 0x30-0x3F |
| Gloomy Galleon | Treasure Trove Cove, Clanker's Cavern | Captain Blubber, Nipper, Clanker, Snippets | 0x40-0x4F |
| Fungi Forest | Click Clock Wood | Eyrie, Gnawty, Nabnut, the seasons | 0x50-0x5F |
| Crystal Caves | Freezeezy Peak | Boggy, Wozza, the Twinklies, Sir Slush | 0x60-0x6F |
| Creepy Castle | Mad Monster Mansion | Motzand, Tumblar, Napper, Loggo | 0x70-0x7F |
| Hideout Helm | Gruntilda's Lair finale | Grunty's Furnace Fun quiz, the tag-team final boss | 0x80-0x8F |

Every fused world has, alongside DK64's own content:

- Music Notes (a trail or two of them), five Jinjos (all five give a Jiggy), two empty
  Honeycomb pieces, Mumbo Tokens and Red and Gold Feathers.
- Two or more Banjo-Kazooie characters, each with a quest, a talk in a text box and a
  short cutscene, giving a Jiggy (a Golden Banana drawn as a Jiggy, counted by DK64 as
  usual).
- Banjo-Kazooie enemies patrolling where DK64's leave room.

## How the pieces fit

- Rewards: `host/patches/jiggy_rewards.c` gives any number of Jiggies without a DK64 flag
  each. Reward ids are in the table above; a world uses its own range.
- Collectibles: per-map tables in `jiggy_notes.c`, `jiggy_jinjos.c`, `jiggy_honeycombs.c`,
  `jiggy_feathers.c` and `jiggy_mumbo_tokens.c`; the Jinjo Jiggy is `jiggy_jinjo_jiggy.c`.
- Characters: one file each, following `jiggy_conga.c`, `jiggy_chimpy.c`, `jiggy_bottles.c`
  and `jiggy_mumbo.c` (model, animations, sounds, text box, cutscene lock, save key).
- Save keys in `jiggy_save.bin` (top byte, ORed with the map): 0x41-0x4E are taken (see the
  files), 0x4B is rewards, and each fused world has its own for its characters: Aztec 0x51,
  Factory 0x52, Galleon 0x53, Fungi 0x54, Caves 0x55, Castle 0x56, Isles 0x57, Helm 0x58,
  Training Grounds 0x59 (character index in bits 16-23).
- Placement: `tools/dk64-setup/setup_dump.py` lists where DK64 put its own bananas, coins
  and actors in a map, which is ground the player can reach.

## Still to do after the worlds

- Two-player split-screen co-op (roadmap milestone 6).
- Mumbo's transformations, the hub rework and the final boss against Gruntilda and K. Rool.
- Android: the game running on a phone, not just the launcher.
