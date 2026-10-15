// The story of Banjo & Kong: Jiggy Jungle, told in short scenes of text boxes at the moments
// DK64 already marks: a new game, the first arrival on DK Isles, each Kong's "help me" at their
// cage and their rescue, each world's Troff 'n' Scoff room before the boss door, and the first
// visits to Fungi Forest, Creepy Castle and Hideout Helm.
//
// Each scene is a few lines, each line one speaker's page in jiggy_talk.c's box (their Banjo-
// Kazooie portrait beside DK64's dialogue font, with their talking sounds), played one after
// another while the box's cutscene lock (bit 0x10 of D_global_asm_8076A0B1) holds the player;
// A turns the pages. The voices follow the concept's story page: Gruntilda always in rhyming
// couplets, Kazooie sarcastic, Banjo friendly and slow, Bottles polite and nervous, Klungo in
// the third person with his doubled s's, K. Rool grand.
//
// Portraits are all Banjo-Kazooie's (ASSET_*_SPRITE_*): Banjo 0x7EF, Kazooie 0x7F4, Bottles
// 0x7F0, Gruntilda 0x816 (and her beautiful self, 0x841, dreaming of her machine), Brentilda
// 0x82E and Klungo 0x82B, who is in Banjo-Kazooie's ROM though not its game. Those without one
// borrow the nearest: Mr. Vile (0x7E6, a red crocodile) for the Kremlings K. Rool and
// K. Lumsel, Chimpy (0x7F5) for Diddy, Tiny and Lanky, Conga (0x7F2) for Chunky.
//
// A scene waits for the player to be free: no DK64 cutscene (is_cutscene_active), no one else
// holding the cutscene lock, no other box (jiggy_talk_open) and no Banjo cameo (jiggy_cameo.c),
// for a moment in a row; then the first scene that's due plays. Each plays once per save file
// (key STORY_SAVE_SHOWN, bit per scene). The Kong scenes follow DK64's own flags turning on
// (PERMFLAG_CUTSCENE_*_HELP_ME at the cage, PERMFLAG_KONG_* when freed): a flag only counts
// once it has been seen off on this file (key STORY_SAVE_ARMED), so an old save that already
// freed a Kong doesn't get the scene out of the blue. Never claims a DK64 flag.
//
// Updated with the player's draw (func_global_asm_80630DCC in patches_draw.c). Without a
// Banjo-Kazooie ROM nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_talk.h"

#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save keys (docs/GAME_PLAN.md): the story's prefix.
#define STORY_SAVE_SHOWN 0x5A000000u // Bit per scene: it has played.
#define STORY_SAVE_ARMED 0x5A000001u // Bit per Kong flag: seen off on this file.

// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10
// Game frames the player has to be free before a scene starts.
#define STORY_FREE_FRAMES 30

// Maps (Maps enum).
#define MAP_ID_HELM 0x11
#define MAP_ID_ISLES 0x22
#define MAP_ID_TROFF_N_SCOFF 0x2A
#define MAP_ID_FUNGI 0x30
#define MAP_ID_CASTLE 0x57
#define MAP_ID_HELM_LOBBY 0xAA
#define MAP_ID_DK_HOUSE 0xAB
#define MAP_ID_TRAINING_GROUNDS 0xB0

// DK64's permanent flags (permflag_e), only read.
#define FLAG_KONG_DIDDY 0x6
#define FLAG_KONG_TINY 0x42
#define FLAG_KONG_LANKY 0x46
#define FLAG_KONG_CHUNKY 0x75
#define FLAG_DIDDY_HELP_ME 0x2A
#define FLAG_LANKY_HELP_ME 0x5D
#define FLAG_TINY_HELP_ME 0x5E
#define FLAG_CHUNKY_HELP_ME 0x8C
#define FLAG_TYPE_PERMANENT 0
// The worlds' Boss Keys (PERMFLAG_ITEM_KEY_1 to 8): a world's boss scene is skipped once its
// key is in hand.
static const s16 story_key_flags[8] = {0x1A, 0x4A, 0x8A, 0xA8, 0xEC, 0x124, 0x13D, 0x17C};

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u32 object_timer;
extern u8 current_file;
extern u8 is_cutscene_active;
extern u8 D_global_asm_8076A0B1;
u8 isFlagSet(s16 flagIndex, u8 flagType);
u8 getLevelIndex(u8 map, u8 arg1);

// The speakers: jiggy_talk.c's talker, and a sound to start each of their lines with.
typedef struct {
    JiggyTalker talker;
    u16 start_sfx; // Played as the line opens, or 0.
    f32 start_volume;
    f32 start_pitch;
} StorySpeaker;

static const u16 sfx_grunty[] = {0xEC, 0xED, 0xEE};   // SFX_EC/ED/EE_GRUNTY_TALKING
static const u16 sfx_bottles[] = {0xB4, 0xBC, 0xBD};  // Bottles' (jiggy_bottles.c)
static const u16 sfx_kazooie[] = {0x49};              // SFX_49_KAZOOIE_RA, high and quick
static const u16 sfx_brentilda[] = {0x14C};           // SFX_14C_BRENTILDA_TALKING
static const u16 sfx_chimpy[] = {0x58, 0x59, 0x5A};   // SFX_58/59/5A_CHIMPY_NOISE
static const u16 sfx_conga[] = {0xE3, 0xE4, 0xE5};    // Conga's (jiggy_conga.c)
static const u16 sfx_kremling[] = {0x6E};             // SFX_6E_VILE_EGH

static const StorySpeaker spk_banjo = {
    {0x7EF, 6, "BANJO", NULL, 0, 0.0f, 1.0f}, 0x3EA, 24000.0f / 32767.0f, 1.0f, // SFX_3EA_BANJO_GUH_HUH
};
static const StorySpeaker spk_kazooie = {
    {0x7F4, 6, "KAZOOIE", sfx_kazooie, 1, 14000.0f / 32767.0f, 1.3f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_bottles = {
    {0x7F0, 6, "BOTTLES", sfx_bottles, 3, 20000.0f / 32767.0f, 1.0f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_grunty = {
    {0x816, 6, "GRUNTILDA", sfx_grunty, 3, 22000.0f / 32767.0f, 1.0f}, 0xEA, 26000.0f / 32767.0f, 1.0f, // SFX_EA_GRUNTY_LAUGH_1
};
static const StorySpeaker spk_grunty_dreaming = {
    {0x841, 6, "GRUNTILDA", sfx_grunty, 3, 22000.0f / 32767.0f, 1.2f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_brentilda = {
    {0x82E, 6, "BRENTILDA", sfx_brentilda, 1, 20000.0f / 32767.0f, 1.0f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_klungo = {
    {0x82B, 6, "KLUNGO", sfx_kremling, 1, 12000.0f / 32767.0f, 1.6f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_krool = {
    {0x7E6, 6, "K. ROOL", sfx_kremling, 1, 18000.0f / 32767.0f, 0.8f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_klumsel = {
    {0x7E6, 6, "K. LUMSEL", sfx_kremling, 1, 18000.0f / 32767.0f, 0.65f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_diddy = {
    {0x7F5, 6, "DIDDY", sfx_chimpy, 3, 20000.0f / 32767.0f, 1.2f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_tiny = {
    {0x7F5, 6, "TINY", sfx_chimpy, 3, 20000.0f / 32767.0f, 1.5f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_lanky = {
    {0x7F5, 6, "LANKY", sfx_chimpy, 3, 20000.0f / 32767.0f, 0.9f}, 0, 0.0f, 0.0f,
};
static const StorySpeaker spk_chunky = {
    {0x7F2, 6, "CHUNKY", sfx_conga, 3, 26000.0f / 32767.0f, 0.8f}, 0, 0.0f, 0.0f,
};

typedef struct {
    const StorySpeaker *who;
    const char *text; // Uppercase, in the font's letters (no quotes).
} StoryLine;

#define LINES(a) a, ARRAY_COUNT_OF(a)

// The Dig: a new game.
static const StoryLine scene_dig[] = {
    {&spk_bottles, "OH DEAR, OH DEAR. I DON'T MEAN TO INTRUDE, BANJO, BUT I SAW THE WHOLE THING FROM MY MOLEHILL LAST NIGHT."},
    {&spk_bottles, "KREMLINGS WITH SHOVELS! THEY LIFTED GRUNTILDA'S BOULDER AND CARRIED HER OFF ON THAT ISLAND OF THEIRS."},
    {&spk_kazooie, "AND YOU WOKE US UP FOR THAT? I WAS DREAMING OF A WORLD WITH NO MOLES IN IT."},
    {&spk_banjo, "HUH? SO WE HID IN A CRATE... AND NOW WE'RE ON A BOAT? THAT EXPLAINS THE WOBBLY TUMMY."},
    {&spk_kazooie, "IT'S AN ISLAND WITH A FACE, BANJO. A FLOATING CROCODILE ISLAND. KEEP UP."},
    {&spk_grunty, "THE APES' GOLD BANANAS, PILED UP HIGH, I'LL TURN TO JIGGIES, BY AND BY!"},
    {&spk_grunty, "ROOL'S BLAST-O-MATIC GETS THEIR POWER, AND BLOWS THESE ISLES UP IN AN HOUR!"},
    {&spk_bottles, "THERE YOU HAVE IT. EVERY JIGGY YOU BRING BACK WEAKENS HER SPELL. I'LL BE ABOUT, SHOULD YOU NEED A LESSON."},
    {&spk_kazooie, "GREAT. MORE JIGSAW PIECES. COME ON, BANJO, BEFORE THE MOLE STARTS ANOTHER LECTURE."},
};

// Krem Lair Rises: the first arrival on DK Isles.
static const StoryLine scene_krem_lair[] = {
    {&spk_grunty, "I CARVED MY FACE INTO THIS ROCK, SO ALL THE ISLES CAN STARE IN SHOCK!"},
    {&spk_grunty, "YOUR MONKEY CHUMS WERE SNATCHED FROM BED, AND CAGED AWAY TILL THEY'RE NEARLY DEAD!"},
    {&spk_krool, "WITCH! LESS RHYMING, MORE CHARGING. MY BLAST-O-MATIC WILL NOT POWER ITSELF."},
    {&spk_grunty, "HUSH NOW, ROOL, YOU SCALY LUMP, MY SPELL WILL GIVE YOUR GUN ITS THUMP!"},
    {&spk_banjo, "GUH-HUH... DK, YOU WERE OUT GETTING BANANAS? LUCKY! WE'LL HELP YOU GET YOUR PALS BACK."},
    {&spk_kazooie, "FIND CAGES, BREAK CAGES, GRAB JIGGIES. EVEN THE APE CAN MANAGE THAT."},
};

// The Kongs' cages: Gruntilda's warning when DK64 shows each one asking for help.
static const StoryLine scene_cage_diddy[] = {
    {&spk_grunty, "MONKEYS LOCKED, THE KEY'S BEEN TOSSED, YOUR FUZZY HERO'S WELL AND LOST!"},
};
static const StoryLine scene_cage_tiny[] = {
    {&spk_grunty, "THIS LITTLE MONKEY IN HER CAGE WILL NEVER LEAVE, NOT FOR AN AGE!"},
};
static const StoryLine scene_cage_lanky[] = {
    {&spk_grunty, "THAT LANKY APE WITH ARMS SO LONG CAN'T REACH THE KEY. HE'S STUCK, POOR PONG!"},
};
static const StoryLine scene_cage_chunky[] = {
    {&spk_grunty, "THE BIGGEST APE SHAKES IN HIS SHOES. HIS CAGE STAYS SHUT, YOU'RE BOUND TO LOSE!"},
};

// The Kongs freed.
static const StoryLine scene_free_diddy[] = {
    {&spk_diddy, "THANKS, DK! WAIT... IS THAT A BEAR WITH A BIRD IN HIS BACKPACK?"},
    {&spk_banjo, "GUH-HUH! I'M BANJO, AND THIS IS KAZOOIE. WE'RE HERE TO HELP!"},
    {&spk_diddy, "COOL! WHEN WE BUST TINY OUT, I'LL THROW HER RIGHT OVER THE BIG GAPS. A TEAM-UP THROW!"},
    {&spk_kazooie, "A MONKEY THROWING A MONKEY. WOW. WE JUST FLY OVER THINGS, BUT SURE, THAT'S ADORABLE."},
};
static const StoryLine scene_free_tiny[] = {
    {&spk_tiny, "FINALLY! THAT CAGE WAS RUINING MY PIGTAILS."},
    {&spk_diddy, "TINY! TEAM-UP THROW, LIKE I PROMISED? I CHUCK, YOU TWIRL!"},
    {&spk_kazooie, "TWIRLING. THAT'S FLYING FOR PEOPLE WHO FORGOT TO GROW FEATHERS."},
    {&spk_banjo, "AW, BE NICE, KAZOOIE. WELCOME TO THE TEAM, TINY!"},
};
static const StoryLine scene_free_lanky[] = {
    {&spk_lanky, "OOH, A BEAR! DO YOU KNOW ANY TUNES? I KNOW ALL OF THEM. BADLY."},
    {&spk_banjo, "WELL, I DO PLAY THE BANJO... GUH-HUH, THAT'S WHERE I GOT THE NAME."},
    {&spk_kazooie, "A NOODLE-ARMED APE AND A BEAR WHO'S PROUD OF HIS OWN NAME. WE'RE DOOMED."},
};
static const StoryLine scene_free_chunky[] = {
    {&spk_chunky, "IS... IS THE WITCH GONE? CHUNKY DOESN'T LIKE WITCHES. OR CAGES. OR KREMLINGS."},
    {&spk_banjo, "IT'S OKAY, CHUNKY. WE'LL ALL STICK TOGETHER."},
    {&spk_kazooie, "THE BIGGEST APE ON THE ISLAND IS SCARED OF A GRANNY ON A BROOM? FANTASTIC."},
};

// Klungo over the speakers in each world's Troff 'n' Scoff room, before the boss.
static const StoryLine scene_boss_japes[] = {
    {&spk_klungo, "ATTENTION, PLEASSE. KLUNGO WORKSS FOR KING K. ROOL NOW. KLUNGO IS SSORRY TO ANNOUNCE ARMY DILLO."},
    {&spk_klungo, "KLUNGO ASSKED HIM TO BE GENTLE. HE SSAID NO, VERY LOUDLY. KLUNGO APOLOGISSES IN ADVANCE."},
};
static const StoryLine scene_boss_aztec[] = {
    {&spk_klungo, "KLUNGO AGAIN. BEHIND THISS DOOR IS DOGADON. HE BREATHESS FIRE AND DOESS NOT LISSTEN TO KLUNGO."},
    {&spk_klungo, "PLEASSE DO NOT BLAME KLUNGO FOR ANY SSINGED FUR. OR FEATHERSS."},
};
static const StoryLine scene_boss_factory[] = {
    {&spk_klungo, "MAD JACK IS IN HISS BOXX. KLUNGO WOUND HIM UP, AND NOW KLUNGO REGRETSS IT."},
    {&spk_klungo, "IF HE ZAPSS YOU, KLUNGO IS SSORRY. IF HE ZAPSS KLUNGO, KLUNGO IS MORE SSORRY."},
};
static const StoryLine scene_boss_galleon[] = {
    {&spk_klungo, "NEXXT IS PUFTOSSS, THE BIG PUFFER FISSH. KLUNGO CANNOT SSWIM, SSO KLUNGO WILL WATCH FROM HERE."},
    {&spk_klungo, "KLUNGO HASS PACKED A TOWEL FOR YOU. IT IS A SSMALL TOWEL. SSORRY."},
};
static const StoryLine scene_boss_fungi[] = {
    {&spk_klungo, "DOGADON IS BACK, AND ANGRIER. KLUNGO TOLD HIM HE WAS ANGRY ENOUGH ALREADY. HE DISSAGREED."},
    {&spk_klungo, "KLUNGO DOESS NOT LIKE WORKING FOR TWO BOSSSES. KLUNGO DOESS NOT LIKE DRAGONSS EITHER."},
};
static const StoryLine scene_boss_caves[] = {
    {&spk_klungo, "ARMY DILLO HASS NEW ROCKETSS. KLUNGO DID NOT ORDER THEM. KLUNGO IS VERY SSORRY ABOUT THE ROCKETSS."},
};
static const StoryLine scene_boss_castle[] = {
    {&spk_klungo, "KING KUT OUT IS MADE OF CARDBOARD. KLUNGO MADE HIM. PLEASSE BE GENTLE WITH KLUNGO'S ARTSS AND CRAFTSS."},
    {&spk_klungo, "...HE WILL NOT BE GENTLE WITH YOU, THOUGH. KLUNGO APOLOGISSES."},
};

// The Overheard Plan: the first visit to Fungi Forest.
static const StoryLine scene_overheard[] = {
    {&spk_brentilda, "PSST! MY SISTER HAS BUILT A MACHINE TO MAKE HERSELF YOUNG AND BEAUTIFUL. SHE'LL DRAIN K. ROOL'S WEAPON TO RUN IT!"},
    {&spk_grunty_dreaming, "ONCE ROOL'S BIG GUN HAS DONE ITS DEED, ITS LEFTOVER SPARK IS ALL I NEED!"},
    {&spk_grunty_dreaming, "MY BEAUTY MACHINE WILL MAKE ME FAIR, WITH SILKY SKIN AND GOLDEN HAIR!"},
    {&spk_krool, "DRAIN MY BLAST-O-MATIC? FOR A FACE CREAM? KLUNGO! FETCH ME THAT WITCH!"},
    {&spk_klungo, "KLUNGO DOESS NOT LIKE WORKING FOR TWO BOSSSES."},
    {&spk_kazooie, "OOH, TROUBLE IN VILLAIN PARADISE. SOMEBODY GET ME A SNACK."},
};

// The Split: the first visit to Creepy Castle.
static const StoryLine scene_split[] = {
    {&spk_krool, "WITCH, I HIRED YOU FOR SPELLS. NOT POEMS. AND CERTAINLY NOT TO STEAL MY WEAPON!"},
    {&spk_grunty, "YOU'D AIM YOUR GUN AT MY OWN TOWER? THEN I'LL TAKE BACK ITS STOLEN POWER!"},
    {&spk_grunty, "I'LL MOVE IT ONTO MY RUSTY BUCKET, AND IF YOU DON'T LIKE IT, ROOL, JUST LUMP IT!"},
    {&spk_klungo, "KLUNGO WILL... PACK THE BOXESS. AGAIN."},
    {&spk_banjo, "HUH? THEY'RE FIGHTING EACH OTHER NOW?"},
    {&spk_kazooie, "YES, BANJO. MORE BICKERING FOR THEM MEANS LESS BLASTING FOR US. KEEP UP."},
};

// Chains Break: the first visit to Hideout Helm (its lobby or the Helm itself).
static const StoryLine scene_chains[] = {
    {&spk_klumsel, "THANK YOU, THANK YOU! EVERY KEY YOU BROUGHT BROKE ONE OF MY CHAINS. K. LUMSEL IS FREE!"},
    {&spk_klumsel, "K. LUMSEL SMASHED THE WAY OPEN. THE BLAST-O-MATIC IS ON THE RUSTY BUCKET. GO STOP IT, FRIENDS!"},
    {&spk_grunty, "SO LUMSEL'S LOOSE AND THE WAY IS CLEAR? THEN COME AND FACE ME, BEAR, UP HERE!"},
    {&spk_kazooie, "FINALLY. LET'S GO KNOCK A WITCH OFF HER BROOM."},
};

// The scenes, in the order they're looked at (bit i of STORY_SAVE_SHOWN is scene i).
enum {
    SCENE_DIG,
    SCENE_KREM_LAIR,
    SCENE_CAGE_DIDDY,
    SCENE_CAGE_TINY,
    SCENE_CAGE_LANKY,
    SCENE_CAGE_CHUNKY,
    SCENE_FREE_DIDDY,
    SCENE_FREE_TINY,
    SCENE_FREE_LANKY,
    SCENE_FREE_CHUNKY,
    SCENE_BOSS_JAPES, // Then one a world, in DK64's level order.
    SCENE_BOSS_AZTEC,
    SCENE_BOSS_FACTORY,
    SCENE_BOSS_GALLEON,
    SCENE_BOSS_FUNGI,
    SCENE_BOSS_CAVES,
    SCENE_BOSS_CASTLE,
    SCENE_OVERHEARD,
    SCENE_SPLIT,
    SCENE_CHAINS,
    SCENE_COUNT
};

static const struct {
    const StoryLine *lines;
    u32 count;
} story_scenes[SCENE_COUNT] = {
    {LINES(scene_dig)},          {LINES(scene_krem_lair)},
    {LINES(scene_cage_diddy)},   {LINES(scene_cage_tiny)},    {LINES(scene_cage_lanky)},  {LINES(scene_cage_chunky)},
    {LINES(scene_free_diddy)},   {LINES(scene_free_tiny)},    {LINES(scene_free_lanky)},  {LINES(scene_free_chunky)},
    {LINES(scene_boss_japes)},   {LINES(scene_boss_aztec)},   {LINES(scene_boss_factory)}, {LINES(scene_boss_galleon)},
    {LINES(scene_boss_fungi)},   {LINES(scene_boss_caves)},   {LINES(scene_boss_castle)},
    {LINES(scene_overheard)},    {LINES(scene_split)},        {LINES(scene_chains)},
};

// The Kongs, in the order of the cage and rescue scenes: their help-me and freed flags. Bits
// 0-3 of STORY_SAVE_ARMED are the help-me flags, 4-7 the freed ones.
static const struct {
    s16 help_flag;
    s16 kong_flag;
} story_kongs[4] = {
    {FLAG_DIDDY_HELP_ME, FLAG_KONG_DIDDY},
    {FLAG_TINY_HELP_ME, FLAG_KONG_TINY},
    {FLAG_LANKY_HELP_ME, FLAG_KONG_LANKY},
    {FLAG_CHUNKY_HELP_ME, FLAG_KONG_CHUNKY},
};

static u32 story_shown;
static u32 story_armed;
static s32 story_file = -1;
static s32 story_scene = -1; // The scene playing, or -1.
static u32 story_line;
static s32 story_map = -1;
static u32 story_last_timer;
static u32 story_free; // Game frames the player has been free in a row.
static s32 story_world = -1; // The last world (level 0-6) the player was in.

static s32 story_flag(s16 flag) {
    return isFlagSet(flag, FLAG_TYPE_PERMANENT) != 0;
}

static s32 story_has_shown(s32 scene) {
    return (story_shown & (1u << scene)) != 0;
}

static void story_mark_shown(s32 scene) {
    story_shown |= 1u << scene;
    jiggy_save_set(current_file, STORY_SAVE_SHOWN, story_shown);
}

// Remembers which Kong flags this file has seen off, so their turning on can be told apart
// from a save that had them already.
static void story_arm_kongs(void) {
    u32 armed = story_armed;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (!story_flag(story_kongs[i].help_flag)) {
            armed |= 1u << i;
        }
        if (!story_flag(story_kongs[i].kong_flag)) {
            armed |= 1u << (4 + i);
        }
    }
    if (armed != story_armed) {
        story_armed = armed;
        jiggy_save_set(current_file, STORY_SAVE_ARMED, story_armed);
    }
}

// The world (level 0-6, Japes to Castle) the Troff 'n' Scoff room belongs to.
static s32 story_boss_world(void) {
    s32 level = getLevelIndex((u8)current_map, 0);

    if (level >= 0 && level <= 6) {
        return level;
    }
    return story_world;
}

// Whether scene `scene` is due now.
static s32 story_due(s32 scene) {
    s32 map = current_map;
    s32 kong;

    switch (scene) {
        case SCENE_DIG:
            // A new game: before Diddy is freed, on the maps a file starts on.
            return (map == MAP_ID_TRAINING_GROUNDS || map == MAP_ID_DK_HOUSE || map == MAP_ID_ISLES) &&
                   !story_flag(FLAG_KONG_DIDDY);
        case SCENE_KREM_LAIR:
            return map == MAP_ID_ISLES && story_has_shown(SCENE_DIG) && !story_flag(FLAG_KONG_TINY) &&
                   !story_flag(FLAG_KONG_LANKY);
        case SCENE_CAGE_DIDDY:
        case SCENE_CAGE_TINY:
        case SCENE_CAGE_LANKY:
        case SCENE_CAGE_CHUNKY:
            kong = scene - SCENE_CAGE_DIDDY;
            return (story_armed & (1u << kong)) && story_flag(story_kongs[kong].help_flag) &&
                   !story_flag(story_kongs[kong].kong_flag);
        case SCENE_FREE_DIDDY:
        case SCENE_FREE_TINY:
        case SCENE_FREE_LANKY:
        case SCENE_FREE_CHUNKY:
            kong = scene - SCENE_FREE_DIDDY;
            return (story_armed & (1u << (4 + kong))) && story_flag(story_kongs[kong].kong_flag);
        case SCENE_OVERHEARD:
            return map == MAP_ID_FUNGI && !story_flag(story_key_flags[4]);
        case SCENE_SPLIT:
            return map == MAP_ID_CASTLE && !story_flag(story_key_flags[6]);
        case SCENE_CHAINS:
            return (map == MAP_ID_HELM || map == MAP_ID_HELM_LOBBY) && !story_flag(story_key_flags[7]);
        default:
            if (scene >= SCENE_BOSS_JAPES && scene <= SCENE_BOSS_CASTLE) {
                s32 world = scene - SCENE_BOSS_JAPES;

                return map == MAP_ID_TROFF_N_SCOFF && story_boss_world() == world &&
                       !story_flag(story_key_flags[world]);
            }
            return FALSE;
    }
}

// Opens line story_line of the scene playing; FALSE if it couldn't (its portrait didn't load).
static s32 story_say_line(void) {
    const StoryLine *line = &story_scenes[story_scene].lines[story_line];
    const StorySpeaker *who = line->who;

    if (!jiggy_talk_say(JIGGY_TALK_OWNER_STORY, &who->talker, &line->text, 1, JIGGY_TALK_LOCK)) {
        return FALSE;
    }
    if (who->start_sfx != 0) {
        jiggy_bk_sfx_play(who->start_sfx, who->start_volume, who->start_pitch);
    }
    return TRUE;
}

// Moves on to the next line that opens, or ends the scene.
static void story_next_line(void) {
    while (++story_line < story_scenes[story_scene].count) {
        if (story_say_line()) {
            return;
        }
    }
    story_scene = -1;
    story_free = 0;
}

static void story_start(s32 scene) {
    // Marked as soon as it starts, so a scene cut short (the map changing) doesn't come back.
    story_mark_shown(scene);
    story_scene = scene;
    story_line = 0;
    if (!story_say_line()) {
        story_next_line();
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c): plays the scene
// that's due, once a frame.
Gfx *jiggy_story_draw(Gfx *dl, Actor *actor) {
    s32 level;
    s32 scene;

    if (actor != gPlayerPointer || cc_number_of_players != 1 || !jiggy_bk_available()) {
        return dl;
    }
    // Once a game frame, and not while paused.
    if (object_timer == story_last_timer) {
        return dl;
    }
    story_last_timer = object_timer;
    if ((s32)current_file != story_file) {
        story_file = current_file;
        story_shown = jiggy_save_get(current_file, STORY_SAVE_SHOWN);
        story_armed = jiggy_save_get(current_file, STORY_SAVE_ARMED);
        story_scene = -1;
    }
    if ((s32)current_map != story_map) {
        // jiggy_talk.c closes the box on leaving the map.
        story_map = current_map;
        story_scene = -1;
        story_free = 0;
        level = current_map != MAP_ID_TROFF_N_SCOFF ? getLevelIndex((u8)current_map, 0) : -1;
        if (level >= 0 && level <= 6) {
            story_world = level;
        }
    }
    jiggy_talk_update();
    if (story_scene >= 0) {
        if (!jiggy_talk_open(JIGGY_TALK_OWNER_STORY)) {
            story_next_line();
        }
        return dl;
    }
    // Not over someone else's talk, a DK64 cutscene or Banjo's cameo; queued until they've gone.
    if (is_cutscene_active == 1 || (D_global_asm_8076A0B1 & CUTSCENE_LOCK) || jiggy_talk_open(-1) ||
        jiggy_cameo_showing()) {
        story_free = 0;
        return dl;
    }
    story_arm_kongs();
    if (++story_free < STORY_FREE_FRAMES) {
        return dl;
    }
    for (scene = 0; scene < SCENE_COUNT; scene++) {
        if (story_has_shown(scene) || !story_due(scene)) {
            continue;
        }
        story_start(scene);
        break;
    }
    // Cage warnings that came too late (the Kong's out) are dropped.
    for (scene = SCENE_CAGE_DIDDY; scene <= SCENE_CAGE_CHUNKY; scene++) {
        if (!story_has_shown(scene) && story_flag(story_kongs[scene - SCENE_CAGE_DIDDY].kong_flag)) {
            story_mark_shown(scene);
        }
    }
    return dl;
}
