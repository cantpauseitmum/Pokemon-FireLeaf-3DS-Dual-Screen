#include <stdio.h>
#include <string.h>
#include <3ds.h>
#include "gba/flash_internal.h"

// Rozmiar zapisu FireRed to 128KB
#define FLASH_SIZE 0x20000
#define SECTOR_SIZE 0x1000
#define SAVE_PATH "sdmc:/3ds/pokefirered/pokefirered.sav"

// Wirtualny Flash RAM w pamięci 3DS-a
static u8 sVirtualFlash[FLASH_SIZE];
static bool sSaveInitialized = false;

static void EnsureSaveFileExists(void)
{
    if (sSaveInitialized) return;
    sSaveInitialized = true;

    FILE *f = fopen(SAVE_PATH, "rb");
    if (f)
    {
        fread(sVirtualFlash, 1, FLASH_SIZE, f);
        fclose(f);
    }
    else
    {
        memset(sVirtualFlash, 0xFF, FLASH_SIZE);
        f = fopen(SAVE_PATH, "wb");
        if (f) {
            fwrite(sVirtualFlash, 1, FLASH_SIZE, f);
            fclose(f);
        }
    }
}

static void FlushSectorToSD(u16 sectorNum)
{
    FILE *f = fopen(SAVE_PATH, "rb+");
    if (f) {
        fseek(f, sectorNum * SECTOR_SIZE, SEEK_SET);
        fwrite(&sVirtualFlash[sectorNum * SECTOR_SIZE], 1, SECTOR_SIZE, f);
        fclose(f);
    }
}

#ifdef PLATFORM_3DS

// Zastępuje oryginalne odczyty z chipu Flash GBA
u16 ReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size)
{
    EnsureSaveFileExists();
    memcpy(dest, &sVirtualFlash[(sectorNum * SECTOR_SIZE) + offset], size);
    return 0;
}

u16 ProgramFlashSectorAndVerify(u16 sectorNum, void *src)
{
    EnsureSaveFileExists();
    memcpy(&sVirtualFlash[sectorNum * SECTOR_SIZE], src, SECTOR_SIZE);
    FlushSectorToSD(sectorNum);
    return 0;
}

u16 ProgramFlashByte(u16 sectorNum, u32 offset, u8 data)
{
    EnsureSaveFileExists();
    sVirtualFlash[(sectorNum * SECTOR_SIZE) + offset] = data;
    FlushSectorToSD(sectorNum); // Nieoptymalne dla zapisu bajt-po-bajcie, ale zgodne funkcjonalnie
    return 0;
}

u16 EraseFlashSector(u16 sectorNum)
{
    EnsureSaveFileExists();
    memset(&sVirtualFlash[sectorNum * SECTOR_SIZE], 0xFF, SECTOR_SIZE);
    FlushSectorToSD(sectorNum);
    return 0;
}

u16 IdentifyFlash(void)
{
    EnsureSaveFileExists();
    return 0; // Succes
}

void CheckForFlashMemory(void)
{
    // Oryginalnie gra zawieszała się tutaj, jeżeli nie wykryto kości 128K.
    // Na 3DS-ie po prostu ignorujemy sprzęt.
}

#endif
