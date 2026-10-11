#include "global.h"
#include "cgb_audio.h"
#include "cgb_tables.h"

struct AudioCGB gb;
float soundChannelPos[4];
const s16 *PU1Table;
const s16 *PU2Table;
u32 apuFrame;
u8 apuCycle;
u32 sampleRate;
u16 lfsrMax[2];
/* The noise channel's position between LFSR steps, 8.24 fixed point. */
static u32 sNoisePhase;
/* 5.27 fixed-point wave positions of the three tone channels, see cgb_audio_generate. */
static u32 sPhase[3];
static u32 sPhaseStep[3];

void cgb_audio_init(u32 rate){
    gb.ch1Freq = 0;
    gb.ch1SweepCounter = 0;
    gb.ch1SweepCounterI = 0;
    gb.ch1SweepDir = 0;
    gb.ch1SweepShift = 0;
    for (u8 ch = 0; ch < 4; ch++){
        gb.Vol[ch] = 0;
        gb.VolI[ch] = 0;
        gb.Len[ch] = 0;
        gb.LenI[ch] = 0;
        gb.LenOn[ch] = 0;
        gb.EnvCounter[ch] = 0;
        gb.EnvCounterI[ch] = 0;
        gb.EnvDir[ch] = 0;
        gb.DAC[ch] = 0;
        soundChannelPos[ch] = 0;
    }
    soundChannelPos[1] = 1;
    sPhase[0] = 0;
    sPhase[1] = 134217728u;
    sPhase[2] = 0;
    PU1Table = PU0;
    PU2Table = PU0;
    sampleRate = rate;
    gb.ch4LFSR[0] = 0x8000;
    gb.ch4LFSR[1] = 0x80;
    lfsrMax[0] = 0x8000;
    lfsrMax[1] = 0x80;
    sNoisePhase = 0;
}


void cgb_set_sweep(u8 sweep){
    gb.ch1SweepDir = (sweep & 0x08) >> 3;
    gb.ch1SweepCounter = gb.ch1SweepCounterI = (sweep & 0x70) >> 4;
    gb.ch1SweepShift = (sweep & 0x07);
}


void cgb_set_wavram(){
    /* n / 7.5 - 1 is (2n - 15) / 15: kept in fifteenths, see cgb_audio_generate. */
    for(u8 wavi = 0; wavi < 0x10; wavi++){
        gb.WAVRAM[(wavi << 1)] = (s8)(2 * (((*(REG_ADDR_WAVE_RAM0 + wavi)) & 0xF0) >> 4) - 15);
        gb.WAVRAM[(wavi << 1) + 1] = (s8)(2 * ((*(REG_ADDR_WAVE_RAM0 + wavi)) & 0x0F) - 15);
    }
}


void cgb_toggle_length(u8 channel, bool8 state){
    gb.LenOn[channel] = state;
}


void cgb_set_length(u8 channel, u8 length){
    gb.Len[channel] = gb.LenI[channel] = length;
}


void cgb_set_envelope(u8 channel, u8 envelope){
    if(channel == 2){
        switch((envelope & 0xE0)){
            case 0x00:  // mute
                gb.Vol[2] = gb.VolI[2] = 0;
            break;
            case 0x20:  // full
                gb.Vol[2] = gb.VolI[2] = 4;
            break;
            case 0x40:  // half
                gb.Vol[2] = gb.VolI[2] = 2;
            break;
            case 0x60:  // quarter
                gb.Vol[2] = gb.VolI[2] = 1;
            break;
            case 0x80:  // 3 quarters
                gb.Vol[2] = gb.VolI[2] = 3;
            break;
        }
    }else{
        gb.DAC[channel] = (envelope & 0xF8) > 0;
        gb.Vol[channel] = gb.VolI[channel] = (envelope & 0xF0) >> 4;
        gb.EnvDir[channel] = (envelope & 0x08) >> 3;
        gb.EnvCounter[channel] = gb.EnvCounterI[channel] = (envelope & 0x07);
    }
}


void cgb_trigger_note(u8 channel){
    gb.Vol[channel] = gb.VolI[channel];
    gb.Len[channel] = gb.LenI[channel];
    if(channel != 2) gb.EnvCounter[channel] = gb.EnvCounterI[channel];
    if(channel == 3) {
        gb.ch4LFSR[0] = 0x8000;
        gb.ch4LFSR[1] = 0x80;
    }
}


/*
 * The wave position of the three tone channels is kept as a 5.27 fixed-point
 * phase: 32 steps to a cycle, so adding the step wraps by itself and the step
 * into a table is a shift. The float version this replaces compared, wrapped
 * and truncated three positions per output sample, and divided each step by
 * sampleRate / 32 - all of it slow on the 3DS's VFP11, 60 times a second at
 * 701 samples a frame.
 */
#define CGB_PHASE_ONE 134217728.0f /* 2^27: one table step */

/*
 * The output is fixed point with 1 << 23 to full scale, as the sample mixer's
 * (sound_mixer.c). A square or noise step of volume v was v / 15 of a channel
 * and the four channels were summed at a quarter: v * 2^23 / 60 a step. The
 * wave channel's steps are fifteenths of its volume (0-4) at another quarter:
 * 2^23 / 240.
 */
#define CGB_SQUARE_STEP 139810 /* 2^23 / 60 */
#define CGB_WAVE_STEP 34953    /* 2^23 / 240 */
#define CGB_RECIPROCALS 64
static u32 sReciprocal[CGB_RECIPROCALS];


static u32 CgbPhaseStep(float tableStep, float stepScale)
{
    float step = tableStep * stepScale;

    /* Past 2^32 the cast is undefined; that is a tone far above hearing. */
    return step >= 4294967040.0f ? 0xFFFFFF00u : (u32)step;
}

/* Samples [0, count) of a run in which no channel's settings change. */
static void GenerateRun(s32 *out, u32 count, u32 nr51, u32 nr30, u32 nr43, u32 noiseStep)
{
    u32 nr52 = REG_NR52;
    bool32 on = (nr52 & 0x80) != 0;

    memset(out, 0, count * 2 * sizeof(*out));
    /* The tone channels' phases run whether they sound or not. */
    for (u32 ch = 0; ch < 3; ch++) {
        u32 phase = sPhase[ch], step = sPhaseStep[ch];
        bool32 sounds;
        s32 scale;
        const s16 *table16 = NULL;
        const s8 *table8 = NULL;
        u32 left = nr51 & (0x10 << ch), right = nr51 & (0x01 << ch);

        if (ch < 2) {
            sounds = on && gb.DAC[ch] && (nr52 & (1 << ch));
            scale = gb.Vol[ch] * CGB_SQUARE_STEP;
            table16 = ch == 0 ? PU1Table : PU2Table;
        } else {
            sounds = on && (nr30 & 0x80) && (nr52 & 0x04);
            scale = gb.Vol[2] * CGB_WAVE_STEP;
            table8 = gb.WAVRAM;
        }
        if (!sounds || !(left || right) || scale == 0) {
            sPhase[ch] = phase + step * count;
            continue;
        }
        for (u32 k = 0; k < count; k++) {
            s32 v;

            phase += step;
            v = scale * (table16 ? table16[phase >> 27] : table8[phase >> 27]);
            if (left) out[k * 2] += v;
            if (right) out[k * 2 + 1] += v;
        }
        sPhase[ch] = phase;
    }
    if (on && gb.DAC[3] && (nr52 & 0x08)) {
        for (u32 k = 0; k < count; k++) {
            bool32 lfsrMode = ((nr43 & 0x08) == 8);
            u32 lfsr = gb.ch4LFSR[lfsrMode], top = lfsrMax[lfsrMode];
            u32 steps;
            int ch4Out = (lfsr & 1) ? 1 : -1;
            int avgDiv;

            sNoisePhase += noiseStep;
            steps = sNoisePhase >> 24;
            sNoisePhase &= 0xFFFFFF;
            avgDiv = 1 + (int)steps;
            while (steps--) {
                /* Bits 1 and 2 before the shift: bit 1 on both sides of it. */
                u32 carry = ((lfsr >> 1) ^ (lfsr >> 2)) & 1;

                lfsr >>= 1;
                if (carry) lfsr |= top;
                ch4Out += (lfsr & 1) ? 1 : -1;
            }
            gb.ch4LFSR[lfsrMode] = (u16)lfsr;
            /* The mean of the steps taken, by a reciprocal: the ARM11
             * has no divide instruction. */
            s32 sample = ch4Out * gb.Vol[3] * CGB_SQUARE_STEP;
            if(avgDiv > 1)
                sample = avgDiv < CGB_RECIPROCALS
                       ? (s32)(((s64)sample * sReciprocal[avgDiv]) >> 16) : sample / avgDiv;
            if (nr51 & 0x80) out[k * 2] += sample;
            if (nr51 & 0x08) out[k * 2 + 1] += sample;
        }
    }
}

void cgb_audio_generate(u16 samplesPerFrame){
    s32 *outBuffer = gb.outBuffer;
    const float stepScale = CGB_PHASE_ONE / (float)(sampleRate / 32);
    const float noiseScale = 1.0f / (float)sampleRate;

    switch(REG_NR11 & 0xC0){
        case 0x00:
            PU1Table = PU0;
        break;
        case 0x40:
            PU1Table = PU1;
        break;
        case 0x80:
            PU1Table = PU2;
        break;
        case 0xC0:
            PU1Table = PU3;
        break;
    }

    switch(REG_NR21 & 0xC0){
        case 0x00:
            PU2Table = PU0;
        break;
        case 0x40:
            PU2Table = PU1;
        break;
        case 0x80:
            PU2Table = PU2;
        break;
        case 0xC0:
            PU2Table = PU3;
        break;
    }

    sPhaseStep[0] = CgbPhaseStep(freqTable[REG_SOUND1CNT_X & 0x7FF], stepScale);
    sPhaseStep[1] = CgbPhaseStep(freqTable[REG_SOUND2CNT_H & 0x7FF], stepScale);
    sPhaseStep[2] = CgbPhaseStep(freqTable[REG_SOUND3CNT_X & 0x7FF], stepScale);

    /* Written only by the sequencer, which waits for this frame. */
    const u32 nr51 = REG_NR51, nr30 = REG_NR30, nr43 = REG_NR43;
    /* LFSR steps per output sample (up to 12.5), 8.24 fixed point: the float
     * this replaces was compared and decremented once per step. */
    const u32 noiseStep = (u32)(freqTableNSE[REG_SOUND4CNT_H & 0xFF] * noiseScale * 16777216.0f);

    if (sReciprocal[2] == 0)
        for (int d = 2; d < CGB_RECIPROCALS; d++)
            sReciprocal[d] = (65536 + d / 2) / d;

    /*
     * Generated in runs between two steps of the APU's frame sequencer (one
     * every 82 samples): within a run no channel's volume, length or sweep
     * changes, so each channel is one tight loop of its own instead of four
     * channels' state read back for every sample. The sum is the same.
     */
    for (u16 i = 0; i < samplesPerFrame;) {
        /* Samples before the one whose step comes due (apuFrame < sampleRate). */
        u32 plain = (sampleRate - apuFrame + 511) / 512 - 1;

        if (plain > (u32)(samplesPerFrame - i))
            plain = samplesPerFrame - i;
        if (plain) {
            GenerateRun(outBuffer + i * 2, plain, nr51, nr30, nr43, noiseStep);
            apuFrame += 512 * plain;
            i += plain;
            if (i == samplesPerFrame)
                break;
        }
        apuFrame += 512;
        apuFrame -= sampleRate;
        apuCycle++;

        if((apuCycle & 1) == 0){  // Length
            for(u8 ch = 0; ch < 4; ch++){
                if(gb.Len[ch]){
                    if(--gb.Len[ch] == 0 && gb.LenOn[ch]){
                        REG_NR52 &= (0xFF ^ (1 << ch));
                    }
                }
            }
        }

        if((apuCycle & 7) == 7){  // Envelope
            for(u8 ch = 0; ch < 4; ch++){
                if(ch == 2) continue;  // Skip wave channel
                if(gb.EnvCounter[ch]){
                    if(--gb.EnvCounter[ch] == 0){
                        if(gb.Vol[ch] && !gb.EnvDir[ch]){
                            gb.Vol[ch]--;
                            gb.EnvCounter[ch] = gb.EnvCounterI[ch];
                        }else if(gb.Vol[ch] < 0x0F && gb.EnvDir[ch]){
                            gb.Vol[ch]++;
                            gb.EnvCounter[ch] = gb.EnvCounterI[ch];
                        }
                    }
                }
            }
        }

        if((apuCycle & 3) == 2){  // Sweep
            if(gb.ch1SweepCounterI && gb.ch1SweepShift){
                if(--gb.ch1SweepCounter == 0){
                    gb.ch1Freq = REG_SOUND1CNT_X & 0x7FF;
                    if(gb.ch1SweepDir){
                        gb.ch1Freq -= gb.ch1Freq >> gb.ch1SweepShift;
                        if(gb.ch1Freq & 0xF800) gb.ch1Freq = 0;
                    }else{
                        gb.ch1Freq += gb.ch1Freq >> gb.ch1SweepShift;
                        if(gb.ch1Freq & 0xF800){
                            gb.ch1Freq = 0;
                            gb.EnvCounter[0] = 0;
                            gb.Vol[0] = 0;
                        }
                    }
                    REG_NR13 = gb.ch1Freq & 0xFF;
                    REG_NR14 &= 0xF8;
                    REG_NR14 += (gb.ch1Freq >> 8) & 0x07;
                    gb.ch1SweepCounter = gb.ch1SweepCounterI;
                    sPhaseStep[0] = CgbPhaseStep(freqTable[REG_SOUND1CNT_X & 0x7FF], stepScale);
                }
            }
        }
        GenerateRun(outBuffer + i * 2, 1, nr51, nr30, nr43, noiseStep);
        i++;
    }
}

s32 *cgb_get_buffer(){
    return gb.outBuffer;
}
