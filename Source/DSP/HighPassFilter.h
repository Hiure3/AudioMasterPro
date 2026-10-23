#pragma once

#include "Biquad.h"
#include "Utilities/SmoothedValue.h"

namespace amp::dsp
{
    // HPF Butterworth em cascata: 12/18/24/36/48 dB/oct (ordens 2,3,4,6,8).
    class HighPassFilter
    {
    public:
        static constexpr int kMaxSections = 4;
        struct Design { BiquadCoeffs c[kMaxSections]; int n = 0; };

        static int orderForSlope (int slopeIndex) noexcept
        {
            static constexpr int orders[5] = { 2, 3, 4, 6, 8 };
            return orders[std::clamp (slopeIndex, 0, 4)];
        }

        static Design design (double sr, double freq, int slopeIndex) noexcept
        {
            Design d;
            const int order = orderForSlope (slopeIndex);
            if (order % 2 == 1)
                d.c[d.n++] = makeFirstOrderHighPass (sr, freq);
            for (int k = 1; k <= order / 2; ++k)
            {
                const double alpha = (order + 1 - 2 * k) * kPi / (2.0 * order);   // angulo do polo
                const double q = 1.0 / (2.0 * std::cos (alpha));
                d.c[d.n++] = makeBiquad (FilterShape::LowCut, sr, freq, q, 0.0);
            }
            return d;
        }

        static double magnitudeDb (double sr, double freq, int slopeIndex, double f) noexcept
        {
            const Design d = design (sr, freq, slopeIndex);
            double s = 0.0;
            for (int i = 0; i < d.n; ++i)
                s += dsp::magnitudeDb (d.c[i], sr, f);
            return s;
        }

        void prepare (double sr)
        {
            sr_ = sr;
            mix_.reset (sr, 0.01);
            mix_.setCurrentAndTarget (0.0f);
            haveParams_ = false;
            clearState();
        }

        void setParams (bool enabled, float freq, int slope) noexcept
        {
            if (! haveParams_ || freq != freq_ || slope != slope_)
            {
                design_ = design (sr_, freq, slope);
                freq_ = freq;
                slope_ = slope;
                haveParams_ = true;
            }
            mix_.setTarget (enabled ? 1.0f : 0.0f);
        }

        void process (float* l, float* r, int n) noexcept
        {
            if (mix_.target() <= 0.0f && ! mix_.isSmoothing())
            {
                clearState();
                return;
            }
            for (int i = 0; i < n; ++i)
            {
                const float m = mix_.next();
                double xl = l[i], xr = r[i], yl = xl, yr = xr;
                for (int s = 0; s < design_.n; ++s)
                {
                    yl = tick (design_.c[s], st_[s][0], yl);
                    yr = tick (design_.c[s], st_[s][1], yr);
                }
                l[i] = static_cast<float> (xl + (yl - xl) * m);
                r[i] = static_cast<float> (xr + (yr - xr) * m);
            }
        }

    private:
        void clearState() noexcept { for (auto& a : st_) for (auto& s : a) s = BiquadState{}; }

        double sr_ = 48000.0;
        Design design_;
        BiquadState st_[kMaxSections][2];
        SmoothedValue mix_;
        float freq_ = 0.0f;
        int slope_ = -1;
        bool haveParams_ = false;
    };
}
