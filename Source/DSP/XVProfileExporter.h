#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#if defined(_WIN32)
 #include <direct.h>
#else
 #include <sys/stat.h>
 #include <sys/types.h>
#endif
#include "XVBassDSP.h"

namespace xvdsp
{

struct ExportResultSummary
{
    bool        ok = false;
    std::string folderPath;
    double      am4FitR2  = 0.0;
    double      am3FitR2  = 0.0;
    double      namFitR2  = 0.0;
    int         filesWritten = 0;
};

/**
 * Motor Studio Bi-Amp v6 de Exportación y Destilación Neuronal para M-VAVE Blackbox y NAM:
 * - Arquitectura Bi-Amp Recurrente (<215 Hz Sub limpio + >430 Hz Crunch SVT/CMOS + Wiener FIR 256 taps)
 * - Calibrado post-compresor 1176 con pesos Q21 seguros ([-28, +22], cero overflow en DSP M-VAVE)
 * - Carpeta limpia de carga directa:
 *     1_Cargar_en_AMP_<slug>.am3data (6204 B, universal para todos los firmwares Blackbox)
 *     2_Cargar_en_CAB_<slug>.wav     (44.1 kHz / 24-bit / 1024 muestras exactas)
 *     3_Foto_Perillas_<slug>.png     (referencia visual 240x240 con posición real de perillas)
 *     LEEME_3_PASOS_BLACKBOX.txt
 *     Opcionales_Otros_Equipos/      (.am4data, .am2data con magic AM2\0, .nam, IRs extra)
 */
class XVProfileExporter
{
public:
    static std::string makeSafeSlug (const std::string& raw)
    {
        std::string out;
        for (unsigned char c : raw)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                out.push_back (static_cast<char> (c));
            else if (c == ' ' || c == '-' || c == '_')
            {
                if (! out.empty() && out.back() != '_')
                    out.push_back ('_');
            }
        }
        while (! out.empty() && out.back() == '_')
            out.pop_back();
        if (out.empty())
            out = "XV";
        if (out.size() > 38)
            out.resize (38);
        while (! out.empty() && out.back() == '_')
            out.pop_back();
        return out;
    }

    static std::string make14CharHardwareName (const std::string& raw)
    {
        std::string out;
        for (unsigned char c : raw)
        {
            if (c >= 32 && c <= 126 && c != '*' && c != '[' && c != ']')
                out.push_back (static_cast<char> (c));
        }
        while (! out.empty() && out.front() == ' ')
            out.erase (out.begin());
        if (out.empty())
            out = "XV Bass";
        if (out.size() > 13)
            out.resize (13);
        return out;
    }

    static std::string joinPathStr (const std::string& a, const std::string& b)
    {
        if (a.empty()) return b;
        const char last = a.back();
        if (last == '/' || last == '\\')
            return a + b;
#if defined(_WIN32)
        return a + "\\" + b;
#else
        return a + "/" + b;
#endif
    }

    static bool makeDirsRecursive (const std::string& dirPath)
    {
        if (dirPath.empty()) return false;
        std::string cur;
        cur.reserve (dirPath.size() + 2);
        for (size_t i = 0; i < dirPath.size(); ++i)
        {
            const char c = dirPath[i];
            cur.push_back (c);
            if ((c == '/' || c == '\\') && cur.size() > 1 && cur != "C:\\" && cur != "D:\\")
            {
#if defined(_WIN32)
                _mkdir (cur.c_str());
#else
                mkdir (cur.c_str(), 0755);
#endif
            }
        }
#if defined(_WIN32)
        _mkdir (dirPath.c_str());
#else
        mkdir (dirPath.c_str(), 0755);
#endif
        return true;
    }

    // -------------------------------------------------------------------------
    // Escritura WAV Mono 24-bit PCM exacto (escala M-VAVE 2^23 = 8388608)
    // -------------------------------------------------------------------------
    static bool writeWav24Mono (const std::string& path,
                                const std::vector<float>& samples,
                                uint32_t sampleRate)
    {
        std::ofstream f (path, std::ios::binary);
        if (! f)
            return false;

        const uint32_t numSamples = static_cast<uint32_t> (samples.size());
        const uint32_t dataBytes  = numSamples * 3u;
        const uint32_t riffSize   = 36u + dataBytes;
        const uint32_t byteRate   = sampleRate * 3u;
        const uint16_t blockAlign = 3u;
        const uint16_t bitsPerSmp = 24u;
        const uint16_t audioFmt   = 1u;
        const uint16_t numCh      = 1u;
        const uint32_t fmtSize    = 16u;

        f.write ("RIFF", 4);
        writeU32LE (f, riffSize);
        f.write ("WAVE", 4);
        f.write ("fmt ", 4);
        writeU32LE (f, fmtSize);
        writeU16LE (f, audioFmt);
        writeU16LE (f, numCh);
        writeU32LE (f, sampleRate);
        writeU32LE (f, byteRate);
        writeU16LE (f, blockAlign);
        writeU16LE (f, bitsPerSmp);
        f.write ("data", 4);
        writeU32LE (f, dataBytes);

        std::vector<uint8_t> raw (dataBytes);
        for (uint32_t i = 0; i < numSamples; ++i)
        {
            const double scaled = std::round (static_cast<double> (samples[i]) * 8388608.0);
            const int32_t q = static_cast<int32_t> (std::max (-8388608.0, std::min (8388607.0, scaled)));
            raw[i * 3u + 0u] = static_cast<uint8_t> (q & 0xFF);
            raw[i * 3u + 1u] = static_cast<uint8_t> ((q >> 8) & 0xFF);
            raw[i * 3u + 2u] = static_cast<uint8_t> ((q >> 16) & 0xFF);
        }
        f.write (reinterpret_cast<const char*> (raw.data()), static_cast<std::streamsize> (raw.size()));
        return f.good();
    }

    // -------------------------------------------------------------------------
    // Síntesis de IRs de Gabinete (44.1 kHz / 24-bit 1024 & 2048 pts + 48 kHz)
    // -------------------------------------------------------------------------
    static std::vector<float> buildImpulseResponse (const PedalControlSnapshot& snap,
                                                    double targetSr,
                                                    int numTaps,
                                                    bool includeToneStackAndScoop)
    {
        const float* src48 = (snap.cab == CabMode::Bass) ? kCabIrBass48k.data()
                           : ((snap.cab == CabMode::Punk) ? kCabIrPunk48k.data() : nullptr);

        constexpr int kSrcIrLen = static_cast<int> (kCabIrPunk48k.size());
        std::vector<float> ir (static_cast<size_t> (numTaps), 0.0f);
        if (src48 == nullptr)
        {
            ir[0] = 0.85f;
        }
        else if (std::fabs (targetSr - 48000.0) < 1.0)
        {
            const int copyN = std::min (numTaps, kSrcIrLen);
            for (int i = 0; i < copyN; ++i)
                ir[static_cast<size_t> (i)] = src48[i];
        }
        else
        {
            const double ratio = 48000.0 / targetSr;
            const double cutoff = 0.46 / ratio;
            const int halfWin = 16;
            const double ampComp = targetSr / 48000.0;
            const int maxOut = std::min (numTaps, static_cast<int> (std::floor (static_cast<double> (kSrcIrLen) / ratio)));

            for (int n = 0; n < maxOut; ++n)
            {
                const double srcPos = static_cast<double> (n) * ratio;
                const int center = static_cast<int> (std::floor (srcPos));
                double acc = 0.0;
                for (int k = -halfWin; k <= halfWin; ++k)
                {
                    const int idx = center + k;
                    if (idx < 0 || idx >= kSrcIrLen)
                        continue;
                    const double x = srcPos - static_cast<double> (idx);
                    const double arg = 2.0 * kPi * cutoff * x;
                    const double sinc = (std::fabs (arg) < 1e-9) ? (2.0 * cutoff) : (std::sin (arg) / (kPi * x));
                    const double wPhase = 0.5 + 0.5 * (x / static_cast<double> (halfWin + 1));
                    const double blk = 0.42 - 0.5 * std::cos (2.0 * kPi * wPhase) + 0.08 * std::cos (4.0 * kPi * wPhase);
                    acc += static_cast<double> (src48[idx]) * sinc * blk;
                }
                ir[static_cast<size_t> (n)] = static_cast<float> (acc * ampComp * ratio);
            }
        }

        if (includeToneStackAndScoop)
        {
            Biquad eqL, eqM, eqH, eqPres;
            eqL.setPeaking (targetSr, 78.0, static_cast<double> (snap.lowDb), 0.72);
            eqM.setPeaking (targetSr, 480.0, static_cast<double> (snap.midDb), 0.85);
            eqH.setHighShelf (targetSr, 1650.0, static_cast<double> (snap.highDb), 0.75);
            eqPres.setPeaking (targetSr, 1420.0, static_cast<double> (snap.presenceDb) * 0.42, 0.90);

            for (int i = 0; i < numTaps; ++i)
            {
                float s = ir[static_cast<size_t> (i)];
                s = eqL.processSample (s);
                s = eqM.processSample (s);
                s = eqH.processSample (s);
                s = eqPres.processSample (s);
                ir[static_cast<size_t> (i)] = s;
            }
        }

        const int fadeLen = std::min (64, numTaps / 8);
        for (int i = 0; i < fadeLen; ++i)
        {
            const double w = 0.5 * (1.0 + std::cos (kPi * static_cast<double> (i + 1) / static_cast<double> (fadeLen)));
            ir[static_cast<size_t> (numTaps - fadeLen + i)] *= static_cast<float> (w);
        }

        float peak = 1e-6f;
        for (float v : ir)
            peak = std::max (peak, std::fabs (v));

        const float normScale = 0.871f / peak;
        for (float& v : ir)
            v *= normScale;

        return ir;
    }

    // -------------------------------------------------------------------------
    // Generador de Señal de Excitación Realista de Bajo (escalable a cualquier duración)
    // -------------------------------------------------------------------------
    static std::vector<float> generateBassExcitation (double sr, double durationSec)
    {
        const int N = static_cast<int> (std::round (sr * durationSec));
        std::vector<float> sig (static_cast<size_t> (N), 0.0f);

        struct NoteEvt { double startS; double durS; double f0; double amp; };
        const std::array<NoteEvt, 10> notes {{
            { 0.02, 0.42,  41.203, 0.12 }, // E1 suave
            { 0.46, 0.42,  55.000, 0.28 }, // A1 medio
            { 0.90, 0.42,  73.416, 0.52 }, // D2 fuerte con púa
            { 1.34, 0.42,  97.999, 0.72 }, // G2 agresivo
            { 1.78, 0.35,  46.249, 0.42 }, // F#1
            { 2.15, 0.35,  61.735, 0.64 }, // B1
            { 2.52, 0.35,  82.407, 0.34 }, // E2
            { 2.89, 0.35, 110.000, 0.80 }, // A2 clank máximo
            { 3.26, 0.50,  41.203, 0.60 }, // E1 palm/open
            { 3.78, 0.65,  55.000, 0.45 }  // A1 sustain
        }};

        double ph1 = 0.0, ph2 = 0.0, ph3 = 0.0, ph4 = 0.0, ph5 = 0.0;
        uint32_t rng = 0x12345678u;
        const double dur = std::max (0.5, durationSec);

        for (int n = 0; n < N; ++n)
        {
            const double t = static_cast<double> (n) / sr;
            double val = 0.0;

            for (const auto& ne : notes)
            {
                if (t >= ne.startS && t < ne.startS + ne.durS)
                {
                    const double lt = t - ne.startS;
                    const double att = 1.0 - std::exp (-lt / 0.0022);
                    const double dec = std::exp (-lt / 0.32);
                    const double pickClick = std::exp (-lt / 0.0035);

                    const double w = 2.0 * kPi * ne.f0 / sr;
                    ph1 += w; ph2 += 2.0 * w; ph3 += 3.0 * w; ph4 += 4.0 * w; ph5 += 7.0 * w;

                    rng = rng * 1664525u + 1013904223u;
                    const double nz = (static_cast<double> (rng & 0xFFFFu) / 32768.0 - 1.0);

                    const double harm = 0.54 * std::sin (ph1)
                                      + 0.28 * std::sin (ph2 + 0.3)
                                      + 0.16 * std::sin (ph3 + 0.8)
                                      + 0.10 * std::sin (ph4 + 1.4)
                                      + 0.07 * (0.4 + 0.6 * pickClick) * std::sin (ph5)
                                      + 0.14 * pickClick * nz;
                    val += ne.amp * att * dec * harm;
                    break;
                }
            }

            const double chirpPh = 2.0 * kPi * 38.0 * dur / std::log (3800.0 / 38.0)
                                 * (std::pow (3800.0 / 38.0, t / dur) - 1.0);
            val += 0.11 * std::sin (chirpPh);

            sig[static_cast<size_t> (n)] = static_cast<float> (std::max (-0.95, std::min (0.95, val)));
        }
        return sig;
    }

    // -------------------------------------------------------------------------
    // Compresor FET 1176 de entrada (modela el bloque FX->Comp previo al AMP en Blackbox)
    // -------------------------------------------------------------------------
    static std::vector<float> apply1176InputComp (const std::vector<float>& rawIn, double fs)
    {
        std::vector<float> out (rawIn.size(), 0.0f);
        Biquad hpf;
        hpf.setHighPass (fs, 42.0, 0.72);
        const float compAtt = static_cast<float> (std::exp (-1.0 / (0.0038 * fs)));
        const float compRel = static_cast<float> (std::exp (-1.0 / (0.0550 * fs)));
        float compEnv = 0.0f;

        for (size_t i = 0; i < rawIn.size(); ++i)
        {
            const float tightIn = hpf.processSample (rawIn[i]);
            const float ax = std::fabs (tightIn);
            compEnv = (ax > compEnv) ? (compAtt * compEnv + (1.0f - compAtt) * ax)
                                     : (compRel * compEnv + (1.0f - compRel) * ax);
            const float tAct = std::clamp ((compEnv - 0.0012f) / (0.0070f - 0.0012f), 0.0f, 1.0f);
            const float act = tAct * tAct * (3.0f - 2.0f * tAct);
            float rawCompGain = 3.4f;
            if (compEnv > 0.042f)
            {
                const float overDb = 20.0f * std::log10 (compEnv / 0.042f);
                const float redDb  = overDb * (1.0f - 1.0f / 6.0f);
                rawCompGain = 3.4f * std::pow (10.0f, -redDb * 0.05f);
            }
            const float compGain = 1.0f + act * (rawCompGain * 1.12f - 1.0f);
            out[i] = tightIn * compGain * 0.45f;
        }
        return out;
    }

    // -------------------------------------------------------------------------
    // Resolución exacta de Sistema Lineal Simétrico (Ridge / Gauss con pivoteo)
    // -------------------------------------------------------------------------
    static std::vector<double> solveLinearRidge (std::vector<std::vector<double>> A,
                                                 std::vector<double> b,
                                                 double ridgeLambda)
    {
        const int M = static_cast<int> (b.size());
        for (int i = 0; i < M; ++i)
            A[static_cast<size_t> (i)][static_cast<size_t> (i)] += ridgeLambda;

        for (int col = 0; col < M; ++col)
        {
            int piv = col;
            double maxAbs = std::fabs (A[static_cast<size_t> (col)][static_cast<size_t> (col)]);
            for (int row = col + 1; row < M; ++row)
            {
                const double v = std::fabs (A[static_cast<size_t> (row)][static_cast<size_t> (col)]);
                if (v > maxAbs) { maxAbs = v; piv = row; }
            }
            if (maxAbs < 1e-14)
                continue;
            if (piv != col)
            {
                std::swap (A[static_cast<size_t> (col)], A[static_cast<size_t> (piv)]);
                std::swap (b[static_cast<size_t> (col)], b[static_cast<size_t> (piv)]);
            }
            const double diag = A[static_cast<size_t> (col)][static_cast<size_t> (col)];
            for (int row = col + 1; row < M; ++row)
            {
                const double factor = A[static_cast<size_t> (row)][static_cast<size_t> (col)] / diag;
                for (int k = col; k < M; ++k)
                    A[static_cast<size_t> (row)][static_cast<size_t> (k)] -= factor * A[static_cast<size_t> (col)][static_cast<size_t> (k)];
                b[static_cast<size_t> (row)] -= factor * b[static_cast<size_t> (col)];
            }
        }

        std::vector<double> x (static_cast<size_t> (M), 0.0);
        for (int i = M - 1; i >= 0; --i)
        {
            double sum = b[static_cast<size_t> (i)];
            for (int j = i + 1; j < M; ++j)
                sum -= A[static_cast<size_t> (i)][static_cast<size_t> (j)] * x[static_cast<size_t> (j)];
            const double diag = A[static_cast<size_t> (i)][static_cast<size_t> (i)];
            x[static_cast<size_t> (i)] = (std::fabs (diag) > 1e-14) ? (sum / diag) : 0.0;
        }
        return x;
    }

    // -------------------------------------------------------------------------
    // Cabecera binaria de 60 bytes exacta SincoANN / M-VAVE (.am2data / .am3data / .am4data)
    // -------------------------------------------------------------------------
    static std::array<uint8_t, 60> buildSincoHeader (const char* magic3,
                                                     const std::string& hwName14,
                                                     uint16_t sig = 0x9664,
                                                     uint16_t ctrl = 0x1E3C)
    {
        std::array<uint8_t, 60> hdr {};
        hdr.fill (0);
        if (magic3 != nullptr)
        {
            hdr[0] = static_cast<uint8_t> (magic3[0]);
            hdr[1] = static_cast<uint8_t> (magic3[1]);
            hdr[2] = static_cast<uint8_t> (magic3[2]);
            hdr[3] = 0;
        }
        const std::string clean = make14CharHardwareName (hwName14);
        for (size_t i = 0; i < std::min<size_t> (14, clean.size()); ++i)
            hdr[4 + i] = static_cast<uint8_t> (clean[i]);

        auto putU16 = [&hdr] (size_t off, uint16_t v)
        {
            hdr[off + 0] = static_cast<uint8_t> (v & 0xFFu);
            hdr[off + 1] = static_cast<uint8_t> ((v >> 8) & 0xFFu);
        };
        auto putF32 = [&hdr] (size_t off, float v)
        {
            uint32_t bits = 0;
            std::memcpy (&bits, &v, 4);
            hdr[off + 0] = static_cast<uint8_t> (bits & 0xFFu);
            hdr[off + 1] = static_cast<uint8_t> ((bits >> 8) & 0xFFu);
            hdr[off + 2] = static_cast<uint8_t> ((bits >> 16) & 0xFFu);
            hdr[off + 3] = static_cast<uint8_t> ((bits >> 24) & 0xFFu);
        };

        putU16 (18, sig);
        putU16 (20, ctrl);
        putU16 (22, 0u);

        putF32 (24,   80.0f); putF32 (28, -12.0f); putF32 (32, 12.0f);
        putF32 (36,  500.0f); putF32 (40, -12.0f); putF32 (44, 12.0f);
        putF32 (48, 2600.0f); putF32 (52, -12.0f); putF32 (56, 12.0f);
        return hdr;
    }

    // -------------------------------------------------------------------------
    // Destilación Studio Bi-Amp a .am4data (AM4\0, 7868 bytes, 44.1 kHz)
    // -------------------------------------------------------------------------
    static double distillAndSaveAM4 (const std::string& outPath,
                                     const std::string& modelName,
                                     const PedalControlSnapshot& userSnap,
                                     const std::vector<float>& u44k,
                                     const std::vector<float>& yTarget44k)
    {
        constexpr int F1 = 512, F2 = 512, H = 16, Kfit = 256;
        const double sr = 44100.0;
        const int N = static_cast<int> (u44k.size());

        std::vector<float> hPre (F1, 0.0f);
        {
            Biquad scoop, svtMid, presPk;
            scoop.setPeaking (sr, 350.0, -5.0, 0.90);
            svtMid.setPeaking (sr, 1420.0, 5.2 + static_cast<double> (userSnap.presenceDb) * 1.05, 0.82);
            presPk.setPeaking (sr, 2250.0, static_cast<double> (userSnap.presenceDb) * 0.70, 1.05);
            for (int i = 0; i < F1; ++i)
            {
                float s = (i == 0) ? 1.0f : 0.0f;
                s = presPk.processSample (svtMid.processSample (scoop.processSample (s)));
                const double win = (i > F1 - 48) ? 0.5 * (1.0 + std::cos (kPi * (i - (F1 - 48)) / 48.0)) : 1.0;
                hPre[static_cast<size_t> (i)] = static_cast<float> (s * win * 0.48);
            }
        }
        std::vector<float> fir1 (F1, 0.0f);
        for (int k = 0; k < F1; ++k)
            fir1[static_cast<size_t> (F1 - 1 - k)] = hPre[static_cast<size_t> (k)];

        std::vector<float> w_ih (3 * H, 0.0f), w_hh (3 * H * H, 0.0f), b_ih (3 * H, 0.0f), b_hh (3 * H, 0.0f);
        const float dn  = std::clamp (userSnap.drive / 10.0f, 0.0f, 1.0f);
        const float drv = 0.70f + 1.60f * std::pow (dn, 1.3f);

        // Neuronas 0..3: Crossover Bi-Amp (430Hz LPF, 215Hz Sub, 95Hz Deep Sub, Direct Clean)
        b_ih[0 * H + 0] = -6.0f; b_ih[1 * H + 0] = 2.7604f; w_ih[2 * H + 0] = 0.80f;
        b_ih[0 * H + 1] = -6.0f; b_ih[1 * H + 1] = 3.4690f; w_ih[2 * H + 1] = 0.75f;
        b_ih[0 * H + 2] = -6.0f; b_ih[1 * H + 2] = 4.2910f; w_ih[2 * H + 2] = 0.22f;
        b_ih[0 * H + 3] = -6.0f; b_ih[1 * H + 3] = -5.500f; w_ih[2 * H + 3] = 0.40f;

        const float rGate = 1.0f / (1.0f + std::exp (-5.5f));
        const std::array<float, 8> driveGains {{ 0.80f, 1.45f, 2.50f, 4.10f, 6.60f, 10.2f, 15.0f, 21.5f }};
        const std::array<float, 8> asymB      {{ 0.00f, 0.08f, -0.08f, 0.14f, -0.14f, 0.20f, -0.20f, 0.10f }};
        for (int m = 0; m < 8; ++m)
        {
            const int j = 4 + m;
            const float g = std::min (21.5f, driveGains[static_cast<size_t> (m)] * drv);
            b_ih[0 * H + j] = 5.5f;
            b_ih[1 * H + j] = -5.2f;
            w_ih[2 * H + j] = g;
            b_ih[2 * H + j] = asymB[static_cast<size_t> (m)];
            w_hh[2 * H * H + j * H + 0] = std::max (-27.8f, -g / (0.80f * rGate));
        }
        for (int m = 0; m < 4; ++m)
        {
            const int j = 12 + m;
            const float g = std::min (20.0f, (1.3f + static_cast<float> (m) * 1.8f) * drv);
            b_ih[0 * H + j] = 5.5f;
            b_ih[1 * H + j] = -4.5f;
            w_ih[2 * H + j] = g;
            b_ih[2 * H + j] = (m % 2 == 0) ? 0.12f : -0.12f;
            w_hh[2 * H * H + j * H + 1] = std::max (-27.8f, -g / (0.75f * rGate));
        }

        auto sigm = [] (float v) -> float { return 1.0f / (1.0f + std::exp (-std::clamp (v, -20.0f, 20.0f))); };
        std::vector<float> x1 (static_cast<size_t> (N), 0.0f);
        for (int n = 0; n < N; ++n)
        {
            double s = 0.0;
            const int kMax = std::min (n, F1 - 1);
            for (int lag = 0; lag <= kMax; ++lag)
                s += static_cast<double> (hPre[static_cast<size_t> (lag)]) * static_cast<double> (u44k[static_cast<size_t> (n - lag)]);
            x1[static_cast<size_t> (n)] = static_cast<float> (s);
        }

        std::array<std::vector<float>, H> hTraj;
        for (int j = 0; j < H; ++j)
            hTraj[static_cast<size_t> (j)].assign (static_cast<size_t> (N), 0.0f);
        std::array<float, H> hS {};
        hS.fill (0.0f);

        for (int n = 0; n < N; ++n)
        {
            const float xv = x1[static_cast<size_t> (n)];
            std::array<float, H> hNext {};
            for (int j = 0; j < H; ++j)
            {
                float hr = b_hh[0 * H + j], hz = b_hh[1 * H + j], hn = b_hh[2 * H + j];
                for (int k = 0; k < H; ++k)
                {
                    hr += w_hh[0 * H * H + j * H + k] * hS[static_cast<size_t> (k)];
                    hz += w_hh[1 * H * H + j * H + k] * hS[static_cast<size_t> (k)];
                    hn += w_hh[2 * H * H + j * H + k] * hS[static_cast<size_t> (k)];
                }
                const float r = sigm (w_ih[0 * H + j] * xv + b_ih[0 * H + j] + hr);
                const float z = sigm (w_ih[1 * H + j] * xv + b_ih[1 * H + j] + hz);
                const float g = std::tanh (w_ih[2 * H + j] * xv + b_ih[2 * H + j] + r * hn);
                hNext[static_cast<size_t> (j)] = (1.0f - z) * g + z * hS[static_cast<size_t> (j)];
                hTraj[static_cast<size_t> (j)][static_cast<size_t> (n)] = hNext[static_cast<size_t> (j)];
            }
            hS = hNext;
        }

        std::vector<float> hPost (F2, 0.0f);
        hPost[0] = 1.0f;
        std::vector<float> ln_w (H, 0.0f);
        const int warm = std::min (1500, N / 4);
        std::array<std::vector<float>, H> basis;
        for (int j = 0; j < H; ++j)
            basis[static_cast<size_t> (j)].assign (static_cast<size_t> (N), 0.0f);

        for (int iter = 0; iter < 4; ++iter)
        {
            for (int j = 0; j < H; ++j)
            {
                for (int n = warm; n < N; n += 2)
                {
                    double s = 0.0;
                    const int kMax = std::min (n, Kfit - 1);
                    for (int lag = 0; lag <= kMax; ++lag)
                        s += static_cast<double> (hPost[static_cast<size_t> (lag)])
                           * static_cast<double> (hTraj[static_cast<size_t> (j)][static_cast<size_t> (n - lag)]);
                    basis[static_cast<size_t> (j)][static_cast<size_t> (n)] = static_cast<float> (s);
                }
            }

            std::vector<std::vector<double>> XtX (H, std::vector<double> (H, 0.0));
            std::vector<double> Xty (H, 0.0);
            for (int n = warm; n < N; n += 2)
            {
                const double yt = static_cast<double> (yTarget44k[static_cast<size_t> (n)]);
                for (int i = 0; i < H; ++i)
                {
                    const double bi = static_cast<double> (basis[static_cast<size_t> (i)][static_cast<size_t> (n)]);
                    Xty[static_cast<size_t> (i)] += bi * yt;
                    for (int j = i; j < H; ++j)
                        XtX[static_cast<size_t> (i)][static_cast<size_t> (j)] += bi * static_cast<double> (basis[static_cast<size_t> (j)][static_cast<size_t> (n)]);
                }
            }
            double tr = 0.0;
            for (int i = 0; i < H; ++i)
            {
                tr += XtX[static_cast<size_t> (i)][static_cast<size_t> (i)];
                for (int j = 0; j < i; ++j)
                    XtX[static_cast<size_t> (i)][static_cast<size_t> (j)] = XtX[static_cast<size_t> (j)][static_cast<size_t> (i)];
            }

            const auto wSol = solveLinearRidge (XtX, Xty, std::max (1e-6, 2e-4 * tr / static_cast<double> (H)));
            float nrm = 0.0f;
            for (int j = 0; j < H; ++j)
            {
                ln_w[static_cast<size_t> (j)] = static_cast<float> (wSol[static_cast<size_t> (j)]);
                nrm += ln_w[static_cast<size_t> (j)] * ln_w[static_cast<size_t> (j)];
            }
            nrm = std::sqrt (nrm);
            if (nrm > 1e-6f)
                for (float& v : ln_w) v *= (1.8f / nrm);

            std::vector<double> zSeq (static_cast<size_t> (N), 0.0);
            for (int n = 0; n < N; ++n)
            {
                double s = 0.0;
                for (int j = 0; j < H; ++j)
                    s += static_cast<double> (ln_w[static_cast<size_t> (j)]) * static_cast<double> (hTraj[static_cast<size_t> (j)][static_cast<size_t> (n)]);
                zSeq[static_cast<size_t> (n)] = s;
            }

            std::vector<std::vector<double>> ZtZ (Kfit, std::vector<double> (Kfit, 0.0));
            std::vector<double> Zty (Kfit, 0.0);
            for (int lag = 0; lag < Kfit; ++lag)
            {
                double r0 = 0.0, ry = 0.0;
                for (int n = warm; n < N; n += 2)
                {
                    r0 += zSeq[static_cast<size_t> (n)] * zSeq[static_cast<size_t> (n - lag)];
                    ry += zSeq[static_cast<size_t> (n - lag)] * static_cast<double> (yTarget44k[static_cast<size_t> (n)]);
                }
                Zty[static_cast<size_t> (lag)] = ry;
                for (int d = 0; d + lag < Kfit; ++d)
                {
                    ZtZ[static_cast<size_t> (d)][static_cast<size_t> (d + lag)] = r0;
                    ZtZ[static_cast<size_t> (d + lag)][static_cast<size_t> (d)] = r0;
                }
            }
            const auto hSol = solveLinearRidge (ZtZ, Zty, std::max (1e-6, 6e-5 * ZtZ[0][0]));
            std::fill (hPost.begin(), hPost.end(), 0.0f);
            for (int k = 0; k < Kfit; ++k)
            {
                const double win = (k > Kfit - 24) ? 0.5 * (1.0 + std::cos (kPi * (k - (Kfit - 24)) / 24.0)) : 1.0;
                hPost[static_cast<size_t> (k)] = static_cast<float> (hSol[static_cast<size_t> (k)] * win);
            }
        }

        // Balancear ln_w y hPost para garantizar margen holgado Q21 (|w| <= 24.0)
        float maxH = 0.0f;
        for (float v : hPost) maxH = std::max (maxH, std::fabs (v));
        if (maxH > 20.0f)
        {
            const float sc = 20.0f / maxH;
            for (float& v : hPost) v *= sc;
            for (float& v : ln_w)  v /= sc;
        }

        std::vector<float> fir2 (F2, 0.0f);
        for (int k = 0; k < F2; ++k)
            fir2[static_cast<size_t> (F2 - 1 - k)] = hPost[static_cast<size_t> (k)];

        std::vector<double> zEval (static_cast<size_t> (N), 0.0);
        for (int n = 0; n < N; ++n)
        {
            double s = 0.0;
            for (int j = 0; j < H; ++j)
                s += static_cast<double> (ln_w[static_cast<size_t> (j)]) * static_cast<double> (hTraj[static_cast<size_t> (j)][static_cast<size_t> (n)]);
            zEval[static_cast<size_t> (n)] = s;
        }

        double ssRes = 0.0, ssTot = 0.0, meanY = 0.0;
        for (int n = warm; n < N; ++n) meanY += yTarget44k[static_cast<size_t> (n)];
        meanY /= static_cast<double> (N - warm);
        for (int n = warm; n < N; ++n)
        {
            double pred = 0.0;
            const int kMax = std::min (n, Kfit - 1);
            for (int lag = 0; lag <= kMax; ++lag)
                pred += static_cast<double> (hPost[static_cast<size_t> (lag)]) * zEval[static_cast<size_t> (n - lag)];
            const double diff = static_cast<double> (yTarget44k[static_cast<size_t> (n)]) - pred;
            const double dy   = static_cast<double> (yTarget44k[static_cast<size_t> (n)]) - meanY;
            ssRes += diff * diff;
            ssTot += dy * dy;
        }
        const double r2 = (ssTot > 1e-9) ? (1.0 - ssRes / ssTot) : 0.0;

        std::vector<float> allWeights;
        allWeights.reserve (1952);
        allWeights.insert (allWeights.end(), fir1.begin(), fir1.end());
        allWeights.insert (allWeights.end(), w_ih.begin(), w_ih.end());
        allWeights.insert (allWeights.end(), w_hh.begin(), w_hh.end());
        allWeights.insert (allWeights.end(), b_ih.begin(), b_ih.end());
        allWeights.insert (allWeights.end(), b_hh.begin(), b_hh.end());
        allWeights.insert (allWeights.end(), ln_w.begin(), ln_w.end());
        allWeights.insert (allWeights.end(), fir2.begin(), fir2.end());

        const auto hdr = buildSincoHeader ("AM4", modelName, 0x9664, 0x1E3C);
        std::ofstream f (outPath, std::ios::binary);
        if (f)
        {
            f.write (reinterpret_cast<const char*> (hdr.data()), 60);
            f.write (reinterpret_cast<const char*> (allWeights.data()), 7808);
        }
        return r2;
    }

    // -------------------------------------------------------------------------
    // Destilación Studio Bi-Amp a .am3data (6204B, Universal Blackbox) y .am2data (AM2\0)
    // -------------------------------------------------------------------------
    static double distillAndSaveAM3AndAM2 (const std::string& am3Path,
                                           const std::string& am2DataPath,
                                           const std::string& am2RawPath,
                                           const std::string& modelName,
                                           const PedalControlSnapshot& userSnap,
                                           const std::vector<float>& u44k,
                                           const std::vector<float>& yTarget44k)
    {
        constexpr int F1 = 256, F2 = 256, KC = 64, H = 16;
        const double sr = 44100.0;
        const int N = static_cast<int> (u44k.size());

        std::vector<float> cnn_w (3 * KC, 0.0f);
        cnn_w[2 * KC + (KC - 1)] =  1.00f;
        cnn_w[0 * KC + (KC - 1)] =  0.30f;
        cnn_w[0 * KC + (KC - 2)] = -0.30f;
        cnn_w[1 * KC + (KC - 1)] =  0.30f;
        cnn_w[1 * KC + (KC - 2)] =  0.15f;

        std::vector<float> hPre (F1, 0.0f);
        {
            Biquad scoop, svtMid, presPk;
            scoop.setPeaking (sr, 350.0, -5.0, 0.90);
            svtMid.setPeaking (sr, 1420.0, 5.2 + static_cast<double> (userSnap.presenceDb) * 1.05, 0.82);
            presPk.setPeaking (sr, 2250.0, static_cast<double> (userSnap.presenceDb) * 0.70, 1.05);
            for (int i = 0; i < F1; ++i)
            {
                float s = (i == 0) ? 1.0f : 0.0f;
                s = presPk.processSample (svtMid.processSample (scoop.processSample (s)));
                const double win = (i > F1 - 40) ? 0.5 * (1.0 + std::cos (kPi * (i - (F1 - 40)) / 40.0)) : 1.0;
                hPre[static_cast<size_t> (i)] = static_cast<float> (s * win * 0.48);
            }
        }
        std::vector<float> fir1 (F1, 0.0f);
        for (int k = 0; k < F1; ++k)
            fir1[static_cast<size_t> (F1 - 1 - k)] = hPre[static_cast<size_t> (k)];

        std::vector<float> w_ih (3 * H, 0.0f), w_hh (3 * H * H, 0.0f);
        const float dn  = std::clamp (userSnap.drive / 10.0f, 0.0f, 1.0f);
        const float drv = 0.70f + 1.60f * std::pow (dn, 1.3f);

        // Neurona 0: 430 Hz LPF para el Crossover Bi-Amp
        w_ih[2 * H + 0] = 0.24f;
        w_hh[2 * H * H + 0 * H + 0] = 1.762f;
        const float h0DcGain = (0.5f * 0.24f) / (1.0f - (0.5f + 0.25f * 1.762f));

        // Neurona 1: 215 Hz Sub-Bass limpio
        w_ih[2 * H + 1] = 0.16f;
        w_hh[2 * H * H + 1 * H + 1] = 1.879f;
        const float h1DcGain = (0.5f * 0.16f) / (1.0f - (0.5f + 0.25f * 1.879f));

        // Neurona 2: 95 Hz Deep Sub SVT
        w_ih[2 * H + 2] = 0.08f;
        w_hh[2 * H * H + 2 * H + 2] = 1.946f;

        // Neurona 3: Canal limpio instantáneo sin retardo
        w_ih[2 * H + 3] = 0.42f;
        w_hh[2 * H * H + 3 * H + 3] = -1.96f;

        // Neuronas 4..11: Escalera de saturación pasa-altos (>430 Hz) restando Neurona 0
        const std::array<float, 8> driveGains {{ 0.80f, 1.45f, 2.50f, 4.10f, 6.60f, 10.2f, 15.0f, 21.5f }};
        for (int m = 0; m < 8; ++m)
        {
            const int j = 4 + m;
            const float g = std::min (21.8f, driveGains[static_cast<size_t> (m)] * drv);
            w_ih[2 * H + j] = g;
            w_hh[2 * H * H + j * H + 0] = -2.0f * g / h0DcGain;
            w_hh[2 * H * H + j * H + j] = -1.70f;
            w_ih[1 * H + j] = (m % 2 == 0) ? (0.80f * drv) : (-0.80f * drv);
        }

        // Neuronas 12..15: Gruñido valvular medio (>215 Hz) con asimetría de 2do armónico
        for (int m = 0; m < 4; ++m)
        {
            const int j = 12 + m;
            const float g = std::min (20.0f, (1.3f + static_cast<float> (m) * 1.8f) * drv);
            w_ih[2 * H + j] = g;
            w_hh[2 * H * H + j * H + 1] = -2.0f * g / h1DcGain;
            w_hh[2 * H * H + j * H + j] = -1.55f;
            w_ih[1 * H + j] = (m % 2 == 0) ? 1.25f : -1.25f;
        }

        auto sigm = [] (float v) -> float { return 1.0f / (1.0f + std::exp (-std::clamp (v, -20.0f, 20.0f))); };

        std::vector<float> xCnn (static_cast<size_t> (N), 0.0f);
        for (int n = 0; n < N; ++n)
        {
            const float x0  = u44k[static_cast<size_t> (n)];
            const float xm1 = (n > 0) ? u44k[static_cast<size_t> (n - 1)] : 0.0f;
            const float c0  = 0.30f * x0 - 0.30f * xm1;
            const float c1  = 0.30f * x0 + 0.15f * xm1;
            xCnn[static_cast<size_t> (n)] = c0 * c1 + x0;
        }

        std::vector<float> xAsinh (static_cast<size_t> (N), 0.0f);
        for (int n = 0; n < N; ++n)
        {
            double s = 0.0;
            const int kMax = std::min (n, F1 - 1);
            for (int lag = 0; lag <= kMax; ++lag)
                s += static_cast<double> (hPre[static_cast<size_t> (lag)]) * static_cast<double> (xCnn[static_cast<size_t> (n - lag)]);
            xAsinh[static_cast<size_t> (n)] = static_cast<float> (std::asinh (s));
        }

        std::array<std::vector<float>, H> hTraj;
        for (int j = 0; j < H; ++j)
            hTraj[static_cast<size_t> (j)].assign (static_cast<size_t> (N), 0.0f);
        std::array<float, H> hS {};
        hS.fill (0.0f);

        for (int n = 0; n < N; ++n)
        {
            const float xv = xAsinh[static_cast<size_t> (n)];
            std::array<float, H> hNext {};
            for (int j = 0; j < H; ++j)
            {
                float hr = 0.0f, hz = 0.0f, hn = 0.0f;
                for (int k = 0; k < H; ++k)
                {
                    hr += w_hh[0 * H * H + j * H + k] * hS[static_cast<size_t> (k)];
                    hz += w_hh[1 * H * H + j * H + k] * hS[static_cast<size_t> (k)];
                    hn += w_hh[2 * H * H + j * H + k] * hS[static_cast<size_t> (k)];
                }
                const float r = sigm (w_ih[0 * H + j] * xv + hr);
                const float z = sigm (w_ih[1 * H + j] * xv + hz);
                const float g = std::tanh (w_ih[2 * H + j] * xv + r * hn);
                hNext[static_cast<size_t> (j)] = (1.0f - z) * g + z * hS[static_cast<size_t> (j)];
                hTraj[static_cast<size_t> (j)][static_cast<size_t> (n)] = hNext[static_cast<size_t> (j)];
            }
            hS = hNext;
        }

        std::vector<float> hPost (F2, 0.0f);
        hPost[0] = 1.0f;
        std::vector<float> ln_w (H, 0.0f);
        const int warm = std::min (1500, N / 4);
        std::array<std::vector<float>, H> basis;
        for (int j = 0; j < H; ++j)
            basis[static_cast<size_t> (j)].assign (static_cast<size_t> (N), 0.0f);

        for (int iter = 0; iter < 5; ++iter)
        {
            for (int j = 0; j < H; ++j)
            {
                for (int n = warm; n < N; n += 2)
                {
                    double s = 0.0;
                    const int kMax = std::min (n, F2 - 1);
                    for (int lag = 0; lag <= kMax; ++lag)
                        s += static_cast<double> (hPost[static_cast<size_t> (lag)])
                           * static_cast<double> (hTraj[static_cast<size_t> (j)][static_cast<size_t> (n - lag)]);
                    basis[static_cast<size_t> (j)][static_cast<size_t> (n)] = static_cast<float> (s);
                }
            }

            std::vector<std::vector<double>> XtX (H, std::vector<double> (H, 0.0));
            std::vector<double> Xty (H, 0.0);
            for (int n = warm; n < N; n += 2)
            {
                const double yt = static_cast<double> (yTarget44k[static_cast<size_t> (n)]);
                for (int i = 0; i < H; ++i)
                {
                    const double bi = static_cast<double> (basis[static_cast<size_t> (i)][static_cast<size_t> (n)]);
                    Xty[static_cast<size_t> (i)] += bi * yt;
                    for (int j = i; j < H; ++j)
                        XtX[static_cast<size_t> (i)][static_cast<size_t> (j)] += bi * static_cast<double> (basis[static_cast<size_t> (j)][static_cast<size_t> (n)]);
                }
            }
            double tr = 0.0;
            for (int i = 0; i < H; ++i)
            {
                tr += XtX[static_cast<size_t> (i)][static_cast<size_t> (i)];
                for (int j = 0; j < i; ++j)
                    XtX[static_cast<size_t> (i)][static_cast<size_t> (j)] = XtX[static_cast<size_t> (j)][static_cast<size_t> (i)];
            }

            const auto wSol = solveLinearRidge (XtX, Xty, std::max (1e-6, 2e-4 * tr / static_cast<double> (H)));
            float nrm = 0.0f;
            for (int j = 0; j < H; ++j)
            {
                ln_w[static_cast<size_t> (j)] = static_cast<float> (wSol[static_cast<size_t> (j)]);
                nrm += ln_w[static_cast<size_t> (j)] * ln_w[static_cast<size_t> (j)];
            }
            nrm = std::sqrt (nrm);
            if (nrm > 1e-6f)
                for (float& v : ln_w) v *= (1.8f / nrm);

            std::vector<double> zSeq (static_cast<size_t> (N), 0.0);
            for (int n = 0; n < N; ++n)
            {
                double s = 0.0;
                for (int j = 0; j < H; ++j)
                    s += static_cast<double> (ln_w[static_cast<size_t> (j)]) * static_cast<double> (hTraj[static_cast<size_t> (j)][static_cast<size_t> (n)]);
                zSeq[static_cast<size_t> (n)] = s;
            }

            std::vector<std::vector<double>> ZtZ (F2, std::vector<double> (F2, 0.0));
            std::vector<double> Zty (F2, 0.0);
            for (int lag = 0; lag < F2; ++lag)
            {
                double r0 = 0.0, ry = 0.0;
                for (int n = warm; n < N; n += 2)
                {
                    r0 += zSeq[static_cast<size_t> (n)] * zSeq[static_cast<size_t> (n - lag)];
                    ry += zSeq[static_cast<size_t> (n - lag)] * static_cast<double> (yTarget44k[static_cast<size_t> (n)]);
                }
                Zty[static_cast<size_t> (lag)] = ry;
                for (int d = 0; d + lag < F2; ++d)
                {
                    ZtZ[static_cast<size_t> (d)][static_cast<size_t> (d + lag)] = r0;
                    ZtZ[static_cast<size_t> (d + lag)][static_cast<size_t> (d)] = r0;
                }
            }
            const auto hSol = solveLinearRidge (ZtZ, Zty, std::max (1e-6, 6e-5 * ZtZ[0][0]));
            for (int k = 0; k < F2; ++k)
            {
                const double win = (k > F2 - 24) ? 0.5 * (1.0 + std::cos (kPi * (k - (F2 - 24)) / 24.0)) : 1.0;
                hPost[static_cast<size_t> (k)] = static_cast<float> (hSol[static_cast<size_t> (k)] * win);
            }
        }

        // Balancear ln_w y hPost para garantizar margen holgado Q21 (|w| <= 24.0)
        float maxH = 0.0f;
        for (float v : hPost) maxH = std::max (maxH, std::fabs (v));
        if (maxH > 20.0f)
        {
            const float sc = 20.0f / maxH;
            for (float& v : hPost) v *= sc;
            for (float& v : ln_w)  v /= sc;
        }

        std::vector<float> fir2 (F2, 0.0f);
        for (int k = 0; k < F2; ++k)
            fir2[static_cast<size_t> (F2 - 1 - k)] = hPost[static_cast<size_t> (k)];

        std::vector<double> zEval (static_cast<size_t> (N), 0.0);
        for (int n = 0; n < N; ++n)
        {
            double s = 0.0;
            for (int j = 0; j < H; ++j)
                s += static_cast<double> (ln_w[static_cast<size_t> (j)]) * static_cast<double> (hTraj[static_cast<size_t> (j)][static_cast<size_t> (n)]);
            zEval[static_cast<size_t> (n)] = s;
        }

        double ssRes = 0.0, ssTot = 0.0, meanY = 0.0;
        for (int n = warm; n < N; ++n) meanY += yTarget44k[static_cast<size_t> (n)];
        meanY /= static_cast<double> (N - warm);
        for (int n = warm; n < N; ++n)
        {
            double pred = 0.0;
            const int kMax = std::min (n, F2 - 1);
            for (int lag = 0; lag <= kMax; ++lag)
                pred += static_cast<double> (hPost[static_cast<size_t> (lag)]) * zEval[static_cast<size_t> (n - lag)];
            const double diff = static_cast<double> (yTarget44k[static_cast<size_t> (n)]) - pred;
            const double dy   = static_cast<double> (yTarget44k[static_cast<size_t> (n)]) - meanY;
            ssRes += diff * diff;
            ssTot += dy * dy;
        }
        const double r2 = (ssTot > 1e-9) ? (1.0 - ssRes / ssTot) : 0.0;

        // Orden exacto AM3 (1536 floats = 6144 bytes): fir1(256), w_ih(48), w_hh(768), ln(16), fir2(256), cnn(192)
        std::vector<float> allWeights;
        allWeights.reserve (1536);
        allWeights.insert (allWeights.end(), fir1.begin(), fir1.end());
        allWeights.insert (allWeights.end(), w_ih.begin(), w_ih.end());
        allWeights.insert (allWeights.end(), w_hh.begin(), w_hh.end());
        allWeights.insert (allWeights.end(), ln_w.begin(), ln_w.end());
        allWeights.insert (allWeights.end(), fir2.begin(), fir2.end());
        allWeights.insert (allWeights.end(), cnn_w.begin(), cnn_w.end());

        {
            const auto hdr3 = buildSincoHeader ("AM3", modelName, 0x9664, 0x1E3C);
            std::ofstream f3 (am3Path, std::ios::binary);
            if (f3)
            {
                f3.write (reinterpret_cast<const char*> (hdr3.data()), 60);
                f3.write (reinterpret_cast<const char*> (allWeights.data()), 6144);
            }
        }
        {
            if (! am2RawPath.empty())
            {
                std::ofstream f2raw (am2RawPath, std::ios::binary);
                if (f2raw)
                    f2raw.write (reinterpret_cast<const char*> (allWeights.data()), 6144);
            }
            if (! am2DataPath.empty())
            {
                const auto hdr2 = buildSincoHeader ("AM2", modelName, 0x9664, 0x1E3C);
                std::ofstream f2dat (am2DataPath, std::ios::binary);
                if (f2dat)
                {
                    f2dat.write (reinterpret_cast<const char*> (hdr2.data()), 60);
                    f2dat.write (reinterpret_cast<const char*> (allWeights.data()), 6144);
                }
            }
        }
        return r2;
    }

    // -------------------------------------------------------------------------
    // Destilación Neuronal Directa a Neural Amp Modeler .nam (v0.5.4 JSON LSTM)
    // -------------------------------------------------------------------------
    static double distillAndSaveNAM (const std::string& namPath,
                                     const std::string& modelName,
                                     const PedalControlSnapshot& userSnap)
    {
        constexpr int H = 32;
        const double sr = 48000.0;

        const auto u48 = generateBassExcitation (sr, 1.05);
        const int N = static_cast<int> (u48.size());
        std::vector<float> yTarget (static_cast<size_t> (N), 0.0f);
        {
            XVChannelProcessor refCh;
            refCh.prepare (sr);
            PedalControlSnapshot preampSnap = userSnap;
            preampSnap.cab = CabMode::Clean;
            preampSnap.bypassed = false;
            preampSnap.tunerMuted = false;
            preampSnap.pitchEnabled = false;
            refCh.updateParameters (preampSnap);
            for (int n = 0; n < N; ++n)
                yTarget[static_cast<size_t> (n)] = refCh.processSample (u48[static_cast<size_t> (n)]);
        }

        std::vector<float> W ((4 * H) * (1 + H), 0.0f);
        std::vector<float> B (4 * H, 0.0f);
        const float drvScale = 0.55f + (userSnap.drive / 10.0f) * 1.45f;

        auto setW = [&] (int row, int col, float val) { W[static_cast<size_t> (row * (1 + H) + col)] = val; };

        for (int j = 0; j < H; ++j)
        {
            if (j < 8)
            {
                const std::array<float, 8> poles {{ 0.992f, 0.982f, 0.968f, 0.945f, 0.910f, 0.850f, 0.760f, 0.620f }};
                const float pole = poles[static_cast<size_t> (j)];
                B[0 * H + j] = std::log ((1.0f - pole) / pole);
                B[1 * H + j] = std::log (pole / (1.0f - pole));
                setW (2 * H + j, 0, 0.65f);
                B[3 * H + j] = 4.0f;
            }
            else if (j < 20)
            {
                const int k = j - 8;
                const float gGain = 0.7f + 22.0f * drvScale * std::pow (static_cast<float> (k) / 11.0f, 1.55f);
                B[0 * H + j] = 4.0f;
                B[1 * H + j] = (k % 3 == 0) ? -0.6f : -5.0f;
                setW (2 * H + j, 0, gGain);
                B[2 * H + j] = ((k % 2 == 0) ? 0.11f : -0.11f);
                B[3 * H + j] = 3.5f;
            }
            else if (j < 24)
            {
                const int k = j - 20;
                B[0 * H + j] = 2.4f;
                B[1 * H + j] = 0.6f;
                setW (2 * H + j, 0, 3.8f * drvScale * (1.0f + 0.35f * static_cast<float> (k)));
                setW (2 * H + j, 1 + j, -0.74f + 0.09f * static_cast<float> (k));
                B[3 * H + j] = 3.2f;
            }
            else
            {
                const int k = j - 24;
                B[0 * H + j] = 5.0f;
                B[1 * H + j] = -6.0f;
                if (k == 0)
                    setW (2 * H + j, 0, 0.85f);
                else
                    setW (2 * H + j, 1 + (j - 1), 1.35f);
                B[3 * H + j] = 5.0f;
            }
        }

        auto sigm = [] (float v) -> float { return 1.0f / (1.0f + std::exp (-std::max (-20.0f, std::min (20.0f, v)))); };
        std::vector<std::array<float, H>> hHist (static_cast<size_t> (N));
        std::array<float, H> h {}, c {};
        h.fill (0.0f); c.fill (0.0f);

        for (int n = 0; n < N; ++n)
        {
            const float x = u48[static_cast<size_t> (n)];
            std::array<float, 4 * H> ifgo {};
            for (int r = 0; r < 4 * H; ++r)
            {
                float acc = B[static_cast<size_t> (r)] + W[static_cast<size_t> (r * (1 + H) + 0)] * x;
                for (int k = 0; k < H; ++k)
                    acc += W[static_cast<size_t> (r * (1 + H) + 1 + k)] * h[static_cast<size_t> (k)];
                ifgo[static_cast<size_t> (r)] = acc;
            }
            for (int j = 0; j < H; ++j)
            {
                const float ig = sigm (ifgo[static_cast<size_t> (0 * H + j)]);
                const float fg = sigm (ifgo[static_cast<size_t> (1 * H + j)]);
                const float gg = std::tanh (ifgo[static_cast<size_t> (2 * H + j)]);
                const float og = sigm (ifgo[static_cast<size_t> (3 * H + j)]);
                c[static_cast<size_t> (j)] = fg * c[static_cast<size_t> (j)] + ig * gg;
                h[static_cast<size_t> (j)] = og * std::tanh (c[static_cast<size_t> (j)]);
            }
            hHist[static_cast<size_t> (n)] = h;
        }

        const int M = H + 1;
        const int warm = 1200;
        std::vector<std::vector<double>> XtX (M, std::vector<double> (M, 0.0));
        std::vector<double> Xty (M, 0.0);
        for (int n = warm; n < N; ++n)
        {
            const double yt = static_cast<double> (yTarget[static_cast<size_t> (n)]);
            for (int i = 0; i < M; ++i)
            {
                const double bi = (i < H) ? static_cast<double> (hHist[static_cast<size_t> (n)][static_cast<size_t> (i)]) : 1.0;
                Xty[static_cast<size_t> (i)] += bi * yt;
                for (int j = i; j < M; ++j)
                {
                    const double bj = (j < H) ? static_cast<double> (hHist[static_cast<size_t> (n)][static_cast<size_t> (j)]) : 1.0;
                    XtX[static_cast<size_t> (i)][static_cast<size_t> (j)] += bi * bj;
                }
            }
        }
        for (int i = 0; i < M; ++i)
            for (int j = 0; j < i; ++j)
                XtX[static_cast<size_t> (i)][static_cast<size_t> (j)] = XtX[static_cast<size_t> (j)][static_cast<size_t> (i)];

        const auto headSol = solveLinearRidge (XtX, Xty, 1.0e-2);

        double ssRes = 0.0, ssTot = 0.0, meanY = 0.0;
        for (int n = warm; n < N; ++n) meanY += yTarget[static_cast<size_t> (n)];
        meanY /= static_cast<double> (N - warm);
        for (int n = warm; n < N; ++n)
        {
            double pred = headSol[H];
            for (int j = 0; j < H; ++j)
                pred += headSol[static_cast<size_t> (j)] * static_cast<double> (hHist[static_cast<size_t> (n)][static_cast<size_t> (j)]);
            const double diff = static_cast<double> (yTarget[static_cast<size_t> (n)]) - pred;
            const double dy   = static_cast<double> (yTarget[static_cast<size_t> (n)]) - meanY;
            ssRes += diff * diff;
            ssTot += dy * dy;
        }
        const double r2 = (ssTot > 1e-9) ? (1.0 - ssRes / ssTot) : 0.0;

        std::vector<float> weights;
        weights.reserve ((4 * H) * (1 + H) + 4 * H + H + H + H + 1);
        weights.insert (weights.end(), W.begin(), W.end());
        weights.insert (weights.end(), B.begin(), B.end());
        for (int j = 0; j < H; ++j) weights.push_back (0.0f);
        for (int j = 0; j < H; ++j) weights.push_back (0.0f);
        for (int j = 0; j < H; ++j) weights.push_back (static_cast<float> (headSol[static_cast<size_t> (j)]));
        weights.push_back (static_cast<float> (headSol[H]));

        std::ofstream f (namPath);
        if (f)
        {
            f << "{\n"
              << "  \"version\": \"0.5.4\",\n"
              << "  \"metadata\": {\n"
              << "    \"name\": \"XV Bass Pedal - " << escapeJson (modelName) << "\",\n"
              << "    \"modeled_by\": \"XV Audio Bass Pedal Studio Bi-Amp\",\n"
              << "    \"gear_type\": \"preamp\",\n"
              << "    \"gear_make\": \"XV Audio\",\n"
              << "    \"gear_model\": \"XV Bass Bi-Amp\"\n"
              << "  },\n"
              << "  \"architecture\": \"LSTM\",\n"
              << "  \"config\": {\n"
              << "    \"input_size\": 1,\n"
              << "    \"hidden_size\": " << H << ",\n"
              << "    \"num_layers\": 1\n"
              << "  },\n"
              << "  \"sample_rate\": 48000.0,\n"
              << "  \"weights\": [";

            f << std::setprecision (7);
            for (size_t i = 0; i < weights.size(); ++i)
            {
                if (i > 0) f << ",";
                float wv = weights[i];
                if (! std::isfinite (wv)) wv = 0.0f;
                f << wv;
            }
            f << "]\n}\n";
        }
        return r2;
    }

    // -------------------------------------------------------------------------
    // Exportación limpia y directa para M-VAVE Blackbox (2 archivos principales + Opcionales)
    // -------------------------------------------------------------------------
    static ExportResultSummary exportFullHardwarePack (const std::string& baseExportDir,
                                                       const std::string& presetDisplayName,
                                                       const PedalControlSnapshot& activeSnap)
    {
        ExportResultSummary summary;
        const std::string slug   = makeSafeSlug (presetDisplayName);
        const std::string root   = joinPathStr (baseExportDir, "XV_Blackbox_" + slug);
        const std::string dirOpt = joinPathStr (root, "Opcionales_Otros_Equipos");

        if (! makeDirsRecursive (dirOpt))
            return summary;

        summary.folderPath = root;

        // 1. Generar excitación de estudio 44.1 kHz, etapa post-1176 y objetivo Bi-Amp real
        const auto uRaw44k  = generateBassExcitation (44100.0, 4.5);
        const auto uComp44k = apply1176InputComp (uRaw44k, 44100.0);
        std::vector<float> yPreamp44k (uRaw44k.size(), 0.0f);
        {
            XVChannelProcessor preampCh;
            preampCh.prepare (44100.0);

            PedalControlSnapshot pSnap = activeSnap;
            pSnap.cab          = CabMode::Clean;
            pSnap.bypassed     = false;
            pSnap.tunerMuted     = false;
            pSnap.pitchEnabled = false;
            preampCh.updateParameters (pSnap);

            for (size_t i = 0; i < uRaw44k.size(); ++i)
                yPreamp44k[i] = preampCh.processSample (uRaw44k[i]) * 1.45f;
        }

        // 2. Archivo N° 1 (Raíz): 1_Cargar_en_AMP_<slug>.am3data (Universal Blackbox!)
        //    + Opcionales: .am4data, .am2data (con AM2\0), .am2 y .nam
        summary.am3FitR2 = distillAndSaveAM3AndAM2 (
            joinPathStr (root,   "1_Cargar_en_AMP_" + slug + ".am3data"),
            joinPathStr (dirOpt, slug + ".am2data"),
            joinPathStr (dirOpt, slug + ".am2"),
            presetDisplayName, activeSnap, uComp44k, yPreamp44k);
        summary.filesWritten += 3;

        summary.am4FitR2 = distillAndSaveAM4 (
            joinPathStr (dirOpt, slug + ".am4data"),
            presetDisplayName, activeSnap, uComp44k, yPreamp44k);
        summary.filesWritten += 1;

        // 3. Archivo N° 2 (Raíz): 2_Cargar_en_CAB_<slug>.wav (Gabinete puro 1024 pts, sin duplicar EQ)
        const char* cabTag = (activeSnap.cab == CabMode::Bass) ? "Bass8x10"
                           : ((activeSnap.cab == CabMode::Punk) ? "Punk8x10" : "CleanDI");
        const auto irPure1024 = buildImpulseResponse (activeSnap, 44100.0, 1024, false);
        const auto irFull2048 = buildImpulseResponse (activeSnap, 44100.0, 2048, true);

        if (writeWav24Mono (joinPathStr (root, std::string ("2_Cargar_en_CAB_") + slug + "_" + cabTag + ".wav"), irPure1024, 44100))
            ++summary.filesWritten;
        if (writeWav24Mono (joinPathStr (dirOpt, "CAB_ConEQ_Completa_" + slug + "_2048.wav"), irFull2048, 44100))
            ++summary.filesWritten;

        // 4. Modelo NAM en Opcionales
        summary.namFitR2 = distillAndSaveNAM (joinPathStr (dirOpt, slug + ".nam"), presetDisplayName, activeSnap);
        ++summary.filesWritten;

        // 5. Guía ultra corta y clara de 3 pasos en la raíz
        {
            std::ofstream g (joinPathStr (root, "LEEME_3_PASOS_BLACKBOX.txt"));
            if (g)
            {
                g << "======================================================================\n"
                  << "  SONIDO LISTO PARA M-VAVE BLACKBOX: " << presetDisplayName << "\n"
                  << "  Fidelidad Bi-Amp de Estudio: " << std::fixed << std::setprecision (1)
                  << (summary.am3FitR2 * 100.0) << "%\n"
                  << "======================================================================\n\n"
                  << "Conecta tu M-VAVE Blackbox por USB, abre el programa M-EFCS y haz esto:\n\n"
                  << "PASO 1 - En el bloque AMP:\n"
                  << "  Haz clic en Importar y elige:\n"
                  << "  -> 1_Cargar_en_AMP_" << slug << ".am3data\n"
                  << "  Deja Gain = 50, Bass = 50, Mid = 50, Treble = 50 (tu EQ ya va adentro).\n\n"
                  << "PASO 2 - En el bloque CAB:\n"
                  << "  Haz clic en Importar y elige:\n"
                  << "  -> 2_Cargar_en_CAB_" << slug << "_" << cabTag << ".wav\n\n"
                  << "PASO 3 - En el bloque FX (al principio de la cadena):\n"
                  << "  Activa el Compresor (Comp) con Sustain = 55 y Level = 60 para darle\n"
                  << "  la pegada exacta de estudio 1176 antes del amplificador.\n\n"
                  << "(Nota: en la carpeta 'Opcionales_Otros_Equipos' incluimos tambien\n"
                  << " .am4data para firmware V20+, .am2data para Tank-B y .nam para PC).\n";
                ++summary.filesWritten;
            }
        }

        summary.ok = true;
        return summary;
    }

#ifdef JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED
    // -------------------------------------------------------------------------
    // Renderizado de Mini-Imagen 240x240 con la posición real de perillas
    // -------------------------------------------------------------------------
    static juce::Image buildCompositePedalImage (const juce::Image& plateActive,
                                                 const juce::Image& knobStrip128,
                                                 const juce::Image& cabImg,
                                                 const juce::Image& fsUpImg,
                                                 const PedalControlSnapshot& snap)
    {
        juce::Image comp (juce::Image::ARGB, 1391, 1131, true);
        juce::Graphics g (comp);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        if (plateActive.isValid())
            g.drawImageAt (plateActive, 0, 0);

        struct KPos { float x, y, norm; };
        const std::array<KPos, 7> kpos {{
            {  171.0f, 114.0f, std::clamp (snap.level / 10.0f, 0.0f, 1.0f) },
            {  390.0f, 114.0f, std::clamp (snap.drive / 10.0f, 0.0f, 1.0f) },
            {  605.0f, 114.0f, std::clamp ((snap.lowDb + 12.0f) / 24.0f, 0.0f, 1.0f) },
            {  820.0f, 114.0f, std::clamp ((snap.highDb + 12.0f) / 24.0f, 0.0f, 1.0f) },
            { 1036.0f, 114.0f, std::clamp (snap.blend, 0.0f, 1.0f) },
            {  504.0f, 351.0f, std::clamp ((snap.midDb + 12.0f) / 24.0f, 0.0f, 1.0f) },
            {  728.0f, 351.0f, std::clamp ((snap.presenceDb + 12.0f) / 24.0f, 0.0f, 1.0f) }
        }};

        if (knobStrip128.isValid())
        {
            const int fw = knobStrip128.getWidth();
            const int fh = knobStrip128.getHeight() / 128;
            for (const auto& kp : kpos)
            {
                const int idx = std::clamp (static_cast<int> (std::round (kp.norm * 127.0f)), 0, 127);
                g.drawImage (knobStrip128,
                             static_cast<int> (kp.x), static_cast<int> (kp.y), 200, 200,
                             0, idx * fh, fw, fh, false);
            }
        }

        if (cabImg.isValid())
            g.drawImage (cabImg, 990, 371, 180, 180, 0, 0, cabImg.getWidth(), cabImg.getHeight(), false);

        if (fsUpImg.isValid())
            g.drawImage (fsUpImg, 166, 734, 220, 220, 0, 0, fsUpImg.getWidth(), fsUpImg.getHeight(), false);

        return comp.getClippedImage ({ 66, 40, 1259, 1046 });
    }

    static juce::Image renderBlackboxScreen240 (const juce::Image& tightPedal,
                                                const juce::String& presetName,
                                                const PedalControlSnapshot& snap,
                                                bool withBadge = true)
    {
        constexpr int S = 480;
        juce::Image hi (juce::Image::ARGB, S, S, true);
        {
            juce::Graphics g (hi);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

            juce::ColourGradient bg (juce::Colour (0xff1a0d12), 240.0f, 80.0f,
                                     juce::Colour (0xff09090d), 240.0f, 480.0f, false);
            g.setGradientFill (bg);
            g.fillAll();

            g.setColour (juce::Colour (0x88e11d48));
            g.drawRoundedRectangle (5.0f, 5.0f, 470.0f, 470.0f, 34.0f, 3.0f);

            if (withBadge)
            {
                const float pw = 406.0f;
                const float ph = pw * (static_cast<float> (tightPedal.getHeight()) / static_cast<float> (std::max (1, tightPedal.getWidth())));
                g.drawImage (tightPedal, juce::Rectangle<float> ((S - pw) * 0.5f, 18.0f, pw, ph));

                const juce::Rectangle<float> tag (24.0f, S - 94.0f, S - 48.0f, 74.0f);
                g.setColour (juce::Colour (0xeb16161d));
                g.fillRoundedRectangle (tag, 18.0f);
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.drawRoundedRectangle (tag, 18.0f, 2.0f);

                g.setColour (juce::Colour (0xfff5f5f7));
#if JUCE_MAJOR_VERSION >= 8
                g.setFont (juce::Font (juce::FontOptions (27.0f, juce::Font::bold)));
#else
                g.setFont (juce::Font (27.0f, juce::Font::bold));
#endif
                juce::String upName = presetName.toUpperCase().trim();
                if (upName.isEmpty()) upName = "XV";
                g.drawText (upName, juce::Rectangle<float> (tag.getX() + 10.0f, tag.getY() + 7.0f, tag.getWidth() - 20.0f, 34.0f),
                            juce::Justification::centred, true);

                const char* cabTxt = (snap.cab == CabMode::Bass) ? "SVT 8X10" : ((snap.cab == CabMode::Punk) ? "SVT PUNK" : "CLEAN DI");
                juce::String subTxt = juce::String ("XV BASS  |  ") + cabTxt;
                if (snap.pitchEnabled && snap.pitchSemitones != 0)
                    subTxt += "  |  " + juce::String (snap.pitchSemitones > 0 ? "+" : "") + juce::String (snap.pitchSemitones) + " ST";

                g.setColour (juce::Colour (0xfff43f5e));
#if JUCE_MAJOR_VERSION >= 8
                g.setFont (juce::Font (juce::FontOptions (17.5f, juce::Font::bold)));
#else
                g.setFont (juce::Font (17.5f, juce::Font::bold));
#endif
                g.drawText (subTxt, juce::Rectangle<float> (tag.getX() + 8.0f, tag.getBottom() - 32.0f, tag.getWidth() - 16.0f, 24.0f),
                            juce::Justification::centred, true);
            }
            else
            {
                const float pw = 442.0f;
                const float ph = pw * (static_cast<float> (tightPedal.getHeight()) / static_cast<float> (std::max (1, tightPedal.getWidth())));
                g.drawImage (tightPedal, juce::Rectangle<float> ((S - pw) * 0.5f, (S - ph) * 0.5f, pw, ph));
            }
        }
        return hi.rescaled (240, 240, juce::Graphics::highResamplingQuality);
    }

    static bool writeBmp24 (const std::string& path, const juce::Image& img)
    {
        const int w = img.getWidth();
        const int h = img.getHeight();
        const int rowStride = (w * 3 + 3) & ~3;
        const uint32_t imgBytes = static_cast<uint32_t> (rowStride * h);
        const uint32_t fileSize = 54u + imgBytes;

        std::ofstream f (path, std::ios::binary);
        if (! f) return false;

        f.write ("BM", 2);
        writeU32LE (f, fileSize);
        writeU32LE (f, 0u);
        writeU32LE (f, 54u);

        writeU32LE (f, 40u);
        writeU32LE (f, static_cast<uint32_t> (w));
        writeU32LE (f, static_cast<uint32_t> (h));
        writeU16LE (f, 1u);
        writeU16LE (f, 24u);
        writeU32LE (f, 0u);
        writeU32LE (f, imgBytes);
        writeU32LE (f, 2835u);
        writeU32LE (f, 2835u);
        writeU32LE (f, 0u);
        writeU32LE (f, 0u);

        std::vector<uint8_t> row (static_cast<size_t> (rowStride), 0u);
        for (int y = h - 1; y >= 0; --y)
        {
            for (int x = 0; x < w; ++x)
            {
                const auto px = img.getPixelAt (x, y);
                row[static_cast<size_t> (x * 3 + 0)] = px.getBlue();
                row[static_cast<size_t> (x * 3 + 1)] = px.getGreen();
                row[static_cast<size_t> (x * 3 + 2)] = px.getRed();
            }
            f.write (reinterpret_cast<const char*> (row.data()), static_cast<std::streamsize> (rowStride));
        }
        return f.good();
    }

    static int exportMiniPedalImages (const std::string& rootDir,
                                      const juce::Image& plateActive,
                                      const juce::Image& knobStrip128,
                                      const juce::Image& cabImg,
                                      const juce::Image& fsUpImg,
                                      const juce::String& presetName,
                                      const PedalControlSnapshot& snap)
    {
        int count = 0;
        const std::string slug   = makeSafeSlug (presetName.toStdString());
        const std::string optDir = joinPathStr (rootDir, "Opcionales_Otros_Equipos");
        makeDirsRecursive (optDir);

        const auto tight    = buildCompositePedalImage (plateActive, knobStrip128, cabImg, fsUpImg, snap);
        const auto scr240   = renderBlackboxScreen240 (tight, presetName, snap, true);
        const auto clean240 = renderBlackboxScreen240 (tight, presetName, snap, false);

        auto savePng = [] (const std::string& p, const juce::Image& im) -> bool
        {
            juce::File jf (juce::String (p));
            jf.deleteFile();
            juce::FileOutputStream fos (jf);
            if (! fos.openedOk()) return false;
            juce::PNGImageFormat png;
            return png.writeImageToStream (im, fos);
        };

        if (savePng (joinPathStr (rootDir, "3_Foto_Perillas_" + slug + ".png"), scr240)) ++count;
        if (savePng (joinPathStr (optDir,  "Icono_SoloPedal_" + slug + "_240x240.png"), clean240)) ++count;
        if (writeBmp24 (joinPathStr (optDir, "Pantalla_" + slug + "_240x240.bmp"), scr240)) ++count;
        return count;
    }
#endif

private:
    static void writeU16LE (std::ofstream& f, uint16_t v)
    {
        const uint8_t b[2] = { static_cast<uint8_t> (v & 0xFFu), static_cast<uint8_t> ((v >> 8) & 0xFFu) };
        f.write (reinterpret_cast<const char*> (b), 2);
    }

    static void writeU32LE (std::ofstream& f, uint32_t v)
    {
        const uint8_t b[4] = {
            static_cast<uint8_t> (v & 0xFFu),
            static_cast<uint8_t> ((v >> 8) & 0xFFu),
            static_cast<uint8_t> ((v >> 16) & 0xFFu),
            static_cast<uint8_t> ((v >> 24) & 0xFFu)
        };
        f.write (reinterpret_cast<const char*> (b), 4);
    }

    static std::string escapeJson (const std::string& s)
    {
        std::string out;
        for (char c : s)
        {
            if (c == '"' || c == '\\') { out.push_back ('\\'); out.push_back (c); }
            else if (static_cast<unsigned char> (c) >= 32) out.push_back (c);
        }
        return out;
    }
};

} // namespace xvdsp
