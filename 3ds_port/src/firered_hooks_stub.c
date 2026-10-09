#include "global.h"
#include "menu.h"
#include "task.h"
#include "main.h"
#include "sound.h"
#include "constants/songs.h"

// --- 3DS UI Input Variables ---
static struct {
    bool8 pending;
    s16 x, y;
    u32 frame;
} sCtrMenuTap;

// --- Start Menu ---
static u8 sCtrStartMenuRequest = 0xFF;

bool8 CtrStartMenu_Available(void)
{
    // Menus are available if we are actively in the overworld
    return (gMain.callback1 == CB1_Overworld);
}

bool8 CtrStartMenu_Busy(void)
{
    return sCtrStartMenuRequest != 0xFF;
}

void CtrStartMenu_Request(u8 target)
{
    if (CtrStartMenu_Available())
    {
        sCtrStartMenuRequest = target;
        // Pushing START button safely cues the GBA engine to open the menu.
        // We simulate the button press to let the game handle the overworld teardown cleanly.
        gMain.newKeys |= START_BUTTON;
    }
}

// --- Menu Taps ---
void CtrMenu_PostTap(s16 x, s16 y)
{
    sCtrMenuTap.pending = TRUE;
    sCtrMenuTap.x = x;
    sCtrMenuTap.y = y;
    sCtrMenuTap.frame = gMain.vblankCounter1;
}

bool8 CtrMenu_TakeTap(s16 *x, s16 *y)
{
    if (!sCtrMenuTap.pending)
        return FALSE;
    sCtrMenuTap.pending = FALSE;
    if (gMain.vblankCounter1 - sCtrMenuTap.frame > 30)
        return FALSE;
    *x = sCtrMenuTap.x;
    *y = sCtrMenuTap.y;
    return TRUE;
}

bool8 CtrMenu_TakeAck(void)
{
    s16 x, y;
    return CtrMenu_TakeTap(&x, &y);
}

s8 CtrMenu_EntryAt(s16 x, s16 y)
{
    // Standard Yes/No box dimensions on GBA screen
    if (x > 160 && y > 60 && y < 100) return 0; // YES
    if (x > 160 && y >= 100 && y < 140) return 1; // NO
    return -1;
}

s8 CtrMenu_Choose(s8 entry, bool8 sound)
{
    if (sound) PlaySE(SE_SELECT);
    return entry;
}

s8 CtrMenu_YesNoInput(void)
{
    s16 x, y;
    s8 entry;

    if (!CtrMenu_TakeTap(&x, &y))
        return Menu_ProcessInputNoWrapClearOnChoose();
    
    entry = CtrMenu_EntryAt(x, y);
    if (entry >= 0)
        CtrMenu_Choose(entry, TRUE);
    else
        entry = MENU_B_PRESSED;
        
    EraseYesNoWindow();
    return entry;
}

// --- Summary Screen ---
void CtrSummary_Tap(s16 x, s16 y)
{
    // The screen has 3 pages. Left/Right tap changes pages.
    if (x < 120)
        gMain.newKeys |= DPAD_LEFT;
    else
        gMain.newKeys |= DPAD_RIGHT;
}

// --- Party Menu ---
bool8 CtrParty_Close(bool8 leaving)
{
    return FALSE;
}

// --- PokeNav (Permanently empty for FireRed) ---
void CtrPokenavMenu_SetCursor(int cursor) {}
void CtrPokenavList_SetSelected(u16 cursor) {}
void CtrPokenavMatchCall_SetOption(u16 cursor) {}
void CtrMonMarkings_SetCursor(s8 cursor) {}
int CtrPokenavMenu_Options(int *cursor) { return 0; }
void CtrPokenavMenu_Rows(int *yStart, int *deltaY) {}
bool8 CtrPokenavCondition_Marking(void) { return FALSE; }
bool8 CtrMonMarkings_Menu(int *mark, int *mx, int *my) { return FALSE; }
