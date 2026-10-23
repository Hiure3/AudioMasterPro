#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Utilities/DbUtils.h"

namespace amp::metering
{
    // True Peak (ITU-R BS.1770): sobreamostragem 4x por interpolador FIR polifasico
    // (sinc com janela de Kaiser, 96 taps). Devolve o maior |valor| entre as 4 fases.
    class TruePeakDetector
    {
    public:
        static constexpr int kPhases = 4;
        static constexpr int kTapsPerPhase = 24;

        TruePeakDetector()
        {
            constexpr int total = kPhases * kTapsPerPhase;
            constexpr double beta = 7.0;
            constexpr double centre = total / 2.0;               // 48 (multiplo de 4 -> fase 0 = amostra original)
            const double i0b = besselI0 (beta);
            for (int i = 0; i < total; ++i)
            {
                const double t = (i - centre) / 4.0;
                const double sinc = std::fabs (t) < 1e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                const double r = (i - centre) / centre;
                const double win = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
                h_[static_cast<size_t> (i % kPhases)][static_cast<size_t> (i / kPhases)] = static_cast<float> (sinc * win);
            }
            reset();
        }

        void reset() noexcept { hist_.fill (0.0f); }

        float process (float x) noexcept
        {
            for (int i = kTapsPerPhase - 1; i > 0; --i)
                hist_[static_cast<size_t> (i)] = hist_[static_cast<size_t> (i - 1)];
            hist_[0] = x;

            float peak = 0.0f;
            for (int p = 0; p < kPhases; ++p)
            {
                float acc = 0.0f;
                for (int k = 0; k < kTapsPerPhase; ++k)
                    acc += h_[static_cast<size_t> (p)][static_cast<size_t> (k)] * hist_[static_cast<size_t> (k)];
                peak = std::max (peak, std::fabs (acc));
            }
            return peak;
        }

    private:
        static double besselI0 (double x) noexcept
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        }

        std::array<std::array<float, kTapsPerPhase>, kPhases> h_ {};
        std::array<float, kTapsPerPhase> hist_ {};
    };
}
