#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include "global.h"
#include "gba/gba.h"

#define CTR_BOT_WIDTH  320
#define CTR_BOT_HEIGHT 240

// Kolory przewodnie Town Map w Kanto (FireRed)
#define COLOR_MAP_BG        C2D_Color32(112, 192, 160, 255) // Zielonkawe tło
#define COLOR_MAP_BORDER    C2D_Color32(248, 112, 112, 255) // Czerwone obramowania Kanto
#define COLOR_MAP_TEXTBOX   C2D_Color32(248, 248, 248, 255) // Biały panel tekstu
#define COLOR_CURSOR        C2D_Color32(248, 216, 112, 255) // Żółty, mrugający kursor z gry

static bool sMapActive = false;
static int sCursorGridX = 0;
static int sCursorGridY = 0;
static int sFrameCounter = 0; // Do animacji mrugania kursora

// Struktura na klikalne lokacje na Mapie (Siatka X/Y z oryginalnej gry)
typedef struct {
    int gridX, gridY;
    int pixelX, pixelY;
    const char* internalName;
} KantoCity;

// Zredukowana siatka głównych miast Kanto (w GBA siatka miała rozmiar 28x15, skalujemy na 320x240)
static KantoCity sKantoCities[] = {
    { 2, 11,  40, 180, "PALLET TOWN" },
    { 2,  8,  40, 130, "VIRIDIAN CITY" },
    { 2,  3,  40,  50, "PEWTER CITY" },
    { 10, 2, 160,  30, "CERULEAN CITY" },
    { 14, 2, 220,  30, "LAVENDER TOWN" }, // Przykład
    { 10, 5, 160,  80, "SAFFRON CITY" },
    { 10, 9, 160, 140, "VERMILION CITY" },
    { 6,  5, 100,  80, "CELADON CITY" },
    { 6, 11, 100, 180, "FUCHSIA CITY" },
    { 2, 13,  40, 210, "CINNABAR ISLAND" }
};
static int sNumCities = sizeof(sKantoCities) / sizeof(KantoCity);

/**
 * @brief Przełącza stan wyświetlania dotykowej Mapy Kanto
 */
void CtrKantoMap_Toggle(bool state) {
    sMapActive = state;
    if (state) {
        // Domyślnie kursor ustawiony na Pallet Town
        sCursorGridX = 2;
        sCursorGridY = 11;
        sFrameCounter = 0;
    }
}

/**
 * @brief Rysuje Mapę Regionu na dolnym ekranie przy użyciu C2D
 */
void CtrKantoMap_Draw(void)
{
    if (!sMapActive) return;

    C2D_TargetClear(C2D_GetBottom(), COLOR_MAP_BG);
    C2D_SceneBegin(C2D_GetBottom());

    // 1. Zarys samej lądu Kanto (Tymczasowe, normalnie użyjemy C2D_DrawImage z teksturą Mapy)
    C2D_DrawRectSolid(20, 20, 0, 280, 200, C2D_Color32(144, 216, 112, 255)); // Kontynent
    C2D_DrawRectSolid(10, 190, 0, 290, 40, C2D_Color32(80, 136, 200, 255)); // Morze (np. Cinnabar)

    // 2. Rysowanie punktów miast (Kwadraciki jak w oryginalnym GBA)
    for (int i = 0; i < sNumCities; i++) {
        C2D_DrawRectSolid(sKantoCities[i].pixelX, sKantoCities[i].pixelY, 0, 
                          12, 12, COLOR_MAP_BORDER);
    }

    // 3. Rysowanie mrugającego kursora na wybranym mieście
    sFrameCounter++;
    if (sFrameCounter % 60 < 30) // Kursor mruga co pół sekundy
    {
        for (int i = 0; i < sNumCities; i++) {
            if (sKantoCities[i].gridX == sCursorGridX && sKantoCities[i].gridY == sCursorGridY) {
                // Rysujemy powiększony żółty kwadrat
                C2D_DrawRectSolid(sKantoCities[i].pixelX - 2, sKantoCities[i].pixelY - 2, 0, 
                                  16, 16, COLOR_CURSOR);
                break;
            }
        }
    }

    // 4. Dolny / Górny panel z nazwą zaznaczonej lokacji (Jak w oryginalnym Town Map)
    C2D_DrawRectSolid(0, 0, 0, CTR_BOT_WIDTH, 20, COLOR_MAP_BORDER);
    C2D_DrawRectSolid(0, 2, 0, CTR_BOT_WIDTH, 16, COLOR_MAP_TEXTBOX);
    // W pełnej implementacji: C2D_DrawText(..., sKantoCities[wybrany].internalName, ...);
}

/**
 * @brief Przechwycenie uderzenia rysika by teleportować kursor Mapy
 */
void CtrKantoMap_Touch(touchPosition touch)
{
    if (!sMapActive) return;

    // Przeszukiwanie czy gracz kliknął w pobliżu jakiegoś miasta (Hitbox 20x20)
    for (int i = 0; i < sNumCities; i++) {
        if (touch.px >= sKantoCities[i].pixelX - 10 && touch.px <= sKantoCities[i].pixelX + 22 &&
            touch.py >= sKantoCities[i].pixelY - 10 && touch.py <= sKantoCities[i].pixelY + 22) 
        {
            // Zmiana pozycji
            if (sCursorGridX != sKantoCities[i].gridX || sCursorGridY != sKantoCities[i].gridY) {
                sCursorGridX = sKantoCities[i].gridX;
                sCursorGridY = sKantoCities[i].gridY;
                
                // Tutaj wstrzykniemy współrzędne bezposrednio do gMapCursor z oryginalnej pamięci RAM
                // gRegionMapCursorX = sCursorGridX;
                // gRegionMapCursorY = sCursorGridY;
                
                gMain.newKeys |= A_BUTTON; // Symulacja odświeżenia okna/Lotu (FLY)
            }
            break;
        }
    }
}
