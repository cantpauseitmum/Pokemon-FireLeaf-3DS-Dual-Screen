#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
#include "gba/gba.h"

#define CTR_BOT_WIDTH  320
#define CTR_BOT_HEIGHT 240

// --- Kolorystyka Teachy TV ---
#define COLOR_TV_BG         C2D_Color32(30, 30, 30, 255)
#define COLOR_TV_FRAME      C2D_Color32(100, 100, 100, 255)
#define COLOR_TV_SCREEN     C2D_Color32(50, 150, 250, 255) // Ekran początkowy
#define COLOR_TV_TEXT       C2D_Color32(255, 255, 255, 255)
#define COLOR_TV_BUTTON     C2D_Color32(200, 50, 50, 255)  // Zakończ

static bool sTeachyTvActive = false;
static int sCurrentChannel = 0;

static C2D_TextBuf sTeachyTextBuf;
static C2D_Text sTvTitleText;
static C2D_Text sTvInfoText;
static C2D_Text sTvExitText;

void CtrTeachyTv_Init(void) {
    sTeachyTextBuf = C2D_TextBufNew(1024);
    
    C2D_TextParse(&sTvTitleText, sTeachyTextBuf, "TEACHY TV");
    C2D_TextOptimize(&sTvTitleText);
    
    C2D_TextParse(&sTvInfoText, sTeachyTextBuf, "Select channel (A/B)");
    C2D_TextOptimize(&sTvInfoText);
    
    C2D_TextParse(&sTvExitText, sTeachyTextBuf, "EXIT (START)");
    C2D_TextOptimize(&sTvExitText);
}

void CtrTeachyTv_Exit(void) {
    if (sTeachyTextBuf) {
        C2D_TextBufDelete(sTeachyTextBuf);
    }
}

void CtrTeachyTv_Toggle(void) {
    sTeachyTvActive = !sTeachyTvActive;
    if (sTeachyTvActive) {
        sCurrentChannel = 0;
        if (!sTeachyTextBuf) {
            CtrTeachyTv_Init();
        }
    }
}

void CtrTeachyTv_Draw(void) {
    if (!sTeachyTvActive) return;

    C2D_TargetClear(C2D_GetBottom(), COLOR_TV_BG);
    C2D_SceneBegin(C2D_GetBottom());

    // Obudowa Telewizora
    C2D_DrawRectSolid(10, 10, 0, 300, 220, COLOR_TV_FRAME);
    
    // Ekran Telewizora
    u32 screenColor = COLOR_TV_SCREEN;
    if (sCurrentChannel == 1) screenColor = C2D_Color32(50, 200, 50, 255); // Kanał walki
    if (sCurrentChannel == 2) screenColor = C2D_Color32(200, 50, 200, 255); // Kanał łapania
    
    C2D_DrawRectSolid(20, 20, 0, 280, 150, screenColor);
    
    if (sTeachyTextBuf) {
        C2D_DrawText(&sTvTitleText, C2D_WithColor, 120, 25, 0.5f, 0.6f, 0.6f, COLOR_TV_TEXT);
        C2D_DrawText(&sTvInfoText, C2D_WithColor, 80, 180, 0.5f, 0.5f, 0.5f, COLOR_TV_TEXT);
        C2D_DrawText(&sTvExitText, C2D_WithColor, 95, 200, 0.5f, 0.5f, 0.5f, COLOR_TV_BUTTON);
    }
}

void CtrTeachyTv_Touch(touchPosition touch) {
    if (!sTeachyTvActive) return;

    // Prosta obsługa dotyku do zmiany kanałów
    if (touch.py >= 20 && touch.py <= 170 && touch.px >= 20 && touch.px <= 300) {
        sCurrentChannel = (sCurrentChannel + 1) % 3;
    }
    
    // Zakończenie
    if (touch.py >= 190 && touch.py <= 220 && touch.px >= 80 && touch.px <= 240) {
        sTeachyTvActive = false;
        gMain.newKeys |= START_BUTTON; // Powrót
    }
}
