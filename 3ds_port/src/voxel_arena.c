#include <3ds.h>
#include <stdio.h>
#include "global.h"

// ZallaxDev zaznaczył w dokumentacji ARCHITECTURE.md:
// "VRAM is claimed as fixed arenas at start-up (voxel_arena.c) 
//  because variable-size allocation fragmented it."

// VRAM na 3DS-ie ma 6MB (dzielone zazwyczaj na 2 banki po 3MB dla ekranu górnego i dolnego).
#define VRAM_ARENA_SIZE (1024 * 1024 * 2) // Rezerwujemy 2MB na sztywno
static u8* sVramArenaBase = NULL;
static u32 sVramArenaOffset = 0;

void InitVramArena(void)
{
    // Alokujemy cały ciągły blok pamięci VRAM na starcie za jednym zamachem
    // Zapobiega to fragmentacji (tzw. "dziurawieniu" pamięci), 
    // co mogłoby spowodować crashe przy częstym ładowaniu map/walk
    sVramArenaBase = vramAlloc(VRAM_ARENA_SIZE);
    sVramArenaOffset = 0;
}

void* VramArenaAlloc(u32 size)
{
    if (!sVramArenaBase) return NULL;
    
    // Zrównanie (alignment) do 8 bajtów, optymalne dla operacji GPU 3DSa
    size = (size + 7) & ~7;
    
    if (sVramArenaOffset + size > VRAM_ARENA_SIZE)
    {
        // Przepełnienie areny! W docelowej grze powinno się tu rzucić błąd
        return NULL;
    }
    
    void* ptr = sVramArenaBase + sVramArenaOffset;
    sVramArenaOffset += size;
    return ptr;
}

void ResetVramArena(void)
{
    // Czysty reset offsetu - bez uwalniania pamięci fizycznej.
    // Gwarantuje to idealną wydajność i brak przecieków pamięci.
    sVramArenaOffset = 0;
}

void FreeVramArena(void)
{
    if (sVramArenaBase)
    {
        vramFree(sVramArenaBase);
        sVramArenaBase = NULL;
        sVramArenaOffset = 0;
    }
}
