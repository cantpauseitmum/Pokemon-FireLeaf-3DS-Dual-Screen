#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"

// Ścieżka do pliku na karcie SD konsoli 3DS
#define SAVE_FILE_PATH "sdmc:/3ds/pokefirered/save.sav"
#define FLASH_SIZE 0x20000 // 128 KB dla Pokemon FireRed (Flash 1M)

static u8 sSaveBuffer[FLASH_SIZE];
static bool sSaveLoaded = false;

/**
 * @brief Inicjalizuje pamięć zapisu, ładując plik z karty SD 3DS-a.
 */
void Port_InitFlash(void)
{
    FILE* file = fopen(SAVE_FILE_PATH, "rb");
    if (file)
    {
        fread(sSaveBuffer, 1, FLASH_SIZE, file);
        fclose(file);
    }
    else
    {
        // Jeśli plik nie istnieje (pierwsze uruchomienie), wypełniamy puste miejsce 0xFF
        memset(sSaveBuffer, 0xFF, FLASH_SIZE);
        
        // Utworzenie folderów i pliku na SD
        mkdir("sdmc:/3ds/pokefirered", 0777);
        file = fopen(SAVE_FILE_PATH, "wb");
        if (file) {
            fwrite(sSaveBuffer, 1, FLASH_SIZE, file);
            fclose(file);
        }
    }
    sSaveLoaded = true;
}

/**
 * @brief Przechwytuje odczyt pamięci (zastępuje ReadFlash z agb_flash.c)
 */
void Port_ReadFlash(u16 sectorNum, u32 offset, void *dest, u32 size)
{
    if (!sSaveLoaded) Port_InitFlash();
    
    // Obliczamy fizyczny adres w buforze SD
    u32 absoluteAddress = (sectorNum * 0x1000) + offset; // 0x1000 = Rozmiar Sektora 4KB
    memcpy(dest, &sSaveBuffer[absoluteAddress], size);
}

/**
 * @brief Przechwytuje zapis pamięci i zapisuje zawartość na kartę SD (Zastępuje WriteFlash)
 */
u16 Port_WriteFlash(u16 sectorNum, const void *src, u32 size)
{
    if (!sSaveLoaded) Port_InitFlash();

    u32 absoluteAddress = sectorNum * 0x1000;
    memcpy(&sSaveBuffer[absoluteAddress], src, size);

    // Zrzucenie (Flush) danych na fizyczną kartę pamięci SD
    FILE* file = fopen(SAVE_FILE_PATH, "r+b");
    if (file)
    {
        fseek(file, absoluteAddress, SEEK_SET);
        fwrite(src, 1, size, file);
        fclose(file);
        return 0; // Sukces
    }
    return 1; // Błąd zapisu I/O
}

/**
 * @brief Pobiera aktualny czas z wbudowanego zegara 3DS-a.
 * Pokemon FireRed nie obsługiwał RTC na kartridżach. My przywracamy czas z OS-u.
 */
void Port_GetTime(s32* hours, s32* minutes, s32* seconds)
{
    u64 timeInSeconds = osGetTime() / 1000; // Czas w MS od 1 Stycznia 1900
    
    // Obliczanie wartości dla gry GBA
    *seconds = timeInSeconds % 60;
    *minutes = (timeInSeconds / 60) % 60;
    *hours = (timeInSeconds / 3600) % 24;
}
