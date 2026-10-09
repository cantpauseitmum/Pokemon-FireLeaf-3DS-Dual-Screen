#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
#include "gba/gba.h"

#define CTR_BOT_WIDTH  320
#define CTR_BOT_HEIGHT 240

// --- Kolorystyka FireRed Help System ---
#define COLOR_HELP_BG       C2D_Color32(240, 248, 248, 255)
#define COLOR_HELP_PANEL    C2D_Color32(112, 200, 248, 255)
#define COLOR_HELP_BUTTON   C2D_Color32(255, 255, 255, 255)
#define COLOR_HELP_BORDER   C2D_Color32(48, 120, 200, 255)
#define COLOR_TEXT_DARK     C2D_Color32(40, 40, 40, 255)
#define COLOR_TEXT_TITLE    C2D_Color32(255, 255, 255, 255)
#define COLOR_SHADOW        C2D_Color32(0, 0, 0, 80)

enum HelpSystemState {
    HELP_STATE_MAIN_MENU,
    HELP_STATE_CONTENT_VIEW
};

static int sHelpState = HELP_STATE_MAIN_MENU;
static int sSelectedTopic = -1;
static bool sHelpActive = false;

// Bufory tekstu dla citro2d
static C2D_TextBuf sHelpTextBuf;
static C2D_Text sTitleText;
static C2D_Text sMenuTexts[4];
static C2D_Text sContentText;
static C2D_Text sBackText;

// Zmienne do Scrollowania
static float sScrollY = 0.0f;
static float sMaxScrollY = 0.0f;
static int sLastTouchY = -1;

typedef struct {
    int x, y;
    int w, h;
    void (*action)(int id);
    int id;
} HelpButton;

static void Action_SelectTopic(int topicId);
static void Action_BackToMenu(int id);
static void Action_CloseHelp(int id);

static HelpButton sMenuButtons[] = {
    { 20,  40,  280, 40, Action_SelectTopic, 0 },
    { 20,  90,  280, 40, Action_SelectTopic, 1 },
    { 20, 140,  280, 40, Action_SelectTopic, 2 },
    { 20, 190,  280, 40, Action_CloseHelp,  -1 }
};

static HelpButton sContentButtons[] = {
    { 20, 190, 280, 40, Action_BackToMenu, -1 }
};

// --- Inicjalizacja i zwalnianie buforów C2D ---
void CtrHelp_Init(void) {
    sHelpTextBuf = C2D_TextBufNew(1024);
    
    C2D_TextParse(&sTitleText, sHelpTextBuf, "HELP SYSTEM");
    C2D_TextOptimize(&sTitleText);
    
    C2D_TextParse(&sMenuTexts[0], sHelpTextBuf, "How to Move");
    C2D_TextParse(&sMenuTexts[1], sHelpTextBuf, "Pokemon Battles");
    C2D_TextParse(&sMenuTexts[2], sHelpTextBuf, "About the Menu");
    C2D_TextParse(&sMenuTexts[3], sHelpTextBuf, "Close Help");
    
    for (int i = 0; i < 4; i++) {
        C2D_TextOptimize(&sMenuTexts[i]);
    }
    
    C2D_TextParse(&sBackText, sHelpTextBuf, "BACK");
    C2D_TextOptimize(&sBackText);
}

void CtrHelp_Exit(void) {
    if (sHelpTextBuf) {
        C2D_TextBufDelete(sHelpTextBuf);
    }
}

static void Action_SelectTopic(int topicId) {
    sSelectedTopic = topicId;
    sHelpState = HELP_STATE_CONTENT_VIEW;
    sScrollY = 0.0f;
    sLastTouchY = -1;
    
    C2D_TextBufClear(sHelpTextBuf);
    
    // Odbudowa tekstów podstawowych
    C2D_TextParse(&sTitleText, sHelpTextBuf, "HELP SYSTEM");
    C2D_TextParse(&sBackText, sHelpTextBuf, "BACK");
    
    const char* content = "";
    if (topicId == 0) {
        content = "Movement in Kanto:\nUse D-Pad to move.\nHold B to run\n(if you have Running Shoes).\nPress A to interact.";
    } else if (topicId == 1) {
        content = "Battle:\nWhen you meet a wild Pokemon\nor Trainer, a battle starts.\nChoose 'FIGHT' to attack.\nChoose 'BAG' for items.\nTo catch a wild Pokemon,\nuse a Poke Ball!";
    } else if (topicId == 2) {
        content = "Player Menu:\nPress START at any time\nto open the menu.\nYou will find Pokemon, Bag,\nTrainer Card, and Save.";
    }
    
    C2D_TextParse(&sContentText, sHelpTextBuf, content);
    C2D_TextOptimize(&sContentText);
    C2D_TextOptimize(&sTitleText);
    C2D_TextOptimize(&sBackText);
    
    // Wysyłamy sygnał A_BUTTON do silnika GBA (aby zachować iluzję działania natywnego)
    gMain.newKeys |= A_BUTTON; 
}

static void Action_BackToMenu(int id) {
    sSelectedTopic = -1;
    sHelpState = HELP_STATE_MAIN_MENU;
    sScrollY = 0.0f;
    
    C2D_TextBufClear(sHelpTextBuf);
    CtrHelp_Init(); // Reinicjalizacja tekstów menu
    
    gMain.newKeys |= B_BUTTON; 
}

static void Action_CloseHelp(int id) {
    sHelpActive = false;
    gMain.newKeys |= L_BUTTON; 
}

void CtrHelp_Toggle(void) {
    sHelpActive = !sHelpActive;
    if (sHelpActive) {
        sHelpState = HELP_STATE_MAIN_MENU;
        sSelectedTopic = -1;
        if (!sHelpTextBuf) {
            CtrHelp_Init();
        }
    }
}

// Rysowanie zaokrąglonego prostokąta (iluzja poprzez nakładające się kształty)
static void DrawRoundedRect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x + 2, y, 0, w - 4, h, color);
    C2D_DrawRectSolid(x, y + 2, 0, w, h - 4, color);
    C2D_DrawCircleSolid(x + 2, y + 2, 0, 2, color);
    C2D_DrawCircleSolid(x + w - 2, y + 2, 0, 2, color);
    C2D_DrawCircleSolid(x + 2, y + h - 2, 0, 2, color);
    C2D_DrawCircleSolid(x + w - 2, y + h - 2, 0, 2, color);
}

void CtrHelp_Draw(void)
{
    if (!sHelpActive) return;

    C2D_TargetClear(C2D_GetBottom(), COLOR_HELP_BG);
    C2D_SceneBegin(C2D_GetBottom());

    // Header gradient illusion (Dwa prostokąty)
    C2D_DrawRectSolid(0, 0, 0, CTR_BOT_WIDTH, 24, COLOR_HELP_BORDER);
    C2D_DrawRectSolid(0, 24, 0, CTR_BOT_WIDTH, 4, COLOR_HELP_PANEL);
    
    if (sHelpTextBuf) {
        C2D_DrawText(&sTitleText, C2D_WithColor, 60, 4, 0.5f, 0.55f, 0.55f, COLOR_TEXT_TITLE);
    }

    if (sHelpState == HELP_STATE_MAIN_MENU)
    {
        for (int i = 0; i < 4; i++) {
            u32 color = (i == 3) ? C2D_Color32(230, 90, 90, 255) : COLOR_HELP_BUTTON;
            u32 text_color = (i == 3) ? COLOR_TEXT_TITLE : COLOR_TEXT_DARK;
            
            // Cień
            DrawRoundedRect(sMenuButtons[i].x + 2, sMenuButtons[i].y + 2, sMenuButtons[i].w, sMenuButtons[i].h, COLOR_SHADOW);
            // Przycisk
            DrawRoundedRect(sMenuButtons[i].x, sMenuButtons[i].y, sMenuButtons[i].w, sMenuButtons[i].h, color);
            
            if (sHelpTextBuf) {
                // Render Tekstu
                C2D_DrawText(&sMenuTexts[i], C2D_WithColor, sMenuButtons[i].x + 20, sMenuButtons[i].y + 10, 0.5f, 0.6f, 0.6f, text_color);
            }
        }
    }
    else if (sHelpState == HELP_STATE_CONTENT_VIEW)
    {
        // Okno treści
        DrawRoundedRect(15, 35, 290, 145, COLOR_SHADOW);
        DrawRoundedRect(12, 32, 296, 151, COLOR_HELP_PANEL);
        DrawRoundedRect(15, 35, 290, 145, COLOR_HELP_BUTTON);
        
        if (sHelpTextBuf) {
            // Render Tekstu Treści (ze wsparciem scrollowania)
            float textY = 45.0f + sScrollY;
            // Prosty system przycinania wymaga marginesów bezpiecznych
            C2D_DrawText(&sContentText, C2D_WithColor, 25, textY, 0.5f, 0.55f, 0.55f, COLOR_TEXT_DARK);
        }
        
        // Przycisk powrotu
        DrawRoundedRect(sContentButtons[0].x + 2, sContentButtons[0].y + 2, sContentButtons[0].w, sContentButtons[0].h, COLOR_SHADOW);
        DrawRoundedRect(sContentButtons[0].x, sContentButtons[0].y, sContentButtons[0].w, sContentButtons[0].h, COLOR_HELP_BORDER);
        
        if (sHelpTextBuf) {
            C2D_DrawText(&sBackText, C2D_WithColor, sContentButtons[0].x + 110, sContentButtons[0].y + 10, 0.5f, 0.6f, 0.6f, COLOR_TEXT_TITLE);
        }
    }
}

void CtrHelp_Touch(touchPosition touch)
{
    if (!sHelpActive) return;

    // Obsługa Scrollowania Treści
    if (sHelpState == HELP_STATE_CONTENT_VIEW) {
        if (touch.py >= 35 && touch.py <= 180 && touch.px >= 15 && touch.px <= 305) {
            if (sLastTouchY != -1) {
                float dy = touch.py - sLastTouchY;
                sScrollY += dy;
                if (sScrollY > 0.0f) sScrollY = 0.0f;
                if (sScrollY < -200.0f) sScrollY = -200.0f; // Limit scrollowania
            }
            sLastTouchY = touch.py;
            return;
        } else {
            sLastTouchY = -1;
        }
    } else {
        sLastTouchY = -1;
    }

    // Obsługa Przycisków (Tylko puszczenie / nowy dotyk)
    HelpButton* buttons = (sHelpState == HELP_STATE_MAIN_MENU) ? sMenuButtons : sContentButtons;
    int count = (sHelpState == HELP_STATE_MAIN_MENU) ? 4 : 1;

    for (int i = 0; i < count; i++) {
        if (touch.px >= buttons[i].x && touch.px <= buttons[i].x + buttons[i].w &&
            touch.py >= buttons[i].y && touch.py <= buttons[i].y + buttons[i].h) 
        {
            buttons[i].action(buttons[i].id);
            break;
        }
    }
}
