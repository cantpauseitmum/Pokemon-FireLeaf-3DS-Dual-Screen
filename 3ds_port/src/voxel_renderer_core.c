#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include "global.h"

// Wskaźniki na zbuforowane tekstury 3D (Zamiast 2D BGMaps z GBA)
extern C3D_Tex gVoxelAtlas;
extern C3D_Mtx gCameraMatrix;
extern C3D_Mtx gProjectionMatrix;

// Struktura pojedynczego wierzchołka (Vertex) przekazywanego do PICA200
typedef struct {
    float x, y, z;    // Pozycja w przestrzeni 3D
    float u, v;       // Współrzędne tekstury (UV) na Atlasie
    u32 color;        // Cieniowanie wierzchołków (Vertex Lighting)
} VoxelVertex;

// Globalny bufor VBO (Vertex Buffer Object) dla Voxelowej Mapy
static VoxelVertex* sVboBuffer = NULL;
static int sVboVertexCount = 0;

/**
 * @brief Główna funkcja ładująca potok graficzny i wywołująca sprzętowy rendering PICA200.
 * Wycinamy stary kod 2D (overworld.c) i każemy Citro3D narysować świat 3D.
 */
void RenderVoxelMap(void)
{
    if (sVboVertexCount == 0 || sVboBuffer == NULL)
        return; // Brak siatki do narysowania (np. menu)

    // 1. Z-Culling (Głębia) i Backface Culling (Optymalizacja niewidocznych krawędzi)
    // Bez tego PICA200 dławiłaby się, próbując narysować tył każdego budynku
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);

    // 2. Ładowanie Macierzy (Model-View-Projection)
    // Przeliczamy kąt patrzenia (Kamera) i perspektywę (Projekcja) na język układu graficznego
    C3D_Mtx mvp;
    Mtx_Copy(&mvp, &gProjectionMatrix);
    Mtx_Multiply(&mvp, &mvp, &gCameraMatrix);
    
    // Przekazanie przetworzonej macierzy do Shaderów Geometrii (Vertex Shader)
    // ZallaxDev używa specjalnego pliku `voxel.v.pica` jako własnego shadera.
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, 0, &mvp);

    // 3. Konfiguracja Tekstur (Texture Environment)
    // Wiążemy nasz wielki zbiór tekstur Pokemon (Atlas) z układem 3DS-a
    C3D_TexBind(0, &gVoxelAtlas);
    
    // Ustawienie oświetlenia stałego: PICA200 pomnoży kolor tekstury przez kolor (cień) wierzchołka
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
    C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

    // 4. Bindowanie Atrybutów Wierzchołków
    // Mówimy karcie graficznej: "Pierwsze 3 floaty to Pozycja, kolejne 2 to Tekstura, a potem 4 bajty Koloru"
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // XYZ
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // UV
    AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4); // RGBA (Lighting)

    // Wysłanie bufora z RAM do VRAM układu PICA200
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, sVboBuffer, sizeof(VoxelVertex), 3, 0x210);

    // 5. Właściwy Rendering!
    // Karta graficzna bierze VBO i zamienia go w piksele z akceleracją sprzętową.
    C3D_DrawArrays(GPU_TRIANGLES, 0, sVboVertexCount);
}
