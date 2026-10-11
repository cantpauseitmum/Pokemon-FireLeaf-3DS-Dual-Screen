#include "global.h"
#include <stdio.h>
#include "gba/isagbprint.h"

#ifndef DBGPRINTF
#define DBGPRINTF printf
#endif
#ifdef PORT_BRIDGE
#include "port_platform.h"
#endif
// BIOS function implementations are based on the VBA-M source code.

//memory defines here because there's no better spot for them
u16 INTR_CHECK;
void *INTR_VECTOR;
unsigned char REG_BASE[0x400] __attribute__ ((aligned (4)));
unsigned char PLTT[PLTT_SIZE] __attribute__ ((aligned (4)));
unsigned char VRAM_[VRAM_SIZE] __attribute__ ((aligned (4)));
unsigned char OAM[OAM_SIZE] __attribute__ ((aligned (4)));
unsigned char FLASH_BASE[131072] __attribute__ ((aligned (4)));
extern struct SoundInfo gSoundInfo;
struct SoundInfo *SOUND_INFO_PTR = &gSoundInfo;

#ifdef PORT_BRIDGE
void *malloc(size_t size);
void free(void *ptr);

#define PORT_LZ77_HEADER 0x10

static bool32 Port_IsValidLz77Header(const void *src)
{
    const u8 *bytes = src;
    u32 decompressedSize;

    if (bytes == NULL)
        return FALSE;

    if (bytes[0] != PORT_LZ77_HEADER)
        return FALSE;

    decompressedSize = (bytes[3] << 16) | (bytes[2] << 8) | bytes[1];
    return decompressedSize != 0;
}

/*
 * A source that is not LZ77: what it was before resolving (a game symbol, an
 * asset stub plus an offset), the asset that names, and who called, so that
 * the faulty call can be found (arm-none-eabi-addr2line on the caller).
 */
static bool32 Port_RejectInvalidLz77Source(const void *src, const void *original, const void *caller,
                                           const char *tag)
{
    const char *asset;

    if (Port_IsValidLz77Header(src))
        return FALSE;
    /* A header of size zero is nothing to decompress on the GBA too: the
     * game's DecompressTrainerBackPic passes the uncompressed back pictures
     * every battle, and the BIOS writes nothing. Not worth a line. */
    if (src != NULL && ((const u8 *)src)[1] == 0 && ((const u8 *)src)[2] == 0 && ((const u8 *)src)[3] == 0)
        return TRUE;
    asset = original != NULL ? Port_GetAssetPath(original) : NULL;
    printf("[PORT][%s] bad ptr %02x @%08lx (given %08lx, asset %s, caller %08lx)\n", tag,
           src != NULL ? *(const u8 *)src : 0, (unsigned long)src, (unsigned long)original,
           asset != NULL ? asset : "-", (unsigned long)caller);
    return TRUE;
}
#endif

static uint32_t CPUReadMemory(const void *src)
{
    return *(uint32_t *)src;
}

static void CPUWriteMemory(void *dest, uint32_t val)
{
    *(uint32_t *)dest = val;
}

static uint16_t CPUReadHalfWord(const void *src)
{
    return *(uint16_t *)src;
}

static void CPUWriteHalfWord(void *dest, uint16_t val)
{
    *(uint16_t *)dest = val;
}

static inline uint8_t CPUReadByte(const void *src)
{
    return *(uint8_t *)src;
}

static inline void CPUWriteByte(void *dest, uint8_t val)
{
    *(uint8_t *)dest = val;
}

/*
 * The BIOS copies one unit at a time from the start, so a copy onto a
 * destination just ahead of its own source repeats the first units: that one
 * case keeps the unit loop. Every other copy is memcpy, and a fill of one
 * repeated byte (zero, nearly always) is memset - the ARM11 moves those a
 * cache line at a time instead of a halfword per loop turn.
 */
static bool32 CopyRepeats(const u8 *source, const u8 *dest, u32 bytes)
{
    return dest > source && dest < source + bytes;
}

static void Fill32(u8 *dest, u32 value, u32 count)
{
    if (((value >> 8) | ((value & 0xFF) << 24)) == value)
    {
        memset(dest, value & 0xFF, count * 4);
        return;
    }
    if (((uintptr_t)dest & 3) == 0)
    {
        u32 *d = (u32 *)dest;

        while (count >= 4)
        {
            d[0] = value; d[1] = value; d[2] = value; d[3] = value;
            d += 4;
            count -= 4;
        }
        while (count--)
            *d++ = value;
        return;
    }
    while (count--)
    {
        CPUWriteMemory(dest, value);
        dest += 4;
    }
}

void CpuSet(const void *src, void *dst, u32 cnt)
{
    if(dst == NULL)
    {
        puts("Attempted to CpuSet to NULL\n");
        return;
    }
    
    u32 count = cnt & 0x1FFFFF;

    const u8 *source = src;
    u8 *dest = dst;

#ifdef PORT_BRIDGE
    if (!((cnt >> 24) & 1))
    {
        u32 maxBytes = ((cnt >> 26) & 1) ? ((cnt & 0x1FFFFF) * 4) : ((cnt & 0x1FFFFF) * 2);
        if (Port_LoadAssetPointerToBufferSized(src, dst, maxBytes)
         || Port_LoadAssetPointerToBuffer(src, dst, maxBytes))
            return;
    }
#endif

    // 32-bit ?
    if ((cnt >> 26) & 1) {
        // fill ?
        if ((cnt >> 24) & 1) {
            Fill32(dest, CPUReadMemory(source), count);
        } else if (!CopyRepeats(source, dest, count * 4)) {
            memcpy(dest, source, count * 4);
        } else {
            // copy
            while (count) {
                CPUWriteMemory(dest, CPUReadMemory(source));
                source += 4;
                dest += 4;
                count--;
            }
        }
    } else {
        // 16-bit fill?
        if ((cnt >> 24) & 1) {
            uint16_t value = CPUReadHalfWord(source);

            if ((value >> 8) == (value & 0xFF)) {
                memset(dest, value & 0xFF, count * 2);
                return;
            }
            if (((uintptr_t)dest & 2) && count) {
                CPUWriteHalfWord(dest, value);
                dest += 2;
                count--;
            }
            if (((uintptr_t)dest & 1) == 0) {
                Fill32(dest, value | ((u32)value << 16), count / 2);
                dest += (count & ~1u) * 2;
                count &= 1;
            }
            while (count) {
                CPUWriteHalfWord(dest, value);
                dest += 2;
                count--;
            }
        } else if (!CopyRepeats(source, dest, count * 2)) {
            memcpy(dest, source, count * 2);
        } else {
            // copy
            while (count) {
                CPUWriteHalfWord(dest, CPUReadHalfWord(source));
                source += 2;
                dest += 2;
                count--;
            }
        }
    }
}

void CpuFastSet(const void *src, void *dst, u32 cnt)
{
    if(dst == NULL)
    {
        puts("Attempted to CpuFastSet to NULL\n");
        return;
    }
    
    int count = cnt & 0x1FFFFF;

    const u8 *source = src;
    u8 *dest = dst;

#ifdef PORT_BRIDGE
    if (!((cnt >> 24) & 1))
    {
        u32 maxBytes = (cnt & 0x1FFFFF) * 4;
        if (Port_LoadAssetPointerToBufferSized(src, dst, maxBytes)
         || Port_LoadAssetPointerToBuffer(src, dst, maxBytes))
            return;
    }
#endif

    // BIOS always transfers 32 bytes at a time
    u32 words = (count + 7) & ~7;

    if (count <= 0)
        return;
    // fill?
    if((cnt >> 24) & 1) {
        Fill32(dest, CPUReadMemory(source), words);
    } else if (!CopyRepeats(source, dest, words * 4)) {
        memcpy(dest, source, words * 4);
    } else {
        // copy
        while(count > 0) {
            for(int i = 0; i < 8; i++) {
                uint32_t value = CPUReadMemory(source);
                CPUWriteMemory(dest, value);
                source += 4;
                dest += 4;
            }
            count -= 8;
        }
    }
}

void LZ77UnCompVram(const void *src_, void *dest_)
{
    int destSize = 0;
#ifdef PORT_BRIDGE
    u8 *assetBuf = NULL;
    u32 assetSize = 0;

    /* Straight from the resident payload: the collector only runs between
     * main-loop callbacks, so it stays put while this decompresses. A copy
     * on the heap first cost a malloc and a memcpy per graphic. */
    (void)assetSize;
    {
        const void *given = src_;

        src_ = Port_ResolveAssetPointer(src_);
        if (Port_RejectInvalidLz77Source(src_, given, __builtin_return_address(0), "LZV"))
            return;
    }
#endif
    const u8 *src = src_;
    u8 *dest = dest_;
    destSize = (src[3] << 16) | (src[2] << 8) | src[1];
    src += 4;

    /*
     * One flag byte, then eight tokens: a literal byte, or a back reference
     * of 3-18 bytes up to 4 KiB behind. The bounds checks are those of the
     * byte loop this replaces, made per token instead of per byte.
     */
    {
        u8 *out = dest, *end = dest + destSize;

        for (;;) {
            unsigned flags = *src++;

            for (int i = 0; i < 8; i++, flags <<= 1) {
                if (flags & 0x80) {
                    int blockSize = (src[0] >> 4) + 3;
                    int blockDistance = (((src[0] & 0xF) << 8) | src[1]) + 1;
                    const u8 *from = out - blockDistance;

                    src += 2;
                    // Some Ruby/Sapphire tilesets overflow.
                    if (blockSize > end - out) {
                        blockSize = end - out;
                        DBGPRINTF("Destination buffer overflow.\n");
                    }
                    if (from < dest)
                        goto fail;
                    do
                        *out++ = *from++;
                    while (--blockSize > 0);
                } else {
                    if (out >= end)
                        goto fail;
                    *out++ = *src++;
                }
                if (out == end) {
#ifdef PORT_BRIDGE
                    if (assetBuf != NULL)
                        free(assetBuf);
#endif
                    return;
                }
            }
        }
    }

fail:
#ifdef PORT_BRIDGE
    if (destSize > 0)
        CpuFill16(0, dest_, destSize);
    if (assetBuf != NULL)
        free(assetBuf);
    return;
#else
    puts("Fatal error while decompressing LZ file.\n");
#endif
}

void LZ77UnCompWram(const void *src, void *dst)
{
#ifdef PORT_BRIDGE
    u8 *assetBuf = NULL;
    u32 assetSize = 0;

    /* Straight from the resident payload: the collector only runs between
     * main-loop callbacks, so it stays put while this decompresses. A copy
     * on the heap first cost a malloc and a memcpy per graphic. */
    (void)assetSize;
    {
        const void *given = src;

        src = Port_ResolveAssetPointer(src);
        if (Port_RejectInvalidLz77Source(src, given, __builtin_return_address(0), "LZW"))
            return;
    }
#endif
    const uint8_t *source = src;
    uint8_t *dest = dst;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    /* As above, the length checked per token rather than per byte. */
    uint8_t *end = dest + (header >> 8);

    while (dest < end) {
        unsigned d = *source++;

        for (int i = 0; i < 8 && dest < end; i++, d <<= 1) {
            if (d & 0x80) {
                unsigned data = (source[0] << 8) | source[1];
                unsigned length = (data >> 12) + 3;
                const uint8_t *windowOffset = dest - (data & 0x0FFF) - 1;

                source += 2;
                if (length > (unsigned)(end - dest))
                    length = end - dest;
                while (length-- > 0)
                    *dest++ = *windowOffset++;
            } else {
                if (dest < end)
                    *dest++ = *source++;
            }
        }
    }

#ifdef PORT_BRIDGE
    if (assetBuf != NULL)
        free(assetBuf);
#endif
}

void RLUnCompWram(const void *src, void *dest)
{
    int remaining = CPUReadMemory(src) >> 8;
    int padding = (4 - remaining) & 0x3;
    int blockHeader;
    int block;
    src += 4;
    while (remaining > 0)
    {
        blockHeader = CPUReadByte(src);
        src++;
        if (blockHeader & 0x80) // Compressed?
        {
            blockHeader &= 0x7F;
            blockHeader += 3;
            block = CPUReadByte(src);
            src++;
            while (blockHeader-- && remaining)
            {
                remaining--;
                CPUWriteByte(dest, block);
                dest++;
            }
        }
        else // Uncompressed
        {
            blockHeader++;
            while (blockHeader-- && remaining)
            {
                remaining--;
                u8 byte = CPUReadByte(src);
                src++;
                CPUWriteByte(dest, byte);
                dest++;
            }
        }
    }
    while (padding--)
    {
        CPUWriteByte(dest, 0);
        dest++;
    }
}

void RLUnCompVram(const void *src, void *dest)
{
    int remaining = CPUReadMemory(src) >> 8;
    int padding = (4 - remaining) & 0x3;
    int blockHeader;
    int block;
    int halfWord = 0;
    src += 4;
    while (remaining > 0)
    {
        blockHeader = CPUReadByte(src);
        src++;
        if (blockHeader & 0x80) // Compressed?
        {
            blockHeader &= 0x7F;
            blockHeader += 3;
            block = CPUReadByte(src);
            src++;
            while (blockHeader-- && remaining)
            {
                remaining--;
                if ((u32)dest & 1)
                {
                    halfWord |= block << 8;
                    CPUWriteHalfWord((void*)((u32)dest ^ 1), halfWord);
                }
                else
                    halfWord = block;
                dest++;
            }
        }
        else // Uncompressed
        {
            blockHeader++;
            while (blockHeader-- && remaining)
            {
                remaining--;
                u8 byte = CPUReadByte(src);
                src++;
                if ((u32)dest & 1)
                {
                    halfWord |= byte << 8;
                    CPUWriteHalfWord((void*)((u32)dest ^ 1), halfWord);
                }
                else
                    halfWord = byte;
                dest++;
            }
        }
    }
    if ((u32)dest & 1)
    {
        padding--;
        dest++;
    }
    for (; padding > 0; padding -= 2, dest += 2)
        CPUWriteHalfWord(dest, 0);
}

const s16 sineTable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    for(s32 i=0; i<count; i++)
    {
        s32 cx = src[i].texX;
        s32 cy = src[i].texY;
        s16 dispx = src[i].scrX;
        s16 dispy = src[i].scrY;
        s16 rx = src[i].sx;
        s16 ry = src[i].sy;
        u16 theta = src[i].alpha>>8;
        s32 a = sineTable[(theta+0x40)&255];
        s32 b = sineTable[theta];

        s16 dx =  (rx * a)>>14;
        s16 dmx = (rx * b)>>14;
        s16 dy =  (ry * b)>>14;
        s16 dmy = (ry * a)>>14;

        dest[i].pa = dx;
        dest[i].pb = -dmx;
        dest[i].pc = dy;
        dest[i].pd = dmy;

        s32 startx = cx - dx * dispx + dmx * dispy;
        s32 starty = cy - dy * dispx - dmy * dispy;

        dest[i].dx = startx;
        dest[i].dy = starty;
    }
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    for(s32 i=0; i<count; i++)
    {
        s16 rx = src[i].xScale;
        s16 ry = src[i].yScale;
        u16 theta = src[i].rotation>>8;

        s32 a = (s32)sineTable[(theta + 64) & 255];
        s32 b = (s32)sineTable[theta];

        s16 dx =  ((s32)rx * a)>>14;
        s16 dmx = ((s32)rx * b)>>14;
        s16 dy =  ((s32)ry * b)>>14;
        s16 dmy = ((s32)ry * a)>>14;

        CPUWriteHalfWord(dest, dx);
        dest += offset;
        CPUWriteHalfWord(dest, -dmx);
        dest += offset;
        CPUWriteHalfWord(dest, dy);
        dest += offset;
        CPUWriteHalfWord(dest, dmy);
        dest += offset;
    }
}

// Following functions taken from mGBA's source
u16 ArcTan(s16 i)
{
    s32 a = -((i * i) >> 14);
    s32 b = ((0xA9 * a) >> 14) + 0x390;
    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;

    return (i * b) >> 16;
}

u16 ArcTan2(s16 x, s16 y)
{
    if (!y)
    {
        if (x >= 0)
            return 0;
        return 0x8000;
    }
    if (!x)
    {
        if (y >= 0)
            return 0x4000;
        return 0xC000;
    }
    if (y >= 0)
    {
        if (x >= 0)
        {
            if (x >= y)
                return ArcTan((y << 14) / x);
        }
        else if (-x >= y)
            return ArcTan((y << 14) / x) + 0x8000;
        return 0x4000 - ArcTan((x << 14) / y);
    }
    else
    {
        if (x <= 0)
        {
            if (-x > -y)
                return ArcTan((y << 14) / x) + 0x8000;
        }
        else if (x >= -y)
            return ArcTan((y << 14) / x) + 0x10000;
        return 0xC000 - ArcTan((x << 14) / y);
    }
}

u16 Sqrt(u32 num)
{
    if (!num)
        return 0;
    u32 lower;
    u32 upper = num;
    u32 bound = 1;
    while (bound < upper)
    {
        upper >>= 1;
        bound <<= 1;
    }
    while (1)
    {
        upper = num;
        u32 accum = 0;
        lower = bound;
        while (1)
        {
            u32 oldLower = lower;
            if (lower <= upper >> 1)
                lower <<= 1;
            if (oldLower >= upper >> 1)
                break;
        }
        while (1)
        {
            accum <<= 1;
            if (upper >= lower)
            {
                ++accum;
                upper -= lower;
            }
            if (lower == bound)
                break;
            lower >>= 1;
        }
        u32 oldBound = bound;
        bound += accum;
        bound >>= 1;
        if (bound >= oldBound)
        {
            bound = oldBound;
            break;
        }
    }
    return bound;
}
