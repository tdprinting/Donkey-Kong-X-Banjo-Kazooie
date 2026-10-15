// Music Notes: Banjo-Kazooie's own collectible note (ASSET_6D6_SPRITE_MUSIC_NOTE, the sprite
// src/core2/ch/musicnote.c and the levels' sprite props use), read from the player's
// Banjo-Kazooie ROM, as a collectible of its own beside DK64's colored bananas.
//
// The sprite has 10 frames of a half turn. Like Banjo-Kazooie (cube prop animation in
// src/core2/code_A5BC0.c, which the sprite's header selects), the note plays them forward
// and then back mirrored, two ticks a frame, for a full spin, and always faces the camera.
//
// The notes exist as trails in the Training Grounds, Jungle Japes and Gloomy Galleon
// (note_trails below). They are drawn right after the player's own model
// (func_global_asm_80630DCC in patches_draw.c).
//
// Touching a note collects it: it disappears, a pickup sound plays, and Banjo-Kazooie's note
// counter (its spinning HUD note, ASSET_7D9_SPRITE_NOTE, and its bold HUD numbers) shows the
// total for a moment in the HUD pass (jiggy_notes_hud_draw, called from patches_ui.c).
// Collected notes and the total are kept with the DK64 save file (jiggy_save_get/set).
// TODO: the collect sparkle and Banjo-Kazooie's own note sound.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define JIGGY_MUSIC_NOTE 0x6D6
#define NOTE_MAX_FRAMES 10
#define NOTE_MAX_TEXELS (64 * 64)
// RGBA16 texels that fit in TMEM (4 KiB) at once; bigger frames are drawn in strips.
#define TMEM_RGBA16_TEXELS 2048
#define NOTE_MAX_STRIPS (NOTE_MAX_TEXELS / TMEM_RGBA16_TEXELS)
// Ticks (object_timer, 30 Hz) per frame, from the sprite's header.
#define NOTE_TICKS_PER_FRAME 2
// How big a note is in the world: the sprite's size in Banjo-Kazooie units (its header,
// 210 for the 64-texel frame), the note actor's scale (musicNote_update), and Banjo-Kazooie
// units to DK64 units (as jiggy_banjo3d.c scales Banjo).
#define NOTE_BK_SIZE 210.0f
#define NOTE_BK_SCALE 0.42857143f
#define BK_TO_DK64 0.25f
#define NOTE_SIZE (NOTE_BK_SIZE * NOTE_BK_SCALE * BK_TO_DK64)
// Height of a note's center above the ground it was placed on.
#define NOTE_HOVER 16.0f
#define NOTE_AIRBORNE_HEIGHT 6.0f
// How close the player has to come to collect a note: across, and up from their feet.
#define NOTE_TOUCH_RADIUS 24.0f
#define NOTE_TOUCH_BELOW 10.0f
#define NOTE_TOUCH_ABOVE 50.0f
// DK64's pickup jingle (the one ammo and feathers play in func_global_asm_806F54E0) until
// Banjo-Kazooie's sounds can be played.
#define NOTE_PICKUP_SOUND 0x331

// The HUD counter: Banjo-Kazooie's spinning HUD note and its bold numbers.
#define JIGGY_HUD_NOTE 0x7D9
#define HUD_NOTE_MAX_FRAMES 10
#define HUD_NOTE_MAX_TEXELS (32 * 32)
#define HUD_DIGIT_MAX_TEXELS (32 * 32)
// Ticks are 1/60 s (the engine's lag counter, as in jiggy_cameo.c).
#define HUD_SHOW_TICKS 150
#define HUD_FADE_TICKS 20
#define HUD_TICKS_PER_FRAME 4
#define HUD_MAX_DIGITS 3
// Where the counter sits, bottom left like Banjo-Kazooie's. The left-aligned HUD pulls in by
// the UI pillar, so this keeps it clear of the edge the way DK64's own counters are.
#define HUD_X 56
#define HUD_BOTTOM_MARGIN 36
// The HUD note's color: the sprite is intensity only, tinted the way the game's notes are.
#define HUD_NOTE_R 0xFF
#define HUD_NOTE_G 0xC8
#define HUD_NOTE_B 0x00

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u32 D_global_asm_80744478;
extern s16 D_global_asm_80744494;
s16 playSound(s16 arg0, s32 arg1, f32 arg2, f32 arg3, u8 arg4, u8 arg5);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
// The floor under (x, z), as the game looks for one (func_global_asm_80667180): writes its
// height to *y, leaving it as it was if there's none. The return value isn't reliable.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// After it: whether there's water over that floor, writing the surface's height to *y if so (as
// the game's props check, func_global_asm_8064D0C4).
u8 func_global_asm_8066715C(f32 *y);

// Where the notes go, map by map, each spot found by walking it so all can be reached on
// foot. Each note hovers over the floor found under its spot (or over the spot's own height,
// if none is found near it), at a step through the spin so the trail doesn't turn in lockstep.
typedef struct {
    f32 x, y, z;
    u8 phase;
} NotePlacement;

// The Training Grounds: over the grass below the treehouse deck (where the player starts),
// from the foot of the deck past the pond's edge.
static const NotePlacement notes_training_grounds[] = {
    {1360.0f, 37.0f, 1010.0f, 0},
    {1366.0f, 33.0f, 940.0f, 3},
    {1372.0f, 29.0f, 870.0f, 6},
    {1480.0f, 34.0f, 940.0f, 9},
    {1550.0f, 36.0f, 990.0f, 12},
};
// Jungle Japes: along the path from the entrance through the first clearing.
static const NotePlacement notes_japes[] = {
    {945.0f, 280.0f, 660.0f, 0},
    {962.0f, 280.0f, 560.0f, 3},
    {985.0f, 280.0f, 450.0f, 6},
    {1060.0f, 280.0f, 395.0f, 9},
    {1150.0f, 280.0f, 385.0f, 12},
    {1225.0f, 280.0f, 350.0f, 15},
    {1300.0f, 280.0f, 300.0f, 2},
    {1380.0f, 280.0f, 240.0f, 5},
};
// Gloomy Galleon (fused with Treasure Trove Cove and Clanker's Cavern): trails between DK64's own
// bananas and coins (each note between two of them on the same path, so on the floor they lie
// on), and two rings. Where water covers a note's floor it floats at the surface instead.
static const NotePlacement notes_galleon[] = {
    // Beside the entrance: between the bananas round the open floor east of where the player comes in.
    {2382.0f, 1612.0f, 2697.0f, 0},
    {2428.0f, 1611.0f, 2844.0f, 3},
    {2501.0f, 1611.0f, 3001.0f, 6},
    {2533.0f, 1611.0f, 3144.0f, 9},
    {2539.0f, 1610.0f, 3273.0f, 12},
    {2542.0f, 1610.0f, 3338.0f, 15},
    {2526.0f, 1610.0f, 3455.0f, 0},
    {2439.0f, 1611.0f, 3503.0f, 3},
    {2373.0f, 1610.0f, 3497.0f, 6},
    {2236.0f, 1610.0f, 3480.0f, 9},
    {2110.0f, 1613.0f, 3499.0f, 12},
    // From the entrance east, between the bananas toward the slope up to the east side.
    {2118.0f, 1612.0f, 2894.0f, 15},
    {2123.0f, 1611.0f, 3049.0f, 0},
    {2170.0f, 1610.0f, 3170.0f, 3},
    {2274.0f, 1610.0f, 3224.0f, 6},
    {2401.0f, 1610.0f, 3230.0f, 9},
    {2596.0f, 1617.0f, 3228.0f, 12},
    // Up the slope to the east side and north along it, between its bananas.
    {2722.0f, 1638.0f, 3230.0f, 15},
    {2898.0f, 1662.0f, 3268.0f, 0},
    {2957.0f, 1665.0f, 3287.0f, 3},
    {3015.0f, 1667.0f, 3305.0f, 6},
    {3074.0f, 1670.0f, 3410.0f, 9},
    {3079.0f, 1669.0f, 3582.0f, 12},
    {3126.0f, 1669.0f, 3724.0f, 15},
    // Up the ramp west of the entrance, between its coins.
    {1817.0f, 1558.0f, 2978.0f, 0},
    {1934.0f, 1586.0f, 2980.0f, 3},
    {2056.0f, 1606.0f, 2978.0f, 6},
    // The lighthouse side: south of the lighthouse, between the bananas (on the water when it is up).
    {1676.0f, 1412.0f, 1062.0f, 9},
    {1700.0f, 1413.0f, 1180.0f, 12},
    {1796.0f, 1409.0f, 1242.0f, 15},
    {1894.0f, 1406.0f, 1297.0f, 0},
    {1981.0f, 1398.0f, 1387.0f, 3},
    // Round the lighthouse door and the tag barrel.
    {1873.0f, 1458.0f, 524.0f, 6},
    {1897.0f, 1462.0f, 613.0f, 9},
    {1870.0f, 1462.0f, 667.0f, 12},
    {1922.0f, 1458.0f, 711.0f, 15},
    {2088.0f, 1449.0f, 695.0f, 0},
    // By Cranky's: between the coins and the door, and by the door north-east.
    {1599.0f, 1610.0f, 4176.0f, 3},
    {2028.0f, 1618.0f, 4740.0f, 6},
    // The east side's north end, between two bananas.
    {3256.0f, 1670.0f, 3790.0f, 9},
    // A ring round Nipper (jiggy_nipper.c), outside his claws.
    {3370.0f, 1670.0f, 3660.0f, 12},
    {3338.0f, 1670.0f, 3738.0f, 15},
    {3182.0f, 1670.0f, 3738.0f, 0},
    {3150.0f, 1670.0f, 3660.0f, 3},
    {3182.0f, 1670.0f, 3582.0f, 6},
    {3260.0f, 1670.0f, 3550.0f, 9},
    {3338.0f, 1670.0f, 3582.0f, 12},
    // The east side's south end, between the coins.
    {3254.0f, 1670.0f, 2439.0f, 15},
    {3377.0f, 1670.0f, 2438.0f, 0},
    // A ring on the open floor between the entrance's two banana trails.
    {2390.0f, 1610.0f, 3360.0f, 3},
    {2372.0f, 1610.0f, 3402.0f, 6},
    {2330.0f, 1610.0f, 3420.0f, 9},
    {2288.0f, 1610.0f, 3402.0f, 12},
    {2270.0f, 1610.0f, 3360.0f, 15},
    {2288.0f, 1610.0f, 3318.0f, 0},
    {2330.0f, 1610.0f, 3300.0f, 3},
    {2372.0f, 1610.0f, 3318.0f, 6},
};


// Creepy Castle (fused with Mad Monster Mansion): trails up the castle's hill, each note between
// two of DK64's own colored bananas (the yellow ones round the lower paths, the red ones up the
// path to the top) where the floor runs unbroken from one to the other, so every note sits on
// the same path as the bananas it's between.
static const NotePlacement notes_castle[] = {
    {1550.0f, 473.0f, 446.0f, 0},
    {1616.0f, 475.0f, 482.0f, 3},
    {1717.0f, 485.0f, 584.0f, 6},
    {1750.0f, 492.0f, 650.0f, 9},
    {1808.0f, 504.0f, 769.0f, 12},
    {1832.0f, 507.0f, 822.0f, 15},
    {1919.0f, 481.0f, 993.0f, 0},
    {1956.0f, 504.0f, 1534.0f, 3},
    {1927.0f, 508.0f, 1588.0f, 6},
    {1864.0f, 516.0f, 1694.0f, 9},
    {1829.0f, 521.0f, 1746.0f, 12},
    {1732.0f, 524.0f, 1801.0f, 15},
    {1669.0f, 524.0f, 1805.0f, 0},
    {1552.0f, 524.0f, 1797.0f, 3},
    {1498.0f, 524.0f, 1785.0f, 6},
    {1346.0f, 584.0f, 1710.0f, 9},
    {1453.0f, 887.0f, 1742.0f, 12},
    {1561.0f, 894.0f, 1737.0f, 15},
    {1609.0f, 897.0f, 1734.0f, 0},
    {1703.0f, 902.0f, 1709.0f, 3},
    {1749.0f, 904.0f, 1688.0f, 6},
    {1825.0f, 904.0f, 1630.0f, 9},
    {1854.0f, 904.0f, 1592.0f, 12},
    {1865.0f, 904.0f, 1092.0f, 15},
    {1236.0f, 869.0f, 1879.0f, 0},
    {1138.0f, 863.0f, 1922.0f, 3},
    {1089.0f, 859.0f, 1926.0f, 6},
    {988.0f, 852.0f, 1924.0f, 9},
    {936.0f, 848.0f, 1917.0f, 12},
    {859.0f, 843.0f, 1872.0f, 15},
    {778.0f, 833.0f, 1750.0f, 0},
    {705.0f, 813.0f, 1686.0f, 3},
    {660.0f, 810.0f, 1666.0f, 6},
    {421.0f, 800.0f, 1564.0f, 9},
    {391.0f, 798.0f, 1526.0f, 12},
    {344.0f, 777.0f, 1435.0f, 15},
    {315.0f, 746.0f, 1325.0f, 0},
    {285.0f, 725.0f, 1227.0f, 3},
    {267.0f, 719.0f, 1185.0f, 6},
    {146.0f, 559.0f, 1212.0f, 9},
    {145.0f, 563.0f, 1256.0f, 12},
    {144.0f, 577.0f, 1353.0f, 15},
    {144.0f, 588.0f, 1406.0f, 0},
    {178.0f, 621.0f, 1508.0f, 3},
    {255.0f, 648.0f, 1573.0f, 6},
    {297.0f, 648.0f, 1587.0f, 9},
    {1590.0f, 930.0f, 812.0f, 12},
    {1503.0f, 936.0f, 748.0f, 15},
    {1360.0f, 957.0f, 722.0f, 0},
    {1315.0f, 968.0f, 732.0f, 3},
    {1217.0f, 978.0f, 755.0f, 6},
    {1162.0f, 987.0f, 768.0f, 9},
    {1057.0f, 1012.0f, 795.0f, 12},
    {1005.0f, 1027.0f, 807.0f, 15},
    {901.0f, 1052.0f, 845.0f, 0},
    {847.0f, 1062.0f, 870.0f, 3},
    {772.0f, 1074.0f, 937.0f, 6},
    {750.0f, 1082.0f, 979.0f, 9},
    {686.0f, 1103.0f, 1067.0f, 12},
    {643.0f, 1121.0f, 1113.0f, 15},
    {568.0f, 1128.0f, 1202.0f, 0},
    {535.0f, 1134.0f, 1246.0f, 3},
    {507.0f, 1136.0f, 1399.0f, 6},
    {540.0f, 1136.0f, 1491.0f, 9},
    {571.0f, 1136.0f, 1528.0f, 12},
    {842.0f, 1185.0f, 1637.0f, 15},
    {905.0f, 1235.0f, 1626.0f, 0},
    {1078.0f, 1302.0f, 1486.0f, 3},
    {1128.0f, 1311.0f, 1497.0f, 6},
    {1222.0f, 1335.0f, 1544.0f, 9},
    {1316.0f, 1366.0f, 1622.0f, 12},
    {1179.0f, 1611.0f, 1653.0f, 15},
    {891.0f, 1564.0f, 1631.0f, 0},
    {815.0f, 1553.0f, 1628.0f, 3},
    {683.0f, 1552.0f, 1601.0f, 6},
    {627.0f, 1551.0f, 1577.0f, 9},
    {546.0f, 1550.0f, 1490.0f, 12},
    {521.0f, 1550.0f, 1428.0f, 15},
    {524.0f, 1550.0f, 1301.0f, 0},
    {551.0f, 1549.0f, 1238.0f, 3},
    {634.0f, 1549.0f, 1150.0f, 6},
    {689.0f, 1545.0f, 1125.0f, 9},
    {802.0f, 1530.0f, 1089.0f, 12},
    {860.0f, 1523.0f, 1076.0f, 15},
    {973.0f, 1509.0f, 1046.0f, 0},
    {1028.0f, 1500.0f, 1027.0f, 3},
    {1149.0f, 1471.0f, 1002.0f, 6},
    {1214.0f, 1458.0f, 996.0f, 9},
    {1744.0f, 1647.0f, 1599.0f, 12},
    {1834.0f, 1656.0f, 1486.0f, 15},
    {1864.0f, 1671.0f, 1270.0f, 0},
    {1841.0f, 1678.0f, 1175.0f, 3},
    {1533.0f, 1706.0f, 960.0f, 6},
    {1490.0f, 1712.0f, 960.0f, 9},
};

// Frantic Factory: around the Foyer (where the player comes in from the lobby) and on every
// floor joined to it, each spot over the floor in the map's own floor file and clear of DK64's
// bananas and objects. The entrance corridor and a ring round the Foyer; the doorway north and
// a U round the north room's east side; the west walkway (between DK64's bananas), its bridge
// and a ring on the round platform at its end, round the pole hole; the east corridor, its ramp
// and a ring in the round room at its end.
static const NotePlacement notes_factory[] = {
    {1263.0f, 826.0f, 2755.0f, 0},  {1263.0f, 826.0f, 2715.0f, 3},  {1263.0f, 826.0f, 2675.0f, 6},
    {1250.0f, 831.0f, 2633.0f, 0},  {1162.0f, 831.0f, 2617.0f, 3},  {1098.0f, 831.0f, 2574.0f, 6},
    {1075.0f, 831.0f, 2515.0f, 9},  {1098.0f, 831.0f, 2456.0f, 12}, {1162.0f, 831.0f, 2413.0f, 15},
    {1250.0f, 831.0f, 2397.0f, 0},  {1338.0f, 831.0f, 2413.0f, 3},  {1402.0f, 831.0f, 2456.0f, 6},
    {1425.0f, 831.0f, 2515.0f, 9},  {1402.0f, 831.0f, 2574.0f, 12}, {1338.0f, 831.0f, 2617.0f, 15},
    {1265.0f, 836.0f, 2306.0f, 0},
    {1330.0f, 840.0f, 2245.0f, 0},  {1390.0f, 840.0f, 2245.0f, 3},  {1450.0f, 840.0f, 2245.0f, 6},
    {1465.0f, 840.0f, 2195.0f, 9},  {1465.0f, 840.0f, 2150.0f, 12}, {1465.0f, 840.0f, 2105.0f, 15},
    {1410.0f, 840.0f, 2090.0f, 2},  {1370.0f, 840.0f, 2090.0f, 5},  {1330.0f, 840.0f, 2090.0f, 8},
    {915.0f, 812.0f, 2500.0f, 0},   {880.0f, 802.0f, 2490.0f, 3},   {806.0f, 802.0f, 2414.0f, 6},
    {716.0f, 802.0f, 2320.0f, 9},   {660.0f, 804.0f, 2235.0f, 12},  {660.0f, 804.0f, 2185.0f, 15},
    {685.0f, 804.0f, 2140.0f, 2},
    {591.0f, 804.0f, 2034.0f, 0},   {516.0f, 804.0f, 1959.0f, 3},   {516.0f, 804.0f, 1851.0f, 6},
    {591.0f, 804.0f, 1776.0f, 9},   {699.0f, 804.0f, 1776.0f, 12},  {774.0f, 804.0f, 1851.0f, 15},
    {774.0f, 804.0f, 1959.0f, 2},   {699.0f, 804.0f, 2034.0f, 5},
    {1495.0f, 838.0f, 2470.0f, 0},  {1560.0f, 836.0f, 2510.0f, 3},  {1610.0f, 836.0f, 2505.0f, 6},
    {1662.0f, 836.0f, 2510.0f, 9},  {1690.0f, 836.0f, 2345.0f, 12}, {1735.0f, 836.0f, 2310.0f, 15},
    {1805.0f, 850.0f, 2390.0f, 2},
    {2046.0f, 862.0f, 2425.0f, 0},  {1985.0f, 862.0f, 2460.0f, 3},  {1924.0f, 862.0f, 2425.0f, 6},
    {1924.0f, 862.0f, 2355.0f, 9},  {1985.0f, 862.0f, 2320.0f, 12}, {2046.0f, 862.0f, 2355.0f, 15},
};

// Fungi Forest (fused with Click Clock Wood): 82 notes in rings and trails through its five
// main areas, each spot on DK64's own floor (the map's floor triangles, and next to its own
// bananas and coins), flat for 25 units round, clear of its objects and doors:
//   up from the entrance (2309, 192, 1655) and round the clock tower's hill (330 out from the
//   clock, 220 to 270 high);
//   along the Tiny bananas from the clock's hill into the apple area, then round the apple's
//   mound (400 out, over the coins' ring);
//   round the giant mushroom, on the low ground (99) inside the walkway ring;
//   round the mill on its plateau (400 out from it, about 245 high);
//   along the owl tree's ring path (83 high), a quarter of the way between DK64's Tiny bananas
//   so as not to sit on them or the little mushrooms halfway between them.
static const NotePlacement notes_fungi[] = {
    // From the entrance up the clock's hill.
    {2330.0f, 207.0f, 1760.0f, 0},
    {2353.0f, 218.0f, 1827.0f, 7},
    {2377.0f, 228.0f, 1893.0f, 14},
    {2400.0f, 244.0f, 1960.0f, 3},
    // Round the clock tower.
    {2800.0f, 257.0f, 2330.0f, 10},
    {2775.0f, 251.0f, 2456.0f, 17},
    {2703.0f, 239.0f, 2563.0f, 6},
    {2596.0f, 229.0f, 2635.0f, 13},
    {2470.0f, 222.0f, 2660.0f, 2},
    {2344.0f, 221.0f, 2635.0f, 9},
    {2237.0f, 229.0f, 2563.0f, 16},
    {2165.0f, 235.0f, 2456.0f, 5},
    {2140.0f, 257.0f, 2330.0f, 12},
    {2165.0f, 254.0f, 2204.0f, 1},
    {2237.0f, 262.0f, 2097.0f, 8},
    {2344.0f, 258.0f, 2025.0f, 15},
    {2470.0f, 259.0f, 2000.0f, 4},
    {2596.0f, 254.0f, 2025.0f, 11},
    {2703.0f, 268.0f, 2097.0f, 0},
    {2775.0f, 263.0f, 2204.0f, 7},
    // Along the Tiny bananas into the apple area.
    {2700.0f, 201.0f, 1600.0f, 14},
    {2780.0f, 201.0f, 1505.0f, 3},
    {2870.0f, 201.0f, 1470.0f, 10},
    {2985.0f, 201.0f, 1440.0f, 17},
    {3087.0f, 203.0f, 1254.0f, 6},
    {3120.0f, 200.0f, 1130.0f, 13},
    // Round the apple's mound.
    {3550.0f, 190.0f, 720.0f, 2},
    {3510.0f, 191.0f, 894.0f, 9},
    {3399.0f, 196.0f, 1033.0f, 16},
    {3239.0f, 198.0f, 1110.0f, 5},
    {3061.0f, 199.0f, 1110.0f, 12},
    {2790.0f, 198.0f, 894.0f, 1},
    {2750.0f, 196.0f, 720.0f, 8},
    {2790.0f, 195.0f, 546.0f, 15},
    {2901.0f, 190.0f, 407.0f, 4},
    {3061.0f, 189.0f, 330.0f, 11},
    {3239.0f, 188.0f, 330.0f, 0},
    {3399.0f, 189.0f, 407.0f, 7},
    {3510.0f, 187.0f, 546.0f, 14},
    // Round the giant mushroom, on the low ground.
    {1520.0f, 99.0f, 880.0f, 3},
    {1487.0f, 99.0f, 1044.0f, 10},
    {1394.0f, 99.0f, 1189.0f, 17},
    {1250.0f, 99.0f, 1296.0f, 6},
    {1074.0f, 99.0f, 1353.0f, 13},
    {886.0f, 99.0f, 1353.0f, 2},
    {710.0f, 99.0f, 1296.0f, 9},
    {566.0f, 99.0f, 1189.0f, 16},
    {473.0f, 99.0f, 1044.0f, 5},
    {440.0f, 99.0f, 880.0f, 12},
    {473.0f, 99.0f, 716.0f, 1},
    {710.0f, 99.0f, 464.0f, 8},
    {886.0f, 99.0f, 407.0f, 15},
    // Round the mill.
    {1700.0f, 239.0f, 4003.0f, 4},
    {1613.0f, 244.0f, 4133.0f, 11},
    {1483.0f, 250.0f, 4220.0f, 0},
    {1330.0f, 247.0f, 4250.0f, 7},
    {1177.0f, 248.0f, 4220.0f, 14},
    {1047.0f, 248.0f, 4133.0f, 3},
    {960.0f, 249.0f, 4003.0f, 10},
    {930.0f, 243.0f, 3850.0f, 17},
    {960.0f, 249.0f, 3697.0f, 6},
    {1177.0f, 244.0f, 3480.0f, 13},
    {1330.0f, 241.0f, 3450.0f, 2},
    {1483.0f, 243.0f, 3480.0f, 9},
    {1613.0f, 243.0f, 3567.0f, 16},
    {1700.0f, 233.0f, 3697.0f, 5},
    // Along the owl tree's ring path.
    {3864.0f, 83.0f, 3591.0f, 12},
    {3997.0f, 83.0f, 3418.0f, 1},
    {4203.0f, 83.0f, 3270.0f, 8},
    {4379.0f, 83.0f, 3228.0f, 15},
    {4549.0f, 83.0f, 3223.0f, 4},
    {4720.0f, 83.0f, 3277.0f, 11},
    {4830.0f, 83.0f, 3394.0f, 0},
    {4900.0f, 83.0f, 3556.0f, 7},
    {4834.0f, 83.0f, 3742.0f, 14},
    {4714.0f, 83.0f, 3856.0f, 3},
    {4470.0f, 83.0f, 4055.0f, 10},
    {4336.0f, 83.0f, 4137.0f, 17},
    {4154.0f, 83.0f, 4124.0f, 6},
    {4006.0f, 83.0f, 4070.0f, 13},
    {3901.0f, 93.0f, 3944.0f, 2},
    {3838.0f, 103.0f, 3789.0f, 9},
};
#define NOTE_TRAIL(map, notes) {map, notes, sizeof(notes) / sizeof(notes[0])}
static const struct {
    Maps map;
    const NotePlacement *notes;
    u32 count;
} note_trails[] = {
    NOTE_TRAIL(MAP_TRAINING_GROUNDS, notes_training_grounds),
    NOTE_TRAIL(MAP_JAPES, notes_japes),
    NOTE_TRAIL(MAP_GALLEON, notes_galleon),
    NOTE_TRAIL(MAP_CASTLE, notes_castle),
    NOTE_TRAIL(MAP_FACTORY, notes_factory),
    NOTE_TRAIL(MAP_FUNGI, notes_fungi),
};
// The most notes on one map, and the u32s of collected bits that takes (each saved under its own
// key: NOTE_SAVE_MAP_KEY for the first, NOTE_SAVE_MAP_WORD_KEY for the rest).
#define NOTE_COUNT_MAX 128
#define NOTE_WORDS ((NOTE_COUNT_MAX + 31) / 32)
// Notes further than this from the player aren't drawn (a big map's trails would fill the
// display list for nothing).
#define NOTE_DRAW_DISTANCE 1400.0f
// Looking for the floor under a note: from this far above its spot, trusted this close to it.
#define NOTE_FLOOR_PROBE 20.0f
#define NOTE_FLOOR_MAX_STEP 40.0f

typedef enum {
    NOTES_UNLOADED,
    NOTES_READY,
    NOTES_UNAVAILABLE, // No Banjo-Kazooie ROM, or the sprite didn't decode.
} NotesLoadState;

static u16 note_texels[NOTE_MAX_FRAMES][NOTE_MAX_TEXELS] __attribute__((aligned(8)));
static s16 note_width[NOTE_MAX_FRAMES];
static s16 note_height[NOTE_MAX_FRAMES];
// A quad per strip of each frame, plain and mirrored, centered on the note.
static Vtx note_vtx[NOTE_MAX_FRAMES][2][NOTE_MAX_STRIPS][4] __attribute__((aligned(8)));
static Mtx note_mtx[2][NOTE_COUNT_MAX];
static s32 note_frames;
static NotesLoadState note_state = NOTES_UNLOADED;
static s32 note_last_map = -1;
static s32 note_placed;
static f32 note_pos[NOTE_COUNT_MAX][3];
// This map's notes (none if it has none), picked when the map changes.
static const NotePlacement *note_trail;
static u32 note_count;
// Bit i % 32 of word i / 32: note_trail[i] has been collected, for the map the notes were last
// loaded on.
static u32 note_collected[NOTE_WORDS];
static s32 notes_total;
// Save keys (jiggy_save_get): the collected bits of a map's notes, and the total.
#define NOTE_SAVE_MAP_KEY(map) (0x4E000000 | (u32)(map)) // 'N'
#define NOTE_SAVE_MAP_WORD_KEY(map, word) (0x4E000000 | ((u32)(word) << 8) | (u32)(map))
#define NOTE_SAVE_TOTAL_KEY 0x4EFFFFFF
#define NOTE_IS_COLLECTED(i) (note_collected[(i) >> 5] & (1u << ((i) & 31)))

static u32 note_save_key(u32 word) {
    return word == 0 ? NOTE_SAVE_MAP_KEY(current_map) : NOTE_SAVE_MAP_WORD_KEY(current_map, word);
}

static NotesLoadState hud_state = NOTES_UNLOADED;
static u16 hud_note_texels[HUD_NOTE_MAX_FRAMES][HUD_NOTE_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_note_width[HUD_NOTE_MAX_FRAMES];
static s16 hud_note_height[HUD_NOTE_MAX_FRAMES];
static s32 hud_note_frames;
static u16 hud_digit_texels[10][HUD_DIGIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_digit_width[10];
static s16 hud_digit_height[10];
// Ticks since the counter last came up; it's hidden once it reaches the show time.
static u32 hud_ticks = HUD_SHOW_TICKS;

static void note_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
    v->v.ob[0] = x;
    v->v.ob[1] = y;
    v->v.ob[2] = 0;
    v->v.flag = 0;
    v->v.tc[0] = s << 5; // 10.5 fixed point
    v->v.tc[1] = t << 5;
    v->v.cn[0] = 0xFF;
    v->v.cn[1] = 0xFF;
    v->v.cn[2] = 0xFF;
    v->v.cn[3] = 0xFF;
}

// Vertices in texels, so a frame's quad is width x height before the note's matrix scales it.
static void note_build_vtx(s32 frame) {
    s32 width = note_width[frame];
    s32 height = note_height[frame];
    s32 rows_per_load = TMEM_RGBA16_TEXELS / width;
    s32 mirrored;
    s32 strip;
    s32 row;

    for (mirrored = 0; mirrored < 2; mirrored++) {
        s32 left = mirrored ? width : 0;
        s32 right = mirrored ? 0 : width;

        for (row = 0, strip = 0; row < height; row += rows_per_load, strip++) {
            s32 rows = row + rows_per_load > height ? height - row : rows_per_load;
            s32 top = height / 2 - row;
            s32 bottom = top - rows;
            Vtx *v = note_vtx[frame][mirrored][strip];

            note_set_vtx(&v[0], -width / 2, top, left, 0);
            note_set_vtx(&v[1], width / 2, top, right, 0);
            note_set_vtx(&v[2], width / 2, bottom, right, rows);
            note_set_vtx(&v[3], -width / 2, bottom, left, rows);
        }
    }
}

static void notes_load(void) {
    s32 count;
    s32 i;

    note_state = NOTES_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_MUSIC_NOTE);
    if (count <= 0) {
        return;
    }
    if (count > NOTE_MAX_FRAMES) {
        count = NOTE_MAX_FRAMES;
    }
    // Every frame is needed for the spin, so one that fails leaves the notes out.
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_MUSIC_NOTE, i, note_texels[i], sizeof(note_texels[0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;

        if (size <= 0 || width == 0 || height == 0 || width * height > NOTE_MAX_TEXELS ||
            width > TMEM_RGBA16_TEXELS) {
            return;
        }
        note_width[i] = width;
        note_height[i] = height;
        note_build_vtx(i);
    }
    note_frames = count;
    note_state = NOTES_READY;
}

// Lays the trail out, each note over the floor at its spot.
static void notes_place(Actor *player) {
    u32 i;

    for (i = 0; i < note_count; i++) {
        f32 probe = note_trail[i].y + NOTE_FLOOR_PROBE;
        f32 floor = probe;
        f32 ground = note_trail[i].y;

        func_global_asm_80667110(note_trail[i].x, note_trail[i].z, &floor);
        if (floor != probe && floor - note_trail[i].y < NOTE_FLOOR_MAX_STEP &&
            note_trail[i].y - floor < NOTE_FLOOR_MAX_STEP) {
            ground = floor;
        }
        // Water over the floor (func_global_asm_8066715C, after the floor lookup): the note
        // floats at the surface, for a swimmer to take.
        {
            f32 water;

            if (func_global_asm_8066715C(&water) && water > ground && water - note_trail[i].y < NOTE_FLOOR_MAX_STEP &&
                note_trail[i].y - water < NOTE_FLOOR_MAX_STEP + NOTE_HOVER) {
                ground = water;
            }
        }
        note_pos[i][0] = note_trail[i].x;
        note_pos[i][1] = ground + NOTE_HOVER;
        note_pos[i][2] = note_trail[i].z;
    }
    note_placed = TRUE;
}

// Collects the notes the player is touching.
static void notes_collect(Actor *player) {
    u32 i;

    for (i = 0; i < note_count; i++) {
        f32 dx, dy, dz;

        if (NOTE_IS_COLLECTED(i)) {
            continue;
        }
        dx = note_pos[i][0] - player->x_position;
        dy = note_pos[i][1] - player->y_position;
        dz = note_pos[i][2] - player->z_position;
        if (dx * dx + dz * dz > NOTE_TOUCH_RADIUS * NOTE_TOUCH_RADIUS || dy < -NOTE_TOUCH_BELOW ||
            dy > NOTE_TOUCH_ABOVE) {
            continue;
        }
        note_collected[i >> 5] |= 1u << (i & 31);
        notes_total++;
        jiggy_save_set(current_file, note_save_key(i >> 5), note_collected[i >> 5]);
        jiggy_save_set(current_file, NOTE_SAVE_TOTAL_KEY, notes_total);
        hud_ticks = 0;
        // Banjo-Kazooie's note jingle, or DK64's pickup sound without a Banjo-Kazooie ROM.
        if (jiggy_bk_jingle_play(JIGGY_BK_JINGLE_NOTE, 16000.0f / 32767.0f) <= 0) {
            playSound(NOTE_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
        }
    }
}

// A matrix that stands the note at pos facing the camera, sized for a frame `width` texels
// wide. Rows are the note's right, up and toward-the-camera axes (row vectors, as guMtxF2L
// takes them).
static void note_billboard(Mtx *m, f32 pos[3], s32 width) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = NOTE_SIZE / width;

    f[0] = cc->look_at_eye_x - pos[0];
    f[1] = cc->look_at_eye_y - pos[1];
    f[2] = cc->look_at_eye_z - pos[2];
    length = _sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (length < 1.0f) {
        length = 1.0f;
    }
    f[0] /= length;
    f[1] /= length;
    f[2] /= length;
    // right = up x toward-camera; straight down on the note, any right will do.
    r[0] = f[2];
    r[1] = 0.0f;
    r[2] = -f[0];
    length = _sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (length < 0.001f) {
        r[0] = 1.0f;
        r[2] = 0.0f;
        length = 1.0f;
    }
    r[0] /= length;
    r[2] /= length;
    u[0] = f[1] * r[2] - f[2] * r[1];
    u[1] = f[2] * r[0] - f[0] * r[2];
    u[2] = f[0] * r[1] - f[1] * r[0];

    mf[0][0] = r[0] * scale; mf[0][1] = r[1] * scale; mf[0][2] = r[2] * scale; mf[0][3] = 0.0f;
    mf[1][0] = u[0] * scale; mf[1][1] = u[1] * scale; mf[1][2] = u[2] * scale; mf[1][3] = 0.0f;
    mf[2][0] = f[0] * scale; mf[2][1] = f[1] * scale; mf[2][2] = f[2] * scale; mf[2][3] = 0.0f;
    mf[3][0] = pos[0];       mf[3][1] = pos[1];       mf[3][2] = pos[2];       mf[3][3] = 1.0f;
    guMtxF2L(mf, m);
}

static Gfx *note_draw(Gfx *dl, Mtx *m, s32 frame, s32 mirrored) {
    s32 width = note_width[frame];
    s32 height = note_height[frame];
    s32 rows_per_load = TMEM_RGBA16_TEXELS / width;
    s32 strip;
    s32 row;

    gSPMatrix(dl++, m, G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    for (row = 0, strip = 0; row < height; row += rows_per_load, strip++) {
        s32 rows = row + rows_per_load > height ? height - row : rows_per_load;

        gDPLoadTextureTile(dl++, &note_texels[frame][row * width], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, rows,
            0, 0, width - 1, rows - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(dl++, note_vtx[frame][mirrored][strip], 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
    }
    gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); draws the
// map's notes once a frame, with the player.
Gfx *jiggy_notes_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 steps;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != note_last_map) {
        note_last_map = current_map;
        note_placed = FALSE;
        // Read back on every map, which also catches a different save file being loaded.
        for (i = 0; i < NOTE_WORDS; i++) {
            note_collected[i] = jiggy_save_get(current_file, note_save_key(i));
        }
        notes_total = jiggy_save_get(current_file, NOTE_SAVE_TOTAL_KEY);
        note_count = 0;
        for (i = 0; i < sizeof(note_trails) / sizeof(note_trails[0]); i++) {
            if (note_trails[i].map == current_map) {
                note_trail = note_trails[i].notes;
                note_count = note_trails[i].count < NOTE_COUNT_MAX ? note_trails[i].count : NOTE_COUNT_MAX;
            }
        }
    }
    if (note_count == 0) {
        return dl;
    }
    if (note_state == NOTES_UNLOADED) {
        notes_load();
    }
    if (note_state != NOTES_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!note_placed) {
        if (actor->y_position - actor->floor > NOTE_AIRBORNE_HEIGHT) {
            return dl;
        }
        notes_place(actor);
    }
    notes_collect(actor);

    gDPPipeSync(dl++);
    gSPClearGeometryMode(dl++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
    gSPSetGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_PERSP);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_BILERP);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(dl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);

    // A spin is the frames forward, then back mirrored without repeating either end.
    steps = (note_frames - 1) * 2;
    if (steps == 0) {
        steps = 1;
    }
    for (i = 0; i < note_count; i++) {
        u32 step = (object_timer / NOTE_TICKS_PER_FRAME + note_trail[i].phase) % steps;
        f32 dx = note_pos[i][0] - actor->x_position;
        f32 dz = note_pos[i][2] - actor->z_position;
        if (NOTE_IS_COLLECTED(i) || dx * dx + dz * dz > NOTE_DRAW_DISTANCE * NOTE_DRAW_DISTANCE) {
            continue;
        }
        s32 mirrored = step >= (u32)note_frames;
        s32 frame = mirrored ? steps - step : step;

        note_billboard(&note_mtx[buffer][i], note_pos[i], note_width[frame]);
        dl = note_draw(dl, &note_mtx[buffer][i], frame, mirrored);
    }

    gDPPipeSync(dl++);
    return dl;
}

static s32 hud_load_rgba16(s32 size, s16 *width, s16 *height, s32 max_texels) {
    s32 w = (size >> 16) & 0xFFFF;
    s32 h = size & 0xFFFF;

    if (size <= 0 || w == 0 || h == 0 || w * h > max_texels) {
        return FALSE;
    }
    *width = w;
    *height = h;
    return TRUE;
}

static void hud_load(void) {
    s32 count;
    s32 i;

    hud_state = NOTES_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_HUD_NOTE);
    if (count <= 0) {
        return;
    }
    if (count > HUD_NOTE_MAX_FRAMES) {
        count = HUD_NOTE_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_HUD_NOTE, i, hud_note_texels[i], sizeof(hud_note_texels[0]));
        if (!hud_load_rgba16(size, &hud_note_width[i], &hud_note_height[i], HUD_NOTE_MAX_TEXELS)) {
            return;
        }
    }
    hud_note_frames = count;
    for (i = 0; i < 10; i++) {
        s32 size = jiggy_bk_bold_number_rgba16(i, hud_digit_texels[i], sizeof(hud_digit_texels[0]));
        if (!hud_load_rgba16(size, &hud_digit_width[i], &hud_digit_height[i], HUD_DIGIT_MAX_TEXELS)) {
            return;
        }
    }
    hud_state = NOTES_READY;
}

// One RGBA16 image as a texture rectangle at (x, y), top left, in screen pixels. Every image
// here fits in TMEM in one load.
static Gfx *hud_draw_image(Gfx *dl, u16 *texels, s32 width, s32 height, s32 x, s32 y) {
    gDPLoadTextureTile(dl++, texels, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        width, height,
        0, 0, width - 1, height - 1,
        0,
        G_TX_CLAMP, G_TX_CLAMP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(dl++,
        x << 2, y << 2, (x + width) << 2, (y + height) << 2,
        G_TX_RENDERTILE,
        0, 0, 1 << 10, 1 << 10);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the note
// counter, for a moment after each note.
Gfx *jiggy_notes_hud_draw(Gfx *dl) {
    s32 digits[HUD_MAX_DIGITS];
    s32 digit_count = 0;
    s32 value;
    s32 frame;
    s32 alpha;
    s32 x, y;
    s32 i;

    if (hud_ticks >= HUD_SHOW_TICKS || cc_number_of_players != 1) {
        return dl;
    }
    if (hud_state == NOTES_UNLOADED) {
        hud_load();
    }
    if (hud_state != NOTES_READY) {
        hud_ticks = HUD_SHOW_TICKS;
        return dl;
    }
    hud_ticks += D_global_asm_80744478;
    if (hud_ticks >= HUD_SHOW_TICKS) {
        return dl;
    }
    alpha = 0xFF;
    if (hud_ticks > HUD_SHOW_TICKS - HUD_FADE_TICKS) {
        alpha = 0xFF * (s32)(HUD_SHOW_TICKS - hud_ticks) / HUD_FADE_TICKS;
    }

    value = notes_total;
    do {
        digits[digit_count++] = value % 10;
        value /= 10;
    } while (value > 0 && digit_count < HUD_MAX_DIGITS);

    frame = (object_timer / HUD_TICKS_PER_FRAME) % hud_note_frames;
    x = HUD_X;
    y = D_global_asm_80744494 - hud_note_height[frame] - HUD_BOTTOM_MARGIN;

    dl = alignHUDGameplay(dl, ALIGN_LEFT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(dl++, 0, 0, HUD_NOTE_R, HUD_NOTE_G, HUD_NOTE_B, alpha);
    dl = hud_draw_image(dl, hud_note_texels[frame], hud_note_width[frame], hud_note_height[frame], x, y);

    // The numbers carry their own colors.
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
    x += hud_note_width[frame] + 4;
    for (i = digit_count - 1; i >= 0; i--) {
        s32 d = digits[i];
        s32 dy = y + (hud_note_height[frame] - hud_digit_height[d]) / 2;

        dl = hud_draw_image(dl, hud_digit_texels[d], hud_digit_width[d], hud_digit_height[d], x, dy);
        x += hud_digit_width[d];
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_LEFT);
    return dl;
}
