#pragma once

#include <cstdint>
#include <vector>

#include "DSP/Biquad.h"
#include "TruePeak.h"

namespace amp::metering
{
    struct LoudnessResults
    {
        double momentary = -120.0;      // LUFS-M (400 ms)
        double shortTerm = -120.0;      // LUFS-S (3 s)
        double integrated = -120.0;     // LUFS-I (com gating)
        double lra = 0.0;               // LU
        double truePeakDb = -120.0;     // dBTP, ultimos ~100 ms
        double truePeakMaxDb = -120.0;  // dBTP maximo desde o reset
        double elapsedSeconds = 0.0;
    };

    inline constexpr double kNoLoudness = -120.0;

    // Medidor de loudness ITU-R BS.1770-4 / EBU R128 (estereo L+R, pesos 1.0):
    //  - K-weighting (shelving + RLB), blocos de 400 ms com 75% de sobreposicao
    //  - Momentary (400 ms), Short-term (3 s), Integrated (gate -70 LUFS absoluto, -10 LU relativo)
    //  - LRA (EBU Tech 3342: gate -70 / -20 LU, percentis 10 e 95)
    //  - True Peak (4x oversampling)
    // Integrated e LRA usam histogramas (sem alocacao em process()).
    class LoudnessMeter
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void process (const float* l, const float* r, int n) noexcept;
        LoudnessResults results() const noexcept;

        // Coeficientes dos filtros K-weighting (publicos para testes).
        static dsp::BiquadCoeffs kWeightStage1 (double fs) noexcept;   // shelving de alta
        static dsp::BiquadCoeffs kWeightStage2 (double fs) noexcept;   // passa-altas RLB

    private:
        void finishSubBlock() noexcept;
        double meanLastEnergy (int count) const noexcept;
        double computeIntegrated() const noexcept;
        double computeLra() const noexcept;

        static constexpr int kRing = 30;            // 30 x 100 ms = 3 s
        static constexpr int kIntBins = 8000;       // -70..+10 LUFS, 0.01 LU
        static constexpr int kLraBins = 900;        // -70..+20 LUFS, 0.1 LU

        double sr_ = 48000.0;
        int step_ = 4800, count_ = 0;
        dsp::BiquadCoeffs s1_, s2_;
        dsp::BiquadState st1_[2], st2_[2];
        TruePeakDetector tp_[2];
        double sumL_ = 0, sumR_ = 0;
        double ring_[kRing] {};
        int ringPos_ = 0, filled_ = 0;
        std::int64_t totalSub_ = 0;
        std::vector<double> intEnergy_, lraEnergy_;
        std::vector<std::uint64_t> intCount_, lraCount_;
        float tpAcc_ = 0.0f, lastBlockTp_ = 0.0f, tpMax_ = 0.0f;
        LoudnessResults res_;
    };
}
