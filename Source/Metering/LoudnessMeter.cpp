#include "LoudnessMeter.h"

#include <algorithm>
#include <cmath>

#include "Utilities/DbUtils.h"

namespace amp::metering
{
    namespace
    {
        constexpr double kAbsGate = -70.0;

        inline double energyToLoudness (double e) noexcept
        {
            return e > 1e-12 ? -0.691 + 10.0 * std::log10 (e) : kNoLoudness;
        }
    }

    dsp::BiquadCoeffs LoudnessMeter::kWeightStage1 (double fs) noexcept
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        dsp::BiquadCoeffs c;
        c.b0 = (Vh + Vb * K / Q + K * K) / a0;
        c.b1 = 2.0 * (K * K - Vh) / a0;
        c.b2 = (Vh - Vb * K / Q + K * K) / a0;
        c.a1 = 2.0 * (K * K - 1.0) / a0;
        c.a2 = (1.0 - K / Q + K * K) / a0;
        return c;
    }

    dsp::BiquadCoeffs LoudnessMeter::kWeightStage2 (double fs) noexcept
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        dsp::BiquadCoeffs c;
        c.b0 = 1.0;  c.b1 = -2.0;  c.b2 = 1.0;     // numerador exato da norma
        c.a1 = 2.0 * (K * K - 1.0) / a0;
        c.a2 = (1.0 - K / Q + K * K) / a0;
        return c;
    }

    void LoudnessMeter::prepare (double sampleRate)
    {
        sr_ = sampleRate;
        step_ = std::max (1, static_cast<int> (std::lround (0.1 * sampleRate)));
        s1_ = kWeightStage1 (sampleRate);
        s2_ = kWeightStage2 (sampleRate);
        intEnergy_.assign (kIntBins, 0.0);
        intCount_.assign (kIntBins, 0);
        lraEnergy_.assign (kLraBins, 0.0);
        lraCount_.assign (kLraBins, 0);
        reset();
    }

    void LoudnessMeter::reset()
    {
        for (int c = 0; c < 2; ++c) { st1_[c] = {}; st2_[c] = {}; tp_[c].reset(); }
        std::fill (intEnergy_.begin(), intEnergy_.end(), 0.0);
        std::fill (intCount_.begin(), intCount_.end(), 0);
        std::fill (lraEnergy_.begin(), lraEnergy_.end(), 0.0);
        std::fill (lraCount_.begin(), lraCount_.end(), 0);
        for (double& v : ring_) v = 0.0;
        ringPos_ = filled_ = count_ = 0;
        totalSub_ = 0;
        sumL_ = sumR_ = 0.0;
        tpAcc_ = lastBlockTp_ = tpMax_ = 0.0f;
        res_ = LoudnessResults{};
    }

    void LoudnessMeter::process (const float* l, const float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float fl = std::isfinite (l[i]) ? l[i] : 0.0f;
            const float fr = std::isfinite (r[i]) ? r[i] : 0.0f;

            const float tp = std::max (tp_[0].process (fl), tp_[1].process (fr));
            tpAcc_ = std::max (tpAcc_, tp);
            tpMax_ = std::max (tpMax_, tp);

            const double yl = dsp::tick (s2_, st2_[0], dsp::tick (s1_, st1_[0], fl));
            const double yr = dsp::tick (s2_, st2_[1], dsp::tick (s1_, st1_[1], fr));
            sumL_ += yl * yl;
            sumR_ += yr * yr;

            if (++count_ >= step_)
                finishSubBlock();
        }
    }

    double LoudnessMeter::meanLastEnergy (int count) const noexcept
    {
        double s = 0.0;
        for (int j = 0; j < count; ++j)
            s += ring_[(ringPos_ - 1 - j + 2 * kRing) % kRing];
        return s / count;
    }

    void LoudnessMeter::finishSubBlock() noexcept
    {
        const double e = (sumL_ + sumR_) / static_cast<double> (step_);
        sumL_ = sumR_ = 0.0;
        count_ = 0;

        ring_[ringPos_] = e;
        ringPos_ = (ringPos_ + 1) % kRing;
        if (filled_ < kRing) ++filled_;
        ++totalSub_;

        if (filled_ >= 4)
        {
            const double m = meanLastEnergy (4);
            const double lm = energyToLoudness (m);
            res_.momentary = lm;
            if (lm > kAbsGate)
            {
                const int bin = std::clamp (static_cast<int> ((lm - kAbsGate) / 0.01), 0, kIntBins - 1);
                intEnergy_[static_cast<size_t> (bin)] += m;
                ++intCount_[static_cast<size_t> (bin)];
            }
        }
        else
            res_.momentary = kNoLoudness;

        const double sEnergy = meanLastEnergy (filled_);
        const double ls = energyToLoudness (sEnergy);
        res_.shortTerm = ls;
        if (filled_ >= kRing && ls > kAbsGate)
        {
            const int bin = std::clamp (static_cast<int> ((ls - kAbsGate) / 0.1), 0, kLraBins - 1);
            lraEnergy_[static_cast<size_t> (bin)] += sEnergy;
            ++lraCount_[static_cast<size_t> (bin)];
        }

        res_.integrated = computeIntegrated();
        res_.lra = computeLra();

        lastBlockTp_ = tpAcc_;
        tpAcc_ = 0.0f;
    }

    double LoudnessMeter::computeIntegrated() const noexcept
    {
        double sumE = 0.0;
        std::uint64_t cnt = 0;
        for (int i = 0; i < kIntBins; ++i)
        {
            sumE += intEnergy_[static_cast<size_t> (i)];
            cnt += intCount_[static_cast<size_t> (i)];
        }
        if (cnt == 0)
            return kNoLoudness;

        const double relGate = energyToLoudness (sumE / static_cast<double> (cnt)) - 10.0;
        const int start = std::clamp (static_cast<int> (std::ceil ((relGate - kAbsGate) / 0.01)), 0, kIntBins - 1);

        double sumE2 = 0.0;
        std::uint64_t cnt2 = 0;
        for (int i = start; i < kIntBins; ++i)
        {
            sumE2 += intEnergy_[static_cast<size_t> (i)];
            cnt2 += intCount_[static_cast<size_t> (i)];
        }
        return cnt2 == 0 ? kNoLoudness : energyToLoudness (sumE2 / static_cast<double> (cnt2));
    }

    double LoudnessMeter::computeLra() const noexcept
    {
        double sumE = 0.0;
        std::uint64_t cnt = 0;
        for (int i = 0; i < kLraBins; ++i)
        {
            sumE += lraEnergy_[static_cast<size_t> (i)];
            cnt += lraCount_[static_cast<size_t> (i)];
        }
        if (cnt == 0)
            return 0.0;

        const double relGate = energyToLoudness (sumE / static_cast<double> (cnt)) - 20.0;
        const int start = std::clamp (static_cast<int> (std::ceil ((relGate - kAbsGate) / 0.1)), 0, kLraBins - 1);

        std::uint64_t total = 0;
        for (int i = start; i < kLraBins; ++i)
            total += lraCount_[static_cast<size_t> (i)];
        if (total == 0)
            return 0.0;

        const double loTarget = 0.10 * static_cast<double> (total);
        const double hiTarget = 0.95 * static_cast<double> (total);
        int lo = start, hi = start;
        bool loSet = false;
        double cum = 0.0;
        for (int i = start; i < kLraBins; ++i)
        {
            cum += static_cast<double> (lraCount_[static_cast<size_t> (i)]);
            if (! loSet && cum >= loTarget) { lo = i; loSet = true; }
            if (cum >= hiTarget) { hi = i; break; }
        }
        return std::max (0, hi - lo) * 0.1;
    }

    LoudnessResults LoudnessMeter::results() const noexcept
    {
        LoudnessResults r = res_;
        r.truePeakDb = linearToDb (static_cast<double> (std::max (lastBlockTp_, tpAcc_)));
        r.truePeakMaxDb = linearToDb (static_cast<double> (tpMax_));
        r.elapsedSeconds = static_cast<double> (totalSub_) * 0.1;
        return r;
    }
}
