#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

// Deklaracje funkcji z silnika pokefirered
extern void AgbMain(void);
extern void CtrVBlankHook(void);
extern void Init3dsAudio(void);
extern void Exit3dsAudio(void);
extern void Init3dsVideo(void);
extern void Exit3dsVideo(void);
extern void DrawGbaScreenTo3ds(void);

extern void Init3dsBottomUi(void);
extern void Handle3dsBottomUiInput(void);
extern void Draw3dsBottomUi(void);
extern void Exit3dsBottomUi(void);

extern void InitVramArena(void);
extern void FreeVramArena(void);

// Flaga główna działania aplikacji
bool gMainLoopRunning = true;

int main(int argc, char **argv)
{
    // Inicjalizacja usług 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    // Inicjalizacja RomFS dla plików gry (.pak / dane)
    romfsInit();
    
    // Inicjalizacja dźwięku NDSP
    Init3dsAudio();

    // Rezerwacja statycznego obszaru VRAM na grafiki (Ochrona przed fragmentacją)
    InitVramArena();

    // Uruchomienie układu GPU Compositor 3DS
    Init3dsVideo();
    
    // Uruchomienie układu dotykowego ekranu
    Init3dsBottomUi();

    // Możesz tu zainicjować inne rzeczy 3DS (np. wątki Voxel)

    // AgbMain to główna pętla z FireRed.
    // Zostanie ona "uwięziona" tam na zawsze, więc to w jej środku
    // będziemy synchronizować klatki 3DS za pomocą CtrVBlankHook.
    AgbMain();

    // Czyszczenie zasobów w przypadku wyjścia
    FreeVramArena();
    Exit3dsBottomUi();
    Exit3dsVideo();
    Exit3dsAudio();
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}

// Funkcja wywoływana pod koniec każdej pętli w AgbMain()
// Zastępuje oryginalne WaitForVBlank()
void CtrVBlankHook(void)
{
    // Aktualizacja stanu środowiska 3DS
    aptMainLoop();
    hidScanInput();
    
    // Sprawdzenie logiki dotyku na dolnym ekranie
    Handle3dsBottomUiInput();

    u32 kDown = hidKeysDown();
    if (kDown & KEY_START) {
        // Taki prymitywny wyłącznik w razie testów
        // gMainLoopRunning = false; 
    }

    // Renderowanie klatki Citro2D/3D
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    
    // Zlecenie narysowania grafiki z pamięci GBA na wirtualnym GPU PICA200
    DrawGbaScreenTo3ds();
    
    // Wyrysowanie UI dla ekranu dotykowego
    Draw3dsBottomUi();
    
    C2D_Flush();
    C3D_FrameEnd(0);
}
