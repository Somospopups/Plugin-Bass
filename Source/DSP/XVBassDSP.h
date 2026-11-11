#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>
#include <vector>
#if defined(__APPLE__)
 #include <Accelerate/Accelerate.h>
#endif
#include "XVCabIRs.h"
#include "XVPitchShifter.h"

namespace xvdsp
{

constexpr double kPi = 3.14159265358979323846;

enum class CabMode : int
{
    Bass  = 0,
    Punk  = 1,
    Clean = 2
};

struct PedalControlSnapshot
{
    float   level      = 6.4f;   // 0..10
    float   drive      = 6.2f;   // 0..10
    float   lowDb      = 2.0f;   // -12..+12 dB
    float   midDb      = -2.4f;  // -12..+12 dB
    float   highDb     = 0.8f;   // -12..+12 dB
    float   presenceDb = 7.2f;   // -12..+12 dB
    float   blend      = 0.88f;  // 0..1
    CabMode cab            = CabMode::Punk;
    bool    bypassed       = false;
    bool    tunerMuted     = false;

    // Digital Pitch Shifter / Drop-Tuner
    bool    pitchEnabled   = false;
    int     pitchSemitones = 0;     // -12 .. +12
    float   pitchCents     = 0.0f;  // -50 .. +50
    float   pitchMix       = 1.0f;  // 0 .. 1
};

struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = 0.0; z2 = 0.0; }

    inline float processSample (float x) noexcept
    {
        const double in  = static_cast<double> (x);
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return static_cast<float> (out);
    }

    void setPeaking (double fs, double fc, double gainDb, double Q) noexcept
    {
        fc = std::clamp (fc, 15.0, fs * 0.46);
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * kPi * fc / fs;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + alpha / A);

        b0 = (1.0 + alpha * A) * a0Inv;
        b1 = (-2.0 * cosw) * a0Inv;
        b2 = (1.0 - alpha * A) * a0Inv;
        a1 = (-2.0 * cosw) * a0Inv;
        a2 = (1.0 - alpha / A) * a0Inv;
    }

    void setLowShelf (double fs, double fc, double gainDb, double Q = 0.7071) noexcept
    {
        fc = std::clamp (fc, 15.0, fs * 0.46);
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * kPi * fc / fs;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * Q);
        const double sq    = 2.0 * std::sqrt (A) * alpha;
        const double a0Inv = 1.0 / ((A + 1.0) + (A - 1.0) * cosw + sq);

        b0 = (A * ((A + 1.0) - (A - 1.0) * cosw + sq)) * a0Inv;
        b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw)) * a0Inv;
        b2 = (A * ((A + 1.0) - (A - 1.0) * cosw - sq)) * a0Inv;
        a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosw)) * a0Inv;
        a2 = ((A + 1.0) + (A - 1.0) * cosw - sq) * a0Inv;
    }

    void setHighShelf (double fs, double fc, double gainDb, double Q = 0.7071) noexcept
    {
        fc = std::clamp (fc, 20.0, fs * 0.46);
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * kPi * fc / fs;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * Q);
        const double sq    = 2.0 * std::sqrt (A) * alpha;
        const double a0Inv = 1.0 / ((A + 1.0) - (A - 1.0) * cosw + sq);

        b0 = (A * ((A + 1.0) + (A - 1.0) * cosw + sq)) * a0Inv;
        b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw)) * a0Inv;
        b2 = (A * ((A + 1.0) + (A - 1.0) * cosw - sq)) * a0Inv;
        a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosw)) * a0Inv;
        a2 = ((A + 1.0) - (A - 1.0) * cosw - sq) * a0Inv;
    }

    void setHighPass (double fs, double fc, double Q = 0.7071) noexcept
    {
        fc = std::clamp (fc, 10.0, fs * 0.46);
        const double w0    = 2.0 * kPi * fc / fs;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + alpha);

        b0 = ((1.0 + cosw) * 0.5) * a0Inv;
        b1 = (-(1.0 + cosw)) * a0Inv;
        b2 = ((1.0 + cosw) * 0.5) * a0Inv;
        a1 = (-2.0 * cosw) * a0Inv;
        a2 = (1.0 - alpha) * a0Inv;
    }

    void setLowPass (double fs, double fc, double Q = 0.7071) noexcept
    {
        fc = std::clamp (fc, 20.0, fs * 0.48);
        const double w0    = 2.0 * kPi * fc / fs;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + alpha);

        b0 = ((1.0 - cosw) * 0.5) * a0Inv;
        b1 = (1.0 - cosw) * a0Inv;
        b2 = ((1.0 - cosw) * 0.5) * a0Inv;
        a1 = (-2.0 * cosw) * a0Inv;
        a2 = (1.0 - alpha) * a0Inv;
    }

    void setBandPass (double fs, double fc, double Q = 1.0) noexcept
    {
        fc = std::clamp (fc, 20.0, fs * 0.46);
        const double w0    = 2.0 * kPi * fc / fs;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + alpha);

        b0 = alpha * a0Inv;
        b1 = 0.0;
        b2 = -alpha * a0Inv;
        a1 = (-2.0 * cosw) * a0Inv;
        a2 = (1.0 - alpha) * a0Inv;
    }
};

/**
 * 1024-Tap Zero-Latency Direct-Form / Ring-Buffer Cabinet Convolver
 * Automatically resamples 48 kHz embedded IRs when host runs at 44.1 / 88.2 / 96 kHz.
 */
class CabIrConvolver
{
public:
    static constexpr int kMaxTaps = 2048;

    void prepare (double hostSampleRate)
    {
        resampleIr (kCabIrBass48k, irBass, hostSampleRate);
        resampleIr (kCabIrPunk48k, irPunk, hostSampleRate);
        reset();
    }

    void reset() noexcept
    {
        ring.fill (0.0f);
        writePos = 0;
    }

    inline float processSample (float x, int cabMode) noexcept
    {
        if (--writePos < 0)
            writePos = activeTaps - 1;

        ring[static_cast<size_t> (writePos)]              = x;
        ring[static_cast<size_t> (writePos + activeTaps)] = x;

        float out = x;
        if (cabMode == 0 || cabMode == 1)
        {
            const float* ir = (cabMode == 0) ? irBass.data() : irPunk.data();
            const float* rp = &ring[static_cast<size_t> (writePos)];
#if defined(__APPLE__)
            float sum = 0.0f;
            vDSP_dotpr (rp, 1, ir, 1, &sum, static_cast<vDSP_Length> (activeTaps));
            out = sum;
#else
            float acc0 = 0.0f, acc1 = 0.0f, acc2 = 0.0f, acc3 = 0.0f;
            int k = 0;
            const int n4 = activeTaps & ~3;
            for (; k < n4; k += 4)
            {
                acc0 += rp[k]     * ir[k];
                acc1 += rp[k + 1] * ir[k + 1];
                acc2 += rp[k + 2] * ir[k + 2];
                acc3 += rp[k + 3] * ir[k + 3];
            }
            float sum = (acc0 + acc1) + (acc2 + acc3);
            for (; k < activeTaps; ++k)
                sum += rp[k] * ir[k];
            out = sum;
#endif
        }

        return out;
    }

private:
    static void resampleIr (const std::array<float, 1024>& src48k,
                            std::array<float, kMaxTaps>& dst,
                            double hostSr) noexcept
    {
        dst.fill (0.0f);
        const double ratio = std::clamp (hostSr / 48000.0, 0.5, 2.0);
        const int taps = std::clamp (static_cast<int> (std::round (1024.0 * ratio)), 256, kMaxTaps);
        const float gainComp = static_cast<float> (1.0 / ratio);

        for (int i = 0; i < taps; ++i)
        {
            const double srcPos = static_cast<double> (i) / ratio;
            const int idx0 = std::min (1023, static_cast<int> (srcPos));
            const int idx1 = std::min (1023, idx0 + 1);
            const float frac = static_cast<float> (srcPos - static_cast<double> (idx0));
            dst[static_cast<size_t> (i)] = (src48k[static_cast<size_t> (idx0)] * (1.0f - frac)
                                          + src48k[static_cast<size_t> (idx1)] * frac) * gainComp;
        }
    }

    int activeTaps = 1024;
    int writePos   = 0;
    alignas(16) std::array<float, kMaxTaps * 2> ring {};
    alignas(16) std::array<float, kMaxTaps> irBass {};
    alignas(16) std::array<float, kMaxTaps> irPunk {};

public:
    void updateActiveTaps (double hostSr) noexcept
    {
        const double ratio = std::clamp (hostSr / 48000.0, 0.5, 2.0);
        activeTaps = std::clamp (static_cast<int> (std::round (1024.0 * ratio)), 256, kMaxTaps);
    }
};

/**
 * Jerry Finn / Mark Hoppus Bi-Amp Studio Bass Channel Processor (v4)
 * - Fast 1176 Rev D FET Studio Compressor + Noise-Safe Input Leveler
 * - Bi-Amp Path A (< 215 Hz): API 512c + 1176 Pinned Sub & Sealed 8x10 Chest Punch
 * - Bi-Amp Path B (> 430 Hz): Adaptive Quarter-Pounder Clank Exciter + 4x Oversampled
 *   Asymmetric Ampeg SVT-CL 12AX7 Tube & SansAmp RBI CMOS Grind + Jerry Finn Mid Carve
 * - 1024-Tap Deconvolved SVT-810AV Sealed Paper-Cone Cabinet IR
 */
class XVChannelProcessor
{
public:
    void prepare (double sampleRate)
    {
        fs   = std::max (22050.0, sampleRate);
        fsOs = fs * 4.0;

        inSubsonicHpf.setHighPass (fs, 42.0, 0.72);

        // 1176 FET Compressor ballistics (3.8 ms attack lets pick snap pop; 55 ms fast release pins 8th notes)
        compAtt = static_cast<float> (std::exp (-1.0 / (0.0038 * fs)));
        compRel = static_cast<float> (std::exp (-1.0 / (0.0550 * fs)));

        // Bi-Amp Path A: Clean Pinned Sub + 175 Hz SVT Chest (< 215 Hz Linkwitz-Riley)
        subLpf1.setLowPass (fs, 215.0, 0.7071);
        subLpf2.setLowPass (fs, 215.0, 0.7071);
        subLowEq.setPeaking (fs, 68.0, 3.4, 0.72);
        subChestEq.setPeaking (fs, 175.0, 3.6, 0.90);

        // Bi-Amp Path B: High-Passed Clank Channel (> 430 Hz prevents low fundamentals from hogging the clipper!)
        hiCrossHpf.setHighPass (fs, 430.0, 0.75);
        clankDetBpf.setBandPass (fs, 1450.0, 0.72);
        pickDetBpf.setBandPass (fs, 2450.0, 0.90);

        clankSlowCoeff = static_cast<float> (std::exp (-1.0 / (0.0450 * fs)));
        pickFastCoeff  = static_cast<float> (std::exp (-1.0 / (0.0018 * fs)));

        preDriveScoopEq.setPeaking (fs, 350.0, -5.5, 0.90);

        // 4x Oversampling Anti-Imaging & Anti-Aliasing Cascaded Butterworth LPF
        const double nyq = std::min (19000.0, fs * 0.45);
        upAntiImage1.setLowPass (fsOs, nyq, 0.54119610);
        upAntiImage2.setLowPass (fsOs, nyq, 1.30656296);
        downAntiAlias1.setLowPass (fsOs, nyq, 0.54119610);
        downAntiAlias2.setLowPass (fsOs, nyq, 1.30656296);

        // Post-Drive Jerry Finn Studio Carve (340 Hz & 590 Hz scoop + 1380 Hz metallic clank bark)
        postDriveHpf.setHighPass (fs, 140.0, 0.7071);
        postCarve1.setPeaking (fs, 340.0, -4.8, 1.05);
        postCarve2.setPeaking (fs, 590.0, -4.4, 1.10);
        postClankBark.setPeaking (fs, 1380.0, 4.6, 0.88);

        cabIr.prepare (fs);
        cabIr.updateActiveTaps (fs);
        pitchShifter.prepare (fs);

        updateParameters (PedalControlSnapshot {});
        reset();
    }

    void reset() noexcept
    {
        pitchShifter.reset();
        inSubsonicHpf.reset();
        subLpf1.reset(); subLpf2.reset(); subLowEq.reset(); subChestEq.reset();
        hiCrossHpf.reset(); clankDetBpf.reset(); pickDetBpf.reset();
        preDriveScoopEq.reset(); preDriveSvtMid.reset(); preDrivePresPeak.reset();
        upAntiImage1.reset(); upAntiImage2.reset(); downAntiAlias1.reset(); downAntiAlias2.reset();
        postDriveHpf.reset(); postCarve1.reset(); postCarve2.reset(); postClankBark.reset();
        eqLow.reset(); eqMid.reset(); eqHigh.reset();
        cabIr.reset();

        compEnv   = 0.0f;
        envBody   = 0.01f;
        envClank  = 0.0024f;
        envPickFast = 0.0f;
        envPickMed  = 0.0f;
        bypassSmooth = targetBypass;
        muteSmooth   = targetMute;
    }

    void updateParameters (const PedalControlSnapshot& s) noexcept
    {
        const float dn = std::clamp (s.drive / 10.0f, 0.0f, 1.0f);
        drivePreGain = 2.2f + 38.0f * std::pow (dn, 1.65f);
        driveMakeup  = 0.56f / std::max (0.32f, std::tanh (0.55f * drivePreGain));

        const double pres = static_cast<double> (s.presenceDb);
        preDriveSvtMid.setPeaking (fs, 1420.0, 5.2 + pres * 1.12, 0.82);
        preDrivePresPeak.setPeaking (fs, 2250.0, pres * 0.72, 1.05);

        blendAmt = std::clamp (s.blend, 0.0f, 1.0f);

        eqLow.setPeaking (fs, 78.0, static_cast<double> (s.lowDb), 0.72);
        eqMid.setPeaking (fs, 480.0, static_cast<double> (s.midDb), 0.85);
        eqHigh.setHighShelf (fs, 1650.0, static_cast<double> (s.highDb), 0.75);

        cabMode = static_cast<int> (s.cab);
        const float ln = std::clamp (s.level / 10.0f, 0.0f, 1.0f);
        levelGain = std::pow (ln, 1.45f) * 0.52f;

        targetBypass = s.bypassed ? 1.0f : 0.0f;
        targetMute   = s.tunerMuted ? 0.0f : 1.0f;
        pitchShifter.setParameters (s.pitchEnabled, s.pitchSemitones, s.pitchCents, s.pitchMix);
    }

    inline float processSample (float rawIn) noexcept
    {
        bypassSmooth += 0.004f * (targetBypass - bypassSmooth);
        muteSmooth   += 0.004f * (targetMute   - muteSmooth);

        const float pitchedIn = pitchShifter.processSample (rawIn);
        const float tightIn   = inSubsonicHpf.processSample (pitchedIn);

        // 1. 1176 Rev D Fast Studio FET Compressor + Noise-Safe Auto-Leveler
        const float ax = std::fabs (tightIn);
        compEnv = (ax > compEnv)
                ? (compAtt * compEnv + (1.0f - compAtt) * ax)
                : (compRel * compEnv + (1.0f - compRel) * ax);

        const float tAct = std::clamp ((compEnv - 0.0012f) / (0.0070f - 0.0012f), 0.0f, 1.0f);
        const float act  = tAct * tAct * (3.0f - 2.0f * tAct);

        float rawCompGain = 3.4f;
        if (compEnv > 0.042f)
        {
            const float overDb = 20.0f * std::log10 (compEnv / 0.042f);
            const float redDb  = overDb * (1.0f - 1.0f / 6.0f);
            rawCompGain = 3.4f * std::pow (10.0f, -redDb * 0.05f);
        }
        const float compGain = 1.0f + act * (rawCompGain * 1.12f - 1.0f);
        const float compOut  = tightIn * compGain;

        // 2. Bi-Amp Path A: Clean Pinned Sub + 175 Hz Sealed 8x10 Chest (< 215 Hz)
        float sub = subLpf2.processSample (subLpf1.processSample (compOut));
        sub = subChestEq.processSample (subLowEq.processSample (sub));
        sub = std::tanh (sub * 1.85f) * 0.68f;

        // 3. Bi-Amp Path B: Adaptive Quarter-Pounder Clank + Driven SVT-CL / SansAmp RBI (> 430 Hz)
        const float hiIn      = hiCrossHpf.processSample (compOut);
        const float clankBand = clankDetBpf.processSample (compOut);
        const float pickBand  = pickDetBpf.processSample (compOut);

        if (act > 0.15f)
        {
            envBody  = clankSlowCoeff * envBody  + (1.0f - clankSlowCoeff) * std::fabs (compOut);
            envClank = clankSlowCoeff * envClank + (1.0f - clankSlowCoeff) * std::fabs (clankBand);
        }

        const float clankRatio = (envClank + 1.0e-5f) / (envBody + 1.0e-4f);
        const float targetDyn  = std::clamp (0.30f / clankRatio, 0.68f, 13.5f);
        const float dynGain    = 1.0f + act * (targetDyn - 1.0f);

        const float ap = std::fabs (pickBand);
        envPickFast = pickFastCoeff  * envPickFast + (1.0f - pickFastCoeff)  * ap;
        envPickMed  = clankSlowCoeff * envPickMed  + (1.0f - clankSlowCoeff) * ap;
        const float transSnap = 1.0f + act * std::min (2.8f, 3.2f * std::max (0.0f, envPickFast - envPickMed * 1.12f) / (envPickMed + 0.0025f));

        const float clankBoosted = hiIn * (0.65f + 0.35f * std::min (2.2f, dynGain))
                                 + clankBand * (dynGain - 0.70f) * 1.45f
                                 + pickBand  * (transSnap * dynGain * 0.58f);

        float pre = preDriveScoopEq.processSample (clankBoosted);
        pre = preDriveSvtMid.processSample (pre);
        pre = preDrivePresPeak.processSample (pre);
        pre *= (0.05f + 0.95f * act); // Noise-safe gate prevents idle hiss boost

        // 4x Oversampled Asymmetric SVT 12AX7 Tube + SansAmp CMOS Dual Clipper
        constexpr float kBias = 0.13909245f; // tanh(0.14f)
        float wetDown = 0.0f;
        for (int os = 0; os < 4; ++os)
        {
            const float upIn = (os == 0) ? (pre * 4.0f) : 0.0f;
            const float u    = upAntiImage2.processSample (upAntiImage1.processSample (upIn));
            const float v    = u * drivePreGain;
            const float av   = std::fabs (v);
            const float s1   = std::tanh (v + (0.38f * v * av) / (1.0f + 0.45f * av) + 0.14f) - kBias;
            const float s2   = std::tanh (s1 * 2.15f) * driveMakeup;
            const float d    = downAntiAlias2.processSample (downAntiAlias1.processSample (s2));
            if (os == 3)
                wetDown = d;
        }

        const float postTrim = 0.88f + 0.16f * std::min (2.4f, std::sqrt (dynGain));
        float wet = postDriveHpf.processSample (wetDown * postTrim);
        wet = postCarve1.processSample (wet);
        wet = postCarve2.processSample (wet);
        wet = postClankBark.processSample (wet);

        // 4. Bi-Amp Blend + Active 3-Band EQ + 1024-Tap SVT-810AV Cab IR
        const float subWeight = 0.80f + 0.28f * (1.0f - blendAmt);
        const float cleanFull = compOut * 0.45f * (1.0f - blendAmt);
        const float mixed     = sub * subWeight + cleanFull + wet * (blendAmt * 0.94f);

        float eqOut = eqLow.processSample (mixed);
        eqOut = eqMid.processSample (eqOut);
        eqOut = eqHigh.processSample (eqOut);

        const float cabOut    = cabIr.processSample (eqOut, cabMode);
        const float processed = std::tanh (cabOut * levelGain * 0.95f) * 0.94f;
        const float outSample = processed * (1.0f - bypassSmooth) + rawIn * bypassSmooth;

        return outSample * muteSmooth;
    }

private:
    double fs   = 48000.0;
    double fsOs = 192000.0;

    float compAtt = 0.994f, compRel = 0.9996f, compEnv = 0.0f;
    float clankSlowCoeff = 0.9995f, pickFastCoeff = 0.988f;
    float envBody = 0.01f, envClank = 0.0024f, envPickFast = 0.0f, envPickMed = 0.0f;

    Biquad inSubsonicHpf;
    Biquad subLpf1, subLpf2, subLowEq, subChestEq;
    Biquad hiCrossHpf, clankDetBpf, pickDetBpf;
    Biquad preDriveScoopEq, preDriveSvtMid, preDrivePresPeak;
    Biquad upAntiImage1, upAntiImage2, downAntiAlias1, downAntiAlias2;
    Biquad postDriveHpf, postCarve1, postCarve2, postClankBark;
    Biquad eqLow, eqMid, eqHigh;
    CabIrConvolver cabIr;
    XVBassPitchShifter pitchShifter;

    float drivePreGain = 20.0f;
    float driveMakeup  = 0.56f;
    float blendAmt     = 0.88f;
    int   cabMode      = 1;
    float levelGain    = 0.72f;
    float targetBypass = 0.0f;
    float bypassSmooth = 0.0f;
    float targetMute   = 1.0f;
    float muteSmooth   = 1.0f;
};

class XVBassDSP
{
public:
    void prepare (double sampleRate, int /*samplesPerBlock*/, int numChannels)
    {
        const int chCount = std::clamp (numChannels, 1, 2);
        channels.resize (static_cast<size_t> (chCount));
        for (auto& ch : channels)
            ch.prepare (sampleRate);
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
            ch.reset();
    }

    template <typename AudioBufferType>
    void process (AudioBufferType& buffer, const PedalControlSnapshot& snapshot) noexcept
    {
        const int numCh = std::min (static_cast<int> (channels.size()), buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        if (numCh <= 0 || numSamples <= 0)
            return;

        // Fast-path para bajo mono en bus estéreo (ahorra 50% de CPU en Apple A7 / iPad mini 2, Mac y PC)
        if (numCh == 2 && std::memcmp (buffer.getReadPointer (0),
                                       buffer.getReadPointer (1),
                                       static_cast<size_t> (numSamples) * sizeof (float)) == 0)
        {
            channels[0].updateParameters (snapshot);
            float* data0 = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                data0[i] = channels[0].processSample (data0[i]);
            std::memcpy (buffer.getWritePointer (1), data0, static_cast<size_t> (numSamples) * sizeof (float));
            return;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            channels[static_cast<size_t> (ch)].updateParameters (snapshot);
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = channels[static_cast<size_t> (ch)].processSample (data[i]);
        }
    }

private:
    std::vector<XVChannelProcessor> channels;
};

} // namespace xvdsp
