
#include "global.h"
#include "m4a.h"
#include "gba/m4a_internal.h"

void Draw3dsBottomUi(void) {}
void Update3dsBottomUI(void) {}
void CtrBag_CheckUpdate(void) {}
void CtrParty_Touch(void) {}

// RFU
u32 rfu_CHILD_getConnectRecoveryStatus(void) { return 0; }
void rfu_REQ_CHILD_endConnectRecovery(void) {}
void rfu_REQ_CHILD_pollConnectRecovery(void) {}
void rfu_REQ_CHILD_startConnectRecovery(void) {}

// GameCube
void GameCubeMultiBoot_HandleSerialInterrupt(void) {}
void GameCubeMultiBoot_Main(void) {}
void GameCubeMultiBoot_Init(void) {}
void GameCubeMultiBoot_ExecuteProgram(void) {}
void GameCubeMultiBoot_Quit(void) {}
const u8 gMultiBootProgram_PokemonColosseum_Start[1] = {0};

// Mystery Gift
void CB2_MysteryGiftEReader(void) {}
void CB2_InitMysteryGift(void) {}

// Sound
// TrackStop implemented in music_player.c
char gNumMusicPlayers[1] = {0};

// Misc
void RegisterRamReset(u32 flags) {}

#include "global.h"
#include "sprite.h"

void CtrPokedex_Backdrop(void) {}
int CtrTitleScreen_RayquazaBg(void) { return -1; }
void CtrPokenavList_Bg(void) {}
void CtrPokenavRegionMap_InfoBg(void) {}
void CtrPokenav_HeaderTiles(void) {}
void CtrBattleIntro_Sliding(void) {}
void CtrBattleBg_MoveBgShown(void) {}

bool8 CtrSprite_IsVoxelWeather(struct Sprite *sprite) { return 0; }
void *PortDoor_Get(u8 x) { return NULL; }

const void *gTileset_Dewford = NULL;
const void *gTileset_Fortree = NULL;

u32 UNPACK_BEHAVIOR(u32 a) { return 0; }
bool8 MetatileBehavior_IsSurfableWaterOrUnderwater(u8 b) { return 0; }

void Port_PreloadLatinFonts(void) {}
void Port_SaveInit(void) {}

const u16 *GetOverworldTextboxPalettePtr(void) { return NULL; }
const void *GetItemIconPicOrPalette(u16 itemId, u8 which) { return NULL; }

void Port_SetAudioDebugEx(u16 tag, u16 a, u16 b, u16 c, u16 d)
{
    (void)tag; (void)a; (void)b; (void)c; (void)d;
}
