#include "global.h"
#include "music_player.h"
#include "sound_mixer.h"
#include "mp2k_common.h"


#ifdef PORTABLE
    #include "cgb_audio.h"
#endif

#ifdef PORT_BRIDGE
extern void Port_SetAudioDebugEx(u16 tag, u16 a, u16 b, u16 c, u16 d);
extern u16 Port_GetLastSongRequest(void);
extern u16 Port_GetAudioLoadCounts(void);
extern u16 Port_GetAudioSongCacheUsedKb(void);
extern u16 Port_GetAudioSampleCacheUsedKb(void);
extern volatile u16 gPortDbgMPlayStartCount;
extern volatile u16 gPortDbgMPlayStartTracks;
extern volatile u16 gPortDbgMPlayStartStatus;
extern void *memalign(u32 alignment, u32 size);
#define PORT_MIX_STAGE(tag, a, b, c, d) Port_SetAudioDebugEx((tag), (a), (b), (c), (d))
#define PORT_MIX_TRACE(tag, a, b, c, d) Port_SetAudioDebugEx((tag), (a), (b), (c), (d))
#else
#define PORT_MIX_STAGE(tag, a, b, c, d) ((void)0)
#define PORT_MIX_TRACE(tag, a, b, c, d) ((void)0)
#endif

#define VCOUNT_VBLANK 160
#define TOTAL_SCANLINES 228

/*
 * Fixed-pitch voices (voice_directsound_no_resample, nearly every drum kit)
 * have no frequency of their own: the GBA mixer steps them one sample per
 * output sample, so they sound at the rate the game mixes at, 13379 Hz in
 * Emerald. This mixer outputs 42060 Hz; stepped one sample per output sample
 * the drums would play three times too fast and over an octave and a half
 * too high, so they are resampled from the GBA's rate like any other voice.
 */
#define FIXED_PITCH_RATE 13379.0f




static inline void GenerateAudio(struct SoundMixerState *mixer, struct MixerSource *chan, struct WaveData2 *wav, s32 *acc, u16 samplesPerFrame, float sampleRateReciprocal);

/*
 * The whole frame is mixed in integers, from the voices to the 16-bit samples
 * the DSP plays: per output sample and channel the float mixer converted an
 * integer to float, scaled it, and compared and truncated a float position -
 * every one of those a stall on the 3DS's VFP11, at 701 samples a frame, and
 * the PSG channels, the reverb, the final sum and the conversion to 16 bits
 * were floats too. A voice adds its sample in 24.8 fixed point times its
 * volume (at most 255), so twelve channels stay far inside 32 bits: MIX_ONE,
 * 2^23, is full scale, as 1.0 was for the floats.
 */
#define MIX_ONE (1 << 23)
void SampleMixer(struct SoundMixerState *mixer, u32 scanlineLimit, u16 samplesPerFrame, s32 *outBuffer, u8 dmaCounter, u16 maxBufSize);
bool32 MP2KTickEnvelope(struct MixerSource *chan, struct WaveData2 *wav);
void GeneratePokemonSampleAudio(struct SoundMixerState *mixer, struct MixerSource *chan, s8 *current, s32 *outBuffer, u16 samplesPerFrame, float sampleRateReciprocal, s32 samplesLeftInWav, signed envR, signed envL, s32 loopLen);
static s8 sub_82DF758(struct MixerSource *chan, u32 current);

void RunMixerFrame(void) {
    struct SoundMixerState *mixer = SOUND_INFO_PTR;

    PORT_MIX_STAGE(0x0D00, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);
    
    if (mixer->lockStatus != MIXER_UNLOCKED) {
        return;
    }
    mixer->lockStatus = MIXER_LOCKED;
    PORT_MIX_STAGE(0x0D01, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);
    
    u32 maxScanlines = mixer->maxScanlines;
    if (mixer->maxScanlines != 0) {
        u32 vcount = REG_VCOUNT;
        maxScanlines += vcount;
        if (vcount < VCOUNT_VBLANK) {
            maxScanlines += TOTAL_SCANLINES;
        }
    }
    
    if (mixer->firstPlayerFunc != NULL) {
        PORT_MIX_STAGE(0x0D02, (u16)((uintptr_t)mixer->firstPlayerFunc >> 16), (u16)(uintptr_t)mixer->firstPlayerFunc, 0, 0);
        mixer->firstPlayerFunc(mixer->firstPlayer);
        PORT_MIX_STAGE(0x0D03, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);
    }
    
    PORT_MIX_STAGE(0x0D04, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);
    if (mixer->cgbMixerFunc != NULL) {
        mixer->cgbMixerFunc();
    } else {
        PORT_MIX_STAGE(0x0D80, 0, 0, 0, 0);
    }
    PORT_MIX_STAGE(0x0D05, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);

    
    s32 samplesPerFrame = mixer->samplesPerFrame;
    s32 *outBuffer = mixer->outBuffer;
    s32 dmaCounter = mixer->dmaCounter;
    
    if (dmaCounter > 1) {
        outBuffer += samplesPerFrame * (mixer->framesPerDmaCycle - (dmaCounter - 1)) * 2;
    }
    
    if (samplesPerFrame <= 0 || samplesPerFrame > MIXED_AUDIO_BUFFER_SIZE || outBuffer == NULL || mixer->numChans > MAX_SAMPLE_CHANNELS) {
        PORT_MIX_STAGE(0x0D81, samplesPerFrame, dmaCounter, mixer->numChans, (u16)((uintptr_t)outBuffer >> 2));
        mixer->lockStatus = MIXER_UNLOCKED;
        return;
    }


    //MixerRamFunc mixerRamFunc = ((MixerRamFunc)MixerCodeBuffer);
    PORT_MIX_STAGE(0x0D06, samplesPerFrame, dmaCounter, MIXED_AUDIO_BUFFER_SIZE, 0);
    SampleMixer(mixer, maxScanlines, samplesPerFrame, outBuffer, dmaCounter, MIXED_AUDIO_BUFFER_SIZE);
    PORT_MIX_STAGE(0x0D07, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);
    #ifdef PORTABLE
        PORT_MIX_STAGE(0x0D08, samplesPerFrame, mixer->dmaCounter, 0, 0);
        cgb_audio_generate(samplesPerFrame);
        PORT_MIX_STAGE(0x0D09, samplesPerFrame, mixer->dmaCounter, 0, 0);
    #endif
}




//__attribute__((target("thumb")))
void SampleMixer(struct SoundMixerState *mixer, u32 scanlineLimit, u16 samplesPerFrame, s32 *outBuffer, u8 dmaCounter, u16 maxBufSize) {
    u32 reverb = mixer->reverb;
    PORT_MIX_TRACE(0x0D10, samplesPerFrame, dmaCounter, mixer->numChans, (u16)reverb);
    if (reverb) {
        // The vanilla reverb effect outputs a mono sound from four sources:
        //  - L/R channels as they were mixer->framesPerDmaCycle frames ago
        //  - L/R channels as they were (mixer->framesPerDmaCycle - 1) frames ago
        s32 *tmp1 = outBuffer;
        s32 *tmp2;
        if (dmaCounter == 2) {
            tmp2 = mixer->outBuffer;
        } else {
            tmp2 = outBuffer + samplesPerFrame * 2;
        }
        uf16 i = 0;
        do {
            s32 s = (s32)(((s64)(tmp1[0] + tmp1[1] + tmp2[0] + tmp2[1]) * (s32)reverb) >> 9);
            tmp1[0] = tmp1[1] = s;
            tmp1+=2;
            tmp2+=2;
        }
        while(++i < samplesPerFrame);
    } else {
        memset(outBuffer, 0, samplesPerFrame * 2 * sizeof(*outBuffer));
    }

    float sampleRateReciprocal = mixer->sampleRateReciprocal;
    sf8 numChans = mixer->numChans;
    struct MixerSource *chan = mixer->chans;
    /* The voices add straight into the frame, on top of the reverb. */
    s32 *acc = outBuffer;

    for (int i = 0; i < numChans; i++, chan++) {
        struct WaveData2 *wav = chan->wav;
        PORT_MIX_TRACE(0x0D11, (u16)i, chan->status, (u16)((uintptr_t)wav >> 16), (u16)(uintptr_t)wav);

        if (scanlineLimit != 0) {
            uf16 vcount = REG_VCOUNT;
            if (vcount < VCOUNT_VBLANK) {
                vcount += TOTAL_SCANLINES;
            }
            if (vcount >= scanlineLimit) {
                goto returnEarly;
            }
        }

        if (MP2KTickEnvelope(chan, wav))
        {
            PORT_MIX_TRACE(0x0D12, (u16)i, chan->status, (u16)((uintptr_t)wav >> 16), (u16)(uintptr_t)wav);
            GenerateAudio(mixer, chan, wav, acc, samplesPerFrame, sampleRateReciprocal);
            PORT_MIX_TRACE(0x0D13, (u16)i, chan->status, chan->ct, 0);
        }
    }
returnEarly:
    mixer->lockStatus = MIXER_UNLOCKED;
    PORT_MIX_TRACE(0x0D14, mixer->lockStatus, mixer->dmaCounter, mixer->numChans, 0);
}

// Returns TRUE if channel is still active after moving envelope forward a frame
//__attribute__((target("thumb")))
bool32 MP2KTickEnvelope(struct MixerSource *chan, struct WaveData2 *wav) {
    // MP2K envelope shape
    //                                                                 |
    // (linear)^                                                       |
    // Attack / \Decay (exponential)                                   |
    //       /   \_                                                    |
    //      /      '.,        Sustain                                  |
    //     /          '.______________                                 |
    //    /                           '-.       Echo (linear)          |
    //   /                 Release (exp) ''--..|\                      |
    //  /                                        \                     |
    
    u8 status = chan->status;
    if ((status & 0xC7) == 0) {
        return FALSE;
    }
    
    u8 env = 0;
    if ((status & 0x80) == 0) {
        env = chan->envelopeVol;
        
        if (status & 4) {
            // Note-wise echo
            // Thumb SUBS/BHI tests the full subtraction, not the stored byte.
            u8 echoLen = chan->echoLen;
            chan->echoLen = echoLen - 1;
            if (echoLen <= 1) {
                chan->status = 0;
                return FALSE;
            } else {
                return TRUE;
            }
        } else if (status & 0x40) {
            // Release
            env = env * chan->release / 256U;
            u8 echoVol = chan->echoVol;
            if (env > echoVol) {
                chan->envelopeVol = env;
                return TRUE;
            } else if (echoVol == 0) {
                chan->status = 0;
                return FALSE;
            } else {
                chan->envelopeVol = echoVol;
                chan->status |= 4;
                return TRUE;
            }
        }
        
        switch (status & 3) {
        uf16 newEnv;
        case 2:
            // Decay
            env = env * chan->decay / 256U;
            
            u8 sustain = chan->sustain;
            if (env <= sustain && sustain == 0) {
                // Duplicated echo check from Release section above
                if (chan->echoVol == 0) {
                    chan->status = 0;
                    return FALSE;
                } else {
                    chan->envelopeVol = chan->echoVol;
                    chan->status |= 4;
                    return TRUE;
                }
            } else if (env <= sustain) {
                env = sustain;
                --chan->status;
            }
            chan->envelopeVol = env;
            break;
        case 3:
        attack:
            newEnv = env + chan->attack;
            if (newEnv >= 0xFF) {
                chan->envelopeVol = 0xFF;
                --chan->status;
            } else {
                chan->envelopeVol = newEnv;
            }
            break;
        case 1: // Sustain
        default:
            break;
        }
        
        return TRUE;
    } else if (status & 0x40) {
        // Init and stop cancel each other out
        chan->status = 0;
        return FALSE;
    } else {
        // Init channel
        chan->status = 3;
#ifdef POKEMON_EXTENSIONS
        chan->current = wav->data + chan->ct;
        chan->ct = wav->size - chan->ct;
#else
        chan->current = wav->data;
        chan->ct = wav->size;
#endif
        chan->fw = 0;
        chan->envelopeVol = 0;
        if (wav->loopFlags & 0xC0) {
            chan->status |= 0x10;
        }
        goto attack;
    }
}

//__attribute__((target("thumb")))
static inline void GenerateAudio(struct SoundMixerState *mixer, struct MixerSource *chan, struct WaveData2 *wav, s32 *acc, u16 samplesPerFrame, float sampleRateReciprocal) {/*, [[[]]]) {*/
    uf8 v = chan->envelopeVol * (mixer->masterVol + 1) / 16U;
    chan->envelopeVolR = chan->rightVol * v / 256U;
    chan->envelopeVolL = chan->leftVol * v / 256U;
    
    s32 loopLen = 0;
    s8 *loopStart;
    if (chan->status & 0x10) {
        loopStart = wav->data + wav->loopStart;
        loopLen = wav->size - wav->loopStart;
    }
    s32 samplesLeftInWav = chan->ct;
    s8 *current = chan->current;
    signed envR = chan->envelopeVolR;
    signed envL = chan->envelopeVolL;
#ifdef POKEMON_EXTENSIONS
    if (chan->type & 0x30) {
        GeneratePokemonSampleAudio(mixer, chan, current, acc, samplesPerFrame, sampleRateReciprocal, samplesLeftInWav, envR, envL, loopLen);
    }
    else
#endif
    {
        // The position within the sample is 8.24 fixed point (below 1 between steps).
        u32 finePos = (u32)(chan->fw * 16777216.0f);
        float romRate = (chan->type & 8) ? FIXED_PITCH_RATE : chan->freq;
        u32 romSamplesPerOutputSample = (u32)(romRate * sampleRateReciprocal * 16777216.0f);
        // Fixed-pitch voices hold each sample, as the GBA's DAC does at its
        // own rate, instead of blending between samples: the steps carry the
        // highs cymbals and triangles have on the console.
        u32 fracMask = (chan->type & 8) ? 0 : 0xFFFFFF;

        s32 b = current[0];
        s32 m = current[1] - b;
        current += 1;

        for (u16 i = 0; i < samplesPerFrame; i++, acc+=2) {
            // Use linear interpolation to calculate a value between the current sample in the wav
            // and the next sample, in 24.8 fixed point.
            s32 sample = ((b * 65536) + (s32)((finePos & fracMask) >> 8) * m) >> 8;

            acc[1] += sample * envR;
            acc[0] += sample * envL;

            finePos += romSamplesPerOutputSample;
            u32 newCoarsePos = finePos >> 24;
            if (newCoarsePos != 0) {
                finePos &= 0xFFFFFF;
                samplesLeftInWav -= newCoarsePos;
                if (samplesLeftInWav <= 0) {
                    if (loopLen != 0) {
                        current = loopStart;
                        newCoarsePos = -samplesLeftInWav;
                        samplesLeftInWav += loopLen;
                        while (samplesLeftInWav <= 0) {
                            newCoarsePos -= loopLen;
                            samplesLeftInWav += loopLen;
                        }
                        b = current[newCoarsePos];
                        m = current[newCoarsePos + 1] - b;
                        current += newCoarsePos + 1;
                    } else {
                        chan->status = 0;
                        return;
                    }
                } else {
                    b = current[newCoarsePos - 1];
                    m = current[newCoarsePos] - b;
                    current += newCoarsePos;
                }
            }
        }
        
        chan->fw = (float)finePos * (1.0f / 16777216.0f);
        chan->ct = samplesLeftInWav;
        chan->current = current - 1;
    }
}

struct WaveData
{
    u16 type;
    u16 status;
    u32 freq;
    u32 loopStart;
    u32 size; // number of samples
    s8 data[1]; // samples
};

/* (sample * env) / 32768 of full scale is sample * env * 256 in MIX_ONE units. */
#define CRY_OUT(sample, env) ((s32)((sample) * (float)((env) * 256)))

void GeneratePokemonSampleAudio(struct SoundMixerState *mixer, struct MixerSource *chan, s8 *current, s32 *outBuffer, u16 samplesPerFrame, float sampleRateReciprocal, s32 samplesLeftInWav, signed envR, signed envL, s32 loopLen) {
    struct WaveData *wav = chan->wav; // r6
    float finePos = chan->fw;
    if((chan->status & 0x20) == 0) {
        chan->status |= 0x20;
        if(chan->type & 0x10) {
            s8 *waveEnd = wav->data + wav->size;
            current = wav->data + (waveEnd - current);
            chan->current = current;
        }
        if(wav->type != 0) {
            current -= (uintptr_t)&wav->data;
            chan->current = current;
        }
    }
    float romSamplesPerOutputSample = (chan->type & 8 ? FIXED_PITCH_RATE : chan->freq) * sampleRateReciprocal;
    if(wav->type != 0) { // is compressed
        chan->blockCount = 0xFF000000;
        if(chan->type & 0x10) { // is reverse
            current -= 1;
            sf16 b = sub_82DF758(chan, (uintptr_t)current);
            sf16 m = sub_82DF758(chan, (uintptr_t)current - 1) - b;

            for (u16 i = 0; i < samplesPerFrame; i++, outBuffer+=2) {
                float sample = (finePos * m) + b;
                
                outBuffer[1] += CRY_OUT(sample, envR);
                outBuffer[0] += CRY_OUT(sample, envL);
                
                finePos += romSamplesPerOutputSample;
                int newCoarsePos = finePos;
                if (newCoarsePos != 0) {
                    finePos -= (int)finePos;
                    samplesLeftInWav -= newCoarsePos;
                    if (samplesLeftInWav <= 0) {
                        chan->status = 0;
                        break;
                    }
                    else {
                        current -= newCoarsePos;
                        b = sub_82DF758(chan, (uintptr_t)current);
                        m = sub_82DF758(chan, (uintptr_t)current - 1) - b;
                    }
                }
            }
            
            chan->fw = finePos;
            chan->ct = samplesLeftInWav;
            chan->current = current + 1;
        }
        else {
            sf16 b = sub_82DF758(chan, (uintptr_t)current);
            sf16 m = sub_82DF758(chan, (uintptr_t)current + 1) - b;
            current += 1;
        
            for (u16 i = 0; i < samplesPerFrame; i++, outBuffer+=2) {
                float sample = (finePos * m) + b;
                
                outBuffer[1] += CRY_OUT(sample, envR);
                outBuffer[0] += CRY_OUT(sample, envL);
                
                finePos += romSamplesPerOutputSample;
                u32 newCoarsePos = finePos; // lr
                if (newCoarsePos != 0) {
                    finePos -= (int)finePos;
                    samplesLeftInWav -= newCoarsePos;
                    if (samplesLeftInWav <= 0) {
                        if (loopLen != 0) {
                            current = chan->wav->loopStart;
                            newCoarsePos = -samplesLeftInWav;
                            samplesLeftInWav += loopLen;
                            while (samplesLeftInWav <= 0) {
                                newCoarsePos -= loopLen;
                                samplesLeftInWav += loopLen;
                            }
                            current += newCoarsePos;
                            b = sub_82DF758(chan, (uintptr_t)current);
                            m = sub_82DF758(chan, (uintptr_t)current + 1) - b;
                            current += 1;
                        } else {
                            chan->status = 0;
                            return;
                        }
                    } else {
                        current += newCoarsePos - 1;
                        b = sub_82DF758(chan, (uintptr_t)current);
                        m = sub_82DF758(chan, (uintptr_t)current + 1) - b;    
                        current += 1;
                    }
                }
            }
            
            chan->fw = finePos;
            chan->ct = samplesLeftInWav;
            chan->current = current - 1;
        }
    }
    else {
        if(chan->type & 0x10) { // is reverse
            current -= 1;
            sf16 b = current[0];
            sf16 m = current[-1] - b;
            
            for (u16 i = 0; i < samplesPerFrame; i++, outBuffer+=2) {
                float sample = (finePos * m) + b;
                
                outBuffer[1] += CRY_OUT(sample, envR);
                outBuffer[0] += CRY_OUT(sample, envL);
                
                finePos += romSamplesPerOutputSample;
                int newCoarsePos = finePos;
                if (newCoarsePos != 0) {
                    finePos -= (int)finePos;
                    samplesLeftInWav -= newCoarsePos;
                    if (samplesLeftInWav <= 0) {
                        chan->status = 0;
                        break;
                    }
                    else {
                        current -= newCoarsePos;
                        b = current[0];
                        m = current[-1] - b;
                    }
                }
            }
            
            chan->fw = finePos;
            chan->ct = samplesLeftInWav;
            chan->current = current + 1;
        }
    }
}

s8 gBDPCMBlockBuffer[64];
extern const s8 gDeltaEncodingTable[];

static s8 sub_82DF758(struct MixerSource *chan, u32 current) {
    u32 blockOffset = current >> 6; // current / 64
    u8 * blockPtr;
    int i;
    //In route 102 lotad wild battle when it growls crashes the game because it decompresses out of bounds data
    //I gave it its own printf error so it wouldn't get forgotten as this needs a more proper fix
    if (chan->wav->size < blockOffset * 0x21) {
            DBGPRINTF("Out of bounds decompress in %s wav->size = %u blockPtr = %u\n", __func__, chan->wav->size, blockOffset * 0x21);
            return gBDPCMBlockBuffer[current & 63];
    }
    
    if(chan->blockCount != blockOffset) { // decode block if not decoded
        s32 s;
        chan->blockCount = blockOffset;
        blockPtr = chan->wav->data + chan->blockCount * 0x21;
        gBDPCMBlockBuffer[0] = s = (s8)*blockPtr++;
        gBDPCMBlockBuffer[1] = s += gDeltaEncodingTable[*blockPtr++ & 0xF];
        for(i = 2; i < 64; i+=2) {
            u32 temp = *blockPtr++;
            gBDPCMBlockBuffer[i] = s += gDeltaEncodingTable[temp >> 4];
            gBDPCMBlockBuffer[i+1] = s += gDeltaEncodingTable[temp & 0xF];
        }
    }
    return gBDPCMBlockBuffer[current & 63]; // index same as current % 64
}
