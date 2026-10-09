#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
#include "gba/gba.h"

#define CTR_BOT_WIDTH  320
#define CTR_BOT_HEIGHT 240

// --- Kolorystyka Fame Checker ---
#define COLOR_BG        C2D_Color32(252, 248, 240, 255) // Pergaminowe tło
#define COLOR_PANEL     C2D_Color32(248, 168, 120, 255) // Koralowe panele
#define COLOR_BUTTON    C2D_Color32(250, 240, 230, 255) // Jasne przyciski
#define COLOR_BORDER    C2D_Color32(220, 110,  80, 255) // Ciemny koralowy
#define COLOR_TEXT_DARK C2D_Color32(64,  40,   30, 255) // Ciemny brąz na tekst
#define COLOR_TITLE     C2D_Color32(255, 255, 255, 255) // Biały na tytuł
#define COLOR_SHADOW    C2D_Color32(0,   0,    0,  80)  // Cień

enum FameCheckerState {
    FAME_STATE_CHARACTER_SELECT,
    FAME_STATE_MESSAGE_VIEW
};

static int sFameState = FAME_STATE_CHARACTER_SELECT;
static int sSelectedCharacter = -1;
static bool sFameActive = false;

// Bufory tekstu dla citro2d
static C2D_TextBuf sFameTextBuf;
static C2D_Text sTitleText;
static C2D_Text sCharNames[5];
static C2D_Text sExitText;
static C2D_Text sMessageTexts[4];
static C2D_Text sBackText;
static C2D_Text sCharTitleText;

// Zmienne do Scrollowania w wiadomościach
static float sScrollY = 0.0f;
static int sLastTouchY = -1;

typedef struct {
    int x, y;
    int w, h;
    void (*action)(int id);
    int id;
} FameTouchButton;

static void Action_SelectCharacter(int characterId);
static void Action_BackToSelect(int dummy);
static void Action_PlayMessage(int messageIndex);
static void Action_CloseFameChecker(int dummy);

static FameTouchButton sCharButtons[] = {
    { 20,  40, 80, 80, Action_SelectCharacter, 0 }, // Prof. Oak
    { 120, 40, 80, 80, Action_SelectCharacter, 1 }, // Daisy
    { 220, 40, 80, 80, Action_SelectCharacter, 2 }, // Brock
    { 20, 140, 80, 80, Action_SelectCharacter, 3 }, // Misty
    { 120, 140, 80, 80, Action_SelectCharacter, 4 }, // Lt. Surge
    { 220, 140, 80, 80, Action_CloseFameChecker, 99 } // Wyjście
};

static FameTouchButton sMessageButtons[] = {
    { 15,  40, 290, 40, Action_PlayMessage, 0 },
    { 15,  90, 290, 40, Action_PlayMessage, 1 },
    { 15, 140, 290, 40, Action_PlayMessage, 2 },
    { 15, 190, 290, 40, Action_PlayMessage, 3 },
    { 100, 200, 120, 30, Action_BackToSelect, -1 } // Powrót - Pływający
};

// --- Inicjalizacja i zwalnianie buforów C2D ---
void CtrFameChecker_Init(void) {
    sFameTextBuf = C2D_TextBufNew(2048);
    
    C2D_TextParse(&sTitleText, sFameTextBuf, "FAME CHECKER");
    C2D_TextOptimize(&sTitleText);
    
    C2D_TextParse(&sCharNames[0], sFameTextBuf, "Prof. Oak");
    C2D_TextParse(&sCharNames[1], sFameTextBuf, "Daisy");
    C2D_TextParse(&sCharNames[2], sFameTextBuf, "Brock");
    C2D_TextParse(&sCharNames[3], sFameTextBuf, "Misty");
    C2D_TextParse(&sCharNames[4], sFameTextBuf, "Lt. Surge");
    C2D_TextParse(&sExitText, sFameTextBuf, "EXIT");
    
    for (int i = 0; i < 5; i++) C2D_TextOptimize(&sCharNames[i]);
    C2D_TextOptimize(&sExitText);
    
    C2D_TextParse(&sBackText, sFameTextBuf, "BACK");
    C2D_TextOptimize(&sBackText);
}

void CtrFameChecker_Exit(void) {
    if (sFameTextBuf) {
        C2D_TextBufDelete(sFameTextBuf);
    }
}

static void Action_SelectCharacter(int characterId) {
    sSelectedCharacter = characterId;
    sFameState = FAME_STATE_MESSAGE_VIEW;
    sScrollY = 0.0f;
    sLastTouchY = -1;
    
    C2D_TextBufClear(sFameTextBuf);
    
    // Odbudowa tekstów podstawowych
    C2D_TextParse(&sTitleText, sFameTextBuf, "FAME CHECKER");
    C2D_TextParse(&sBackText, sFameTextBuf, "BACK");
    
    char titleBuffer[64];
    snprintf(titleBuffer, sizeof(titleBuffer), "Person: %d", characterId);
    C2D_TextParse(&sCharTitleText, sFameTextBuf, titleBuffer);
    
    // Generowanie fałszywych ciekawostek (Placeholder dla silnika FR)
    const char* msgs[4] = {
        "Fan Message",
        "Memory",
        "Research Report",
        "Pokemon Secret"
    };
    
    for (int i = 0; i < 4; i++) {
        C2D_TextParse(&sMessageTexts[i], sFameTextBuf, msgs[i]);
        C2D_TextOptimize(&sMessageTexts[i]);
    }
    
    C2D_TextOptimize(&sTitleText);
    C2D_TextOptimize(&sBackText);
    C2D_TextOptimize(&sCharTitleText);
    
    gMain.newKeys |= A_BUTTON; 
}

static void Action_BackToSelect(int dummy) {
    sSelectedCharacter = -1;
    sFameState = FAME_STATE_CHARACTER_SELECT;
    sScrollY = 0.0f;
    
    C2D_TextBufClear(sFameTextBuf);
    CtrFameChecker_Init(); 
    
    gMain.newKeys |= B_BUTTON; 
}

static void Action_PlayMessage(int messageIndex) {
    // Odpalenie animacji wiadomości na górnym ekranie
    gMain.newKeys |= A_BUTTON;
}

static void Action_CloseFameChecker(int dummy) {
    sFameActive = false;
    gMain.newKeys |= START_BUTTON; 
}

void CtrFameChecker_Toggle(void) {
    sFameActive = !sFameActive;
    if (sFameActive) {
        sFameState = FAME_STATE_CHARACTER_SELECT;
        sSelectedCharacter = -1;
        if (!sFameTextBuf) {
            CtrFameChecker_Init();
        }
    }
}

// Rysowanie zaokrąglonego prostokąta
static void DrawRoundedRect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x + 2, y, 0, w - 4, h, color);
    C2D_DrawRectSolid(x, y + 2, 0, w, h - 4, color);
    C2D_DrawCircleSolid(x + 2, y + 2, 0, 2, color);
    C2D_DrawCircleSolid(x + w - 2, y + 2, 0, 2, color);
    C2D_DrawCircleSolid(x + 2, y + h - 2, 0, 2, color);
    C2D_DrawCircleSolid(x + w - 2, y + h - 2, 0, 2, color);
}

void CtrFameChecker_Draw(void)
{
    if (!sFameActive) return;

    C2D_TargetClear(C2D_GetBottom(), COLOR_BG);
    C2D_SceneBegin(C2D_GetBottom());

    // Tytuł i Tło górne
    C2D_DrawRectSolid(0, 0, 0, CTR_BOT_WIDTH, 24, COLOR_BORDER);
    C2D_DrawRectSolid(0, 24, 0, CTR_BOT_WIDTH, 4, COLOR_PANEL);
    
    if (sFameTextBuf) {
        C2D_DrawText(&sTitleText, C2D_WithColor, 100, 4, 0.5f, 0.55f, 0.55f, COLOR_TITLE);
    }

    if (sFameState == FAME_STATE_CHARACTER_SELECT)
    {
        for (int i = 0; i < 6; i++) {
            u32 color = (i == 5) ? C2D_Color32(230, 90, 90, 255) : COLOR_BUTTON;
            u32 text_color = (i == 5) ? COLOR_TITLE : COLOR_TEXT_DARK;
            C2D_Text* txt = (i == 5) ? &sExitText : &sCharNames[i];
            
            // Cień
            DrawRoundedRect(sCharButtons[i].x + 3, sCharButtons[i].y + 3, sCharButtons[i].w, sCharButtons[i].h, COLOR_SHADOW);
            // Obramowanie & Przycisk
            DrawRoundedRect(sCharButtons[i].x - 1, sCharButtons[i].y - 1, sCharButtons[i].w + 2, sCharButtons[i].h + 2, COLOR_BORDER);
            DrawRoundedRect(sCharButtons[i].x, sCharButtons[i].y, sCharButtons[i].w, sCharButtons[i].h, color);
            
            if (sFameTextBuf) {
                // Wyśrodkowany Tekst pod ikoną (Ikony rysowane by były wyżej)
                C2D_DrawText(txt, C2D_WithColor, sCharButtons[i].x + 10, sCharButtons[i].y + 60, 0.5f, 0.5f, 0.5f, text_color);
            }
        }
    }
    else if (sFameState == FAME_STATE_MESSAGE_VIEW)
    {
        // Pasek informacyjny wybranej osoby
        if (sFameTextBuf) {
            C2D_DrawText(&sCharTitleText, C2D_WithColor, 20, 26, 0.5f, 0.5f, 0.5f, COLOR_TEXT_DARK);
        }

        // Przyciski z ciekawostkami
        float currentY = 40.0f + sScrollY;
        for (int i = 0; i < 4; i++) {
            float yPos = currentY + (i * 50);
            
            // Prosty Scissor Test logiczny
            if (yPos > -20 && yPos < 200) {
                DrawRoundedRect(sMessageButtons[i].x + 2, yPos + 2, sMessageButtons[i].w, sMessageButtons[i].h, COLOR_SHADOW);
                DrawRoundedRect(sMessageButtons[i].x, yPos, sMessageButtons[i].w, sMessageButtons[i].h, COLOR_BUTTON);
                
                if (sFameTextBuf) {
                    C2D_DrawText(&sMessageTexts[i], C2D_WithColor, sMessageButtons[i].x + 10, yPos + 10, 0.5f, 0.6f, 0.6f, COLOR_TEXT_DARK);
                }
            }
        }
        
        // Pływający Przycisk "Powrót"
        DrawRoundedRect(sMessageButtons[4].x + 2, sMessageButtons[4].y + 2, sMessageButtons[4].w, sMessageButtons[4].h, COLOR_SHADOW);
        DrawRoundedRect(sMessageButtons[4].x, sMessageButtons[4].y, sMessageButtons[4].w, sMessageButtons[4].h, COLOR_BORDER);
        
        if (sFameTextBuf) {
            C2D_DrawText(&sBackText, C2D_WithColor, sMessageButtons[4].x + 30, sMessageButtons[4].y + 5, 0.5f, 0.6f, 0.6f, COLOR_TITLE);
        }
    }
}

void CtrFameChecker_Touch(touchPosition touch)
{
    if (!sFameActive) return;

    // Obsługa Scrollowania Treści Wiadomości
    if (sFameState == FAME_STATE_MESSAGE_VIEW) {
        if (touch.py >= 40 && touch.py <= 190 && touch.px >= 10 && touch.px <= 310) {
            if (sLastTouchY != -1) {
                float dy = touch.py - sLastTouchY;
                sScrollY += dy;
                if (sScrollY > 0.0f) sScrollY = 0.0f;
                if (sScrollY < -80.0f) sScrollY = -80.0f; // Limit scrollowania
            }
            sLastTouchY = touch.py;
            return;
        } else {
            sLastTouchY = -1;
        }
    } else {
        sLastTouchY = -1;
    }

    // Obsługa Przycisków
    FameTouchButton* buttons = (sFameState == FAME_STATE_CHARACTER_SELECT) ? sCharButtons : sMessageButtons;
    int count = (sFameState == FAME_STATE_CHARACTER_SELECT) ? 6 : 5;

    for (int i = 0; i < count; i++) {
        // Obliczenie aktualnej pozycji przycisku (istotne dla przesuwanego widoku wiadomości)
        float btnY = buttons[i].y;
        if (sFameState == FAME_STATE_MESSAGE_VIEW && i < 4) {
            btnY += sScrollY;
        }
        
        if (touch.px >= buttons[i].x && touch.px <= buttons[i].x + buttons[i].w &&
            touch.py >= btnY && touch.py <= btnY + buttons[i].h) 
        {
            buttons[i].action(buttons[i].id);
            break;
        }
    }
}
