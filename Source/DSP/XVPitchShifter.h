#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

namespace xvdsp
{

/**
 * Studio-Grade Low-Frequency WSOLA (Waveform Similarity Overlap-Add) Bass Pitch Shifter.
 *
 * Engineered specifically for electric bass (B0 30.87 Hz - G4):
 * - Dual complementary sin^2(pi*p) grains (~44 ms window covering ~2 cycles of low E1 41.2 Hz)
 * - Correlation-locked grain splice search at every grain reset (when window == 0.0):
 *   snaps the incoming grain's read pointer to an integer-period phase match with the
 *   currently active grain, eliminating low-end phase cancellation, tremolo warble,
 *   and metallic comb filtering.
 * - 4-point Catmull-Rom cubic Hermite fractional delay interpolation.
 * - Anti-aliasing 2nd-order Butterworth LPF when pitching upward (+semitones).
 * - Zero-latency true bypass when pitch shift is inactive or 0 st / 0 ct.
 */
class XVBassPitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = (sampleRate > 8000.0) ? sampleRate : 48000.0;

        // Buffer circular potencia de 2 (~170 ms a 48 kHz, ~340 ms a 44.1/96 kHz escalado)
        bufSize = 16384;
        bufMask = bufSize - 1;
        ringBuf.assign (static_cast<size_t> (bufSize), 0.0f);

        // Ventana de grano de ~45 ms (óptima para fundamentales graves E1/D1/B0 sin latencia excesiva)
        grainLen = std::max (512, std::min (4096, static_cast<int> (std::round (0.046 * sr))));
        corrLen  = std::max (128, std::min (1024, static_cast<int> (std::round (0.015 * sr))));
        searchRadius = std::max (64, std::min (1400, static_cast<int> (std::round (0.020 * sr))));

        reset();
        updateAntiAliasFilter (1.0f);
    }

    void reset()
    {
        std::fill (ringBuf.begin(), ringBuf.end(), 0.0f);
        writePos = 0;

        const float nomDelay = static_cast<float> (grainLen) * 0.65f;
        delayA = nomDelay;
        delayB = nomDelay;
        phaseA = 0.0f;
        aaZ1 = aaZ2 = 0.0f;
        wetSmooth = 0.0f;
    }

    void setParameters (bool enabled, int semitones, float fineCents, float mix01)
    {
        isEnabled  = enabled;
        semiShift  = std::max (-12, std::min (12, semitones));
        centsShift = std::max (-50.0f, std::min (50.0f, fineCents));
        wetTarget  = std::max (0.0f, std::min (1.0f, mix01));

        const float totalSemi = static_cast<float> (semiShift) + centsShift * 0.01f;
        pitchActive = isEnabled && (std::fabs (totalSemi) > 0.005f) && (wetTarget > 0.001f);

        if (pitchActive)
        {
            const float newRatio = std::pow (2.0f, totalSemi / 12.0f);
            if (std::fabs (newRatio - ratio) > 0.0005f)
            {
                ratio = newRatio;
                updateAntiAliasFilter (ratio);
            }
        }
        else
        {
            ratio = 1.0f;
        }
    }

    bool isActive() const noexcept { return pitchActive; }
    int  getSemitones() const noexcept { return semiShift; }
    float getCents() const noexcept { return centsShift; }
    float getMix() const noexcept { return wetTarget; }
    bool getEnabled() const noexcept { return isEnabled; }

    inline float processSample (float x) noexcept
    {
        // Siempre alimentamos el ring buffer para que al activar el pitch no haya silencio ni click
        const float xPre = (ratio > 1.02f) ? processAntiAlias (x) : x;
        ringBuf[static_cast<size_t> (writePos)] = xPre;

        const float targetActive = pitchActive ? 1.0f : 0.0f;
        wetSmooth += 0.005f * (targetActive - wetSmooth);

        if (wetSmooth < 0.0002f && ! pitchActive)
        {
            writePos = (writePos + 1) & bufMask;
            return x;
        }

        const float invGrain = 1.0f / static_cast<float> (grainLen);
        const float prevPhaseA = phaseA;
        phaseA += invGrain;
        bool resetA = false;
        if (phaseA >= 1.0f)
        {
            phaseA -= 1.0f;
            resetA = true;
        }

        float phaseB = phaseA + 0.5f;
        if (phaseB >= 1.0f)
            phaseB -= 1.0f;
        const bool resetB = (prevPhaseA < 0.5f && (prevPhaseA + invGrain) >= 0.5f);

        // Evolución de los retardos fraccionales: d(n+1) = d(n) + (1 - ratio)
        const float dStep = 1.0f - ratio;
        delayA += dStep;
        delayB += dStep;

        // Retardo nominal centrado para evitar underflow (< 8 samples) y overflow (> bufSize - corrLen)
        const float minSafeDelay = 16.0f + static_cast<float> (searchRadius);
        const float nominalDelay = (ratio > 1.0f)
            ? (minSafeDelay + (ratio - 1.0f) * static_cast<float> (grainLen) + 64.0f)
            : (minSafeDelay + 96.0f);

        if (resetA)
            delayA = findCorrelatedDelay (delayB, nominalDelay);
        if (resetB)
            delayB = findCorrelatedDelay (delayA, nominalDelay);

        // Ventanas complementarias exactas: sin^2(pi * pA) + sin^2(pi * pB) == 1.0
        constexpr float kPi = 3.14159265358979323846f;
        const float sA = std::sin (kPi * phaseA);
        const float wA = sA * sA;
        const float wB = 1.0f - wA;

        const float yA = readHermite (delayA);
        const float yB = readHermite (delayB);
        const float shifted = wA * yA + wB * yB;

        writePos = (writePos + 1) & bufMask;

        const float mixed = x * (1.0f - wetTarget) + shifted * wetTarget;
        return x * (1.0f - wetSmooth) + mixed * wetSmooth;
    }

private:
    inline float readSampleInt (int delaySamples) const noexcept
    {
        const int idx = (writePos - delaySamples) & bufMask;
        return ringBuf[static_cast<size_t> (idx)];
    }

    inline float readHermite (float delaySamples) const noexcept
    {
        const float clamped = std::max (2.0f, std::min (static_cast<float> (bufSize - 8), delaySamples));
        const int dInt = static_cast<int> (clamped);
        const float frac = clamped - static_cast<float> (dInt);

        const float xm1 = ringBuf[static_cast<size_t> ((writePos - dInt + 1) & bufMask)];
        const float x0  = ringBuf[static_cast<size_t> ((writePos - dInt)     & bufMask)];
        const float x1  = ringBuf[static_cast<size_t> ((writePos - dInt - 1) & bufMask)];
        const float x2  = ringBuf[static_cast<size_t> ((writePos - dInt - 2) & bufMask)];

        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    /**
     * Busca alrededor de `nominalDelay` el retardo que maximiza la correlación de fase
     * con la lectura actual del grano activo (`activeDelay`), evitando cancelaciones en graves.
     */
    float findCorrelatedDelay (float activeDelay, float nominalDelay) const noexcept
    {
        const int refD = std::max (8, std::min (bufSize - corrLen - 8, static_cast<int> (std::round (activeDelay))));
        const int nomD = std::max (8 + searchRadius, std::min (bufSize - corrLen - searchRadius - 8,
                                                               static_cast<int> (std::round (nominalDelay))));

        // Evaluar energía de referencia (si es silencio, volver directo a nominalDelay)
        float refEnergy = 0.0f;
        for (int m = 0; m < corrLen; m += 4)
        {
            const float r = readSampleInt (refD + m);
            refEnergy += r * r;
        }
        if (refEnergy < 1e-7f)
            return static_cast<float> (nomD);

        // Búsqueda gruesa (paso de 6 muestras) + refinamiento fino (paso de 1 muestra)
        int bestOffset = 0;
        float bestScore = -1e30f;

        for (int off = -searchRadius; off <= searchRadius; off += 6)
        {
            const int candD = nomD + off;
            float dot = 0.0f;
            float eCand = 1e-8f;
            for (int m = 0; m < corrLen; m += 4)
            {
                const float r = readSampleInt (refD + m);
                const float c = readSampleInt (candD + m);
                dot   += r * c;
                eCand += c * c;
            }
            // Pequeña preferencia suave hacia el centro nominal para estabilidad
            const float centerBias = 1.0f - 0.06f * (static_cast<float> (std::abs (off)) / static_cast<float> (searchRadius));
            const float score = (dot / std::sqrt (eCand)) * centerBias;
            if (score > bestScore)
            {
                bestScore  = score;
                bestOffset = off;
            }
        }

        int refinedOff = bestOffset;
        for (int off = std::max (-searchRadius, bestOffset - 5);
             off <= std::min (searchRadius, bestOffset + 5); ++off)
        {
            const int candD = nomD + off;
            float dot = 0.0f;
            float eCand = 1e-8f;
            for (int m = 0; m < corrLen; m += 2)
            {
                const float r = readSampleInt (refD + m);
                const float c = readSampleInt (candD + m);
                dot   += r * c;
                eCand += c * c;
            }
            const float score = dot / std::sqrt (eCand);
            if (score > bestScore)
            {
                bestScore  = score;
                refinedOff = off;
            }
        }

        return static_cast<float> (nomD + refinedOff);
    }

    void updateAntiAliasFilter (float pitchRatio) noexcept
    {
        const double maxFc = 0.44 * sr / std::max (1.0, static_cast<double> (pitchRatio));
        const double fc    = std::max (1200.0, std::min (18000.0, maxFc));
        const double w0    = 2.0 * 3.14159265358979323846 * fc / sr;
        const double c     = std::cos (w0);
        const double s     = std::sin (w0);
        const double al    = s / (2.0 * 0.70710678);
        const double invA0 = 1.0 / (1.0 + al);

        aaB0 = static_cast<float> (((1.0 - c) * 0.5) * invA0);
        aaB1 = static_cast<float> ((1.0 - c) * invA0);
        aaB2 = aaB0;
        aaA1 = static_cast<float> ((-2.0 * c) * invA0);
        aaA2 = static_cast<float> ((1.0 - al) * invA0);
    }

    inline float processAntiAlias (float x) noexcept
    {
        const float y = aaB0 * x + aaZ1;
        aaZ1 = aaB1 * x - aaA1 * y + aaZ2;
        aaZ2 = aaB2 * x - aaA2 * y;
        return y;
    }

    double sr = 48000.0;
    int bufSize = 16384;
    int bufMask = 16383;
    int grainLen = 2160;
    int corrLen  = 672;
    int searchRadius = 816;

    std::vector<float> ringBuf;
    int writePos = 0;

    float phaseA = 0.0f;
    float delayA = 1400.0f;
    float delayB = 1400.0f;

    bool  isEnabled   = false;
    bool  pitchActive = false;
    int   semiShift   = 0;
    float centsShift  = 0.0f;
    float wetTarget   = 1.0f;
    float wetSmooth   = 0.0f;
    float ratio       = 1.0f;

    float aaB0 = 1.0f, aaB1 = 0.0f, aaB2 = 0.0f, aaA1 = 0.0f, aaA2 = 0.0f;
    float aaZ1 = 0.0f, aaZ2 = 0.0f;
};

} // namespace xvdsp
