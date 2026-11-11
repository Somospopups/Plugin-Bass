#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace xvdsp
{

struct TunerReading
{
    bool  hasSignal  = false;
    float freqHz     = 0.0f;
    float cents      = 0.0f;      // -50..+50 (smoothed for needle)
    float rawCents   = 0.0f;
    int   midiNote   = 0;
    int   octave     = 1;
    const char* noteName   = "--";
    const char* stringHint = "Tocá una cuerda";
};

/**
 * Lock-free YIN Chromatic Bass Tuner (27 Hz - 520 Hz).
 * - Audio thread only pushes low-pass conditioned mono samples (2:1 decimated) into a ring buffer.
 * - UI timer thread (30-60 Hz) computes YIN Cumulative Mean Normalized Difference + parabolic
 *   interpolation for ±0.1 cent precision down to 5-string B0 (30.87 Hz) and Drop D1 (36.71 Hz).
 */
class XVBassTuner
{
public:
    static constexpr int kRingSize   = 4096; // Power of two
    static constexpr int kRingMask   = kRingSize - 1;
    static constexpr int kWindowSize = 2048; // ~85 ms @ 24 kHz decimated rate

    void prepare (double newSampleRate) noexcept
    {
        rawSampleRate = std::max (22050.0, newSampleRate);
        decimFactor   = (rawSampleRate >= 70000.0) ? 4 : 2;
        effSampleRate = static_cast<float> (rawSampleRate / static_cast<double> (decimFactor));

        // 2nd-order Butterworth LPF @ 340 Hz isolates pure bass fundamental + 2nd harmonic
        // so bright steel pick noise (1-4 kHz) never causes octave jumping!
        const double w0    = 2.0 * 3.14159265358979323846 * 340.0 / rawSampleRate;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * 0.70710678);
        const double a0Inv = 1.0 / (1.0 + alpha);

        lpB1 = static_cast<float> ((1.0 - cosw) * a0Inv);
        lpB0 = lpB1 * 0.5f;
        lpB2 = lpB0;
        lpA1 = static_cast<float> ((-2.0 * cosw) * a0Inv);
        lpA2 = static_cast<float> ((1.0 - alpha) * a0Inv);

        reset();
    }

    void reset() noexcept
    {
        ring.fill (0.0f);
        writeIndex.store (0, std::memory_order_relaxed);
        decimCounter = 0;
        z1 = 0.0f;
        z2 = 0.0f;
        lastMidiNote  = -1;
        smoothedCents = 0.0f;
        smoothedFreq  = 0.0f;
        holdFrames    = 0;
    }

    void setReferenceA4 (float hz) noexcept
    {
        refA4Hz.store (std::clamp (hz, 430.0f, 450.0f), std::memory_order_relaxed);
    }

    float getReferenceA4() const noexcept
    {
        return refA4Hz.load (std::memory_order_relaxed);
    }

    // Called on real-time audio thread (ultra-cheap: 1 biquad + ring store)
    inline void pushSamples (const float* monoSamples, int numSamples) noexcept
    {
        int wIdx = writeIndex.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = monoSamples[i];
            const float y = lpB0 * x + z1;
            z1 = lpB1 * x - lpA1 * y + z2;
            z2 = lpB2 * x - lpA2 * y;

            if (++decimCounter >= decimFactor)
            {
                decimCounter = 0;
                ring[static_cast<size_t> (wIdx & kRingMask)] = y;
                ++wIdx;
            }
        }
        writeIndex.store (wIdx, std::memory_order_release);
    }

    // Called on UI timer thread (~30-60 Hz)
    TunerReading analyze() noexcept
    {
        const int wIdx = writeIndex.load (std::memory_order_acquire);
        const int startIdx = wIdx - kWindowSize;

        float sumSq = 0.0f;
        for (int i = 0; i < kWindowSize; ++i)
        {
            const float s = ring[static_cast<size_t> ((startIdx + i) & kRingMask)];
            analysisBuf[static_cast<size_t> (i)] = s;
            sumSq += s * s;
        }

        const float rms = std::sqrt (sumSq / static_cast<float> (kWindowSize));
        if (rms < 0.0018f) // ~ -55 dBFS gate
        {
            if (holdFrames > 0)
            {
                --holdFrames;
                return lastValidReading;
            }
            TunerReading idle;
            return idle;
        }

        const int tauMin = std::max (4, static_cast<int> (effSampleRate / 540.0f));  // Max ~540 Hz
        const int tauMax = std::min (kWindowSize / 2 - 2, static_cast<int> (effSampleRate / 27.5f)); // Min ~27.5 Hz (A0/B0)
        const int halfW  = kWindowSize / 2;

        yinBuf[0] = 1.0f;
        float runningSum = 0.0f;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float diff = 0.0f;
            const float* __restrict a = analysisBuf.data();
            const float* __restrict b = analysisBuf.data() + tau;
            for (int j = 0; j < halfW; ++j)
            {
                const float d = a[j] - b[j];
                diff += d * d;
            }
            runningSum += diff;
            yinBuf[static_cast<size_t> (tau)] = (runningSum > 1e-12f)
                ? (diff * static_cast<float> (tau) / runningSum)
                : 1.0f;
        }

        // Find first deep minimum below threshold 0.14 (prevents sub-harmonic lock)
        constexpr float kThreshold = 0.14f;
        int bestTau = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (yinBuf[static_cast<size_t> (tau)] < kThreshold)
            {
                while (tau + 1 < tauMax && yinBuf[static_cast<size_t> (tau + 1)] < yinBuf[static_cast<size_t> (tau)])
                    ++tau;
                bestTau = tau;
                break;
            }
        }

        if (bestTau < 0)
        {
            float minVal = 1.0f;
            for (int tau = tauMin; tau < tauMax; ++tau)
            {
                if (yinBuf[static_cast<size_t> (tau)] < minVal)
                {
                    minVal  = yinBuf[static_cast<size_t> (tau)];
                    bestTau = tau;
                }
            }
            if (minVal > 0.38f)
            {
                if (holdFrames > 0) { --holdFrames; return lastValidReading; }
                return {};
            }
        }

        // Parabolic interpolation around bestTau for sub-cent accuracy
        float refinedTau = static_cast<float> (bestTau);
        if (bestTau > 1 && bestTau < tauMax)
        {
            const float y0 = yinBuf[static_cast<size_t> (bestTau - 1)];
            const float y1 = yinBuf[static_cast<size_t> (bestTau)];
            const float y2 = yinBuf[static_cast<size_t> (bestTau + 1)];
            const float denom = 2.0f * (2.0f * y1 - y2 - y0);
            if (std::fabs (denom) > 1e-9f)
                refinedTau += std::clamp ((y2 - y0) / denom, -0.5f, 0.5f);
        }

        const float detectedHz = effSampleRate / refinedTau;
        if (detectedHz < 27.0f || detectedHz > 540.0f)
            return {};

        const float a4 = refA4Hz.load (std::memory_order_relaxed);
        const float exactMidi = 69.0f + 12.0f * std::log2 (detectedHz / a4);
        const int nearestMidi = static_cast<int> (std::round (exactMidi));
        const float rawCents  = std::clamp ((exactMidi - static_cast<float> (nearestMidi)) * 100.0f, -50.0f, 50.0f);

        // Ballistic smoothing: jumps fast on new note, glides smoothly while tuning a string
        if (nearestMidi != lastMidiNote)
        {
            smoothedCents = rawCents;
            smoothedFreq  = detectedHz;
            lastMidiNote  = nearestMidi;
        }
        else
        {
            smoothedCents += 0.28f * (rawCents - smoothedCents);
            smoothedFreq  += 0.28f * (detectedHz - smoothedFreq);
        }

        static constexpr const char* kNoteNames[12] = {
            "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
        };

        const int noteIdx = ((nearestMidi % 12) + 12) % 12;
        const int octave  = (nearestMidi / 12) - 1;

        TunerReading out;
        out.hasSignal  = true;
        out.freqHz     = smoothedFreq;
        out.cents      = smoothedCents;
        out.rawCents   = rawCents;
        out.midiNote   = nearestMidi;
        out.octave     = octave;
        out.noteName   = kNoteNames[noteIdx];
        out.stringHint = getBassStringHint (nearestMidi);

        lastValidReading = out;
        holdFrames = 10; // Hold reading ~280 ms on decay
        return out;
    }

private:
    static const char* getBassStringHint (int midi) noexcept
    {
        switch (midi)
        {
            case 23: return "5a Cuerda · Si (B0)";
            case 24: return "Drop C · Do (C1)";
            case 26: return "Drop D · Re (D1)";
            case 27: return "4a Mi bemol (Eb1)";
            case 28: return "4a Cuerda · Mi (E1)";
            case 32: return "3a La bemol (Ab1)";
            case 33: return "3a Cuerda · La (A1)";
            case 37: return "2a Re bemol (Db2)";
            case 38: return "2a Cuerda · Re (D2)";
            case 42: return "1a Sol bemol (Gb2)";
            case 43: return "1a Cuerda · Sol (G2)";
            default: return "Modo Cromatico";
        }
    }

    double rawSampleRate = 48000.0;
    float  effSampleRate = 24000.0f;
    int    decimFactor   = 2;
    int    decimCounter  = 0;

    float lpB0 = 1.0f, lpB1 = 0.0f, lpB2 = 0.0f, lpA1 = 0.0f, lpA2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    std::array<float, kRingSize>   ring {};
    std::array<float, kWindowSize> analysisBuf {};
    std::array<float, kWindowSize / 2 + 2> yinBuf {};

    std::atomic<int>   writeIndex { 0 };
    std::atomic<float> refA4Hz    { 440.0f };

    int   lastMidiNote  = -1;
    float smoothedCents = 0.0f;
    float smoothedFreq  = 0.0f;
    int   holdFrames    = 0;
    TunerReading lastValidReading {};
};

} // namespace xvdsp
