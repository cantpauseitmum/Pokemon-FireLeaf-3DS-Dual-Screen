#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <3ds.h>
#include "gba/flash_internal.h"

// Rozmiar zapisu FireRed to 128KB
#define FLASH_SIZE 0x20000
#define SECTOR_SIZE 0x1000
#define SAVE_PATH "sdmc:/3ds/pokefirered/pokefirered.sav"

// Wirtualny Flash RAM w pamieci 3DS-a
static u8 sVirtualFlash[FLASH_SIZE];
static bool sSaveInitialized = false;

u16 (*ProgramFlashByte)(u16, u32, u8);
u16 (*EraseFlashSector)(u16);

unsigned char g_PortLogActive = 1;
void Port_Log_Printf(const char *fmt, ...) { }
void CtrAssets_Fatal(const char *reason) { }

static void EnsureSaveFileExists(void)
{
    if (sSaveInitialized) return;
    sSaveInitialized = true;

    extern u16 CtrProgramFlashByte(u16, u32, u8);
    extern u16 CtrEraseFlashSector(u16);
    ProgramFlashByte = CtrProgramFlashByte;
    EraseFlashSector = CtrEraseFlashSector;

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/pokefirered", 0777);

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

// Zastepuje oryginalne odczyty z chipu Flash GBA
void ReadFlash(u16 sectorNum, u32 offset, void *dest, u32 size)
{
    EnsureSaveFileExists();
    memcpy(dest, &sVirtualFlash[(sectorNum * SECTOR_SIZE) + offset], size);
}

u32 ProgramFlashSectorAndVerify(u16 sectorNum, u8 *src)
{
    EnsureSaveFileExists();
    memcpy(&sVirtualFlash[sectorNum * SECTOR_SIZE], src, SECTOR_SIZE);
    FlushSectorToSD(sectorNum);
    return 0;
}

u16 CtrProgramFlashByte(u16 sectorNum, u32 offset, u8 data)
{
    EnsureSaveFileExists();
    sVirtualFlash[(sectorNum * SECTOR_SIZE) + offset] = data;
    FlushSectorToSD(sectorNum);
    return 0;
}

u16 CtrEraseFlashSector(u16 sectorNum)
{
    EnsureSaveFileExists();
    memset(&sVirtualFlash[sectorNum * SECTOR_SIZE], 0xFF, SECTOR_SIZE);
    FlushSectorToSD(sectorNum);
    return 0;
}

u16 IdentifyFlash(void)
{
    EnsureSaveFileExists();
    return 0; // Success
}

void CheckForFlashMemory(void)
{
}

#endif

void CB2_InitBerryFixProgram(void) { }
