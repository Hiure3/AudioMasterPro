#pragma once

#include "Audio/Parameters.h"
#include "Biquad.h"
#include "Utilities/SmoothedValue.h"

namespace amp::dsp
{
    // EQ paramétrico de 8 bandas (Bell, Low/High Shelf, Low/High Cut, Notch).
    class ParametricEQ
    {
    public:
        void prepare (double sr)
        {
            sr_ = sr;
            for (int b = 0; b < kNumEqBands; ++b)
            {
                mix_[b].reset (sr, 0.01);
                mix_[b].setCurrentAndTarget (0.0f);
                st_[b][0] = st_[b][1] = BiquadState{};
            }
            valid_ = false;
        }

        void setParams (bool eqEnabled, const EqBandParams* bands) noexcept
        {
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const auto& p = bands[b];
                if (! valid_ || differs (p, cur_[b]))
                {
                    coeffs_[b] = makeBiquad (static_cast<FilterShape> (p.type), sr_, p.freqHz, p.q, p.gainDb);
                    cur_[b] = p;
                }
                mix_[b].setTarget ((eqEnabled && p.enabled) ? 1.0f : 0.0f);
            }
            valid_ = true;
        }

        void process (float* l, float* r, int n) noexcept
        {
            for (int b = 0; b < kNumEqBands; ++b)
            {
                auto& mix = mix_[b];
                if (mix.target() <= 0.0f && mix.current() <= 0.0f && ! mix.isSmoothing())
                {
                    st_[b][0] = st_[b][1] = BiquadState{};
                    continue;
                }
                const auto& c = coeffs_[b];
                for (int i = 0; i < n; ++i)
                {
                    const float m = mix.next();
                    const double xl = l[i], xr = r[i];
                    const double yl = tick (c, st_[b][0], xl);
                    const double yr = tick (c, st_[b][1], xr);
                    l[i] = static_cast<float> (xl + (yl - xl) * m);
                    r[i] = static_cast<float> (xr + (yr - xr) * m);
                }
            }
        }

        // Resposta combinada das bandas ativas (usada pelo grafico; mesma matematica do DSP).
        static double responseDb (bool eqEnabled, const EqBandParams* bands, double sr, double f) noexcept
        {
            if (! eqEnabled)
                return 0.0;
            double s = 0.0;
            for (int b = 0; b < kNumEqBands; ++b)
                if (bands[b].enabled)
                    s += magnitudeDb (makeBiquad (static_cast<FilterShape> (bands[b].type), sr, bands[b].freqHz,
                                                  bands[b].q, bands[b].gainDb), sr, f);
            return s;
        }

    private:
        static bool differs (const EqBandParams& a, const EqBandParams& b) noexcept
        {
            return a.type != b.type || a.freqHz != b.freqHz || a.gainDb != b.gainDb || a.q != b.q;
        }

        double sr_ = 48000.0;
        BiquadCoeffs coeffs_[kNumEqBands];
        EqBandParams cur_[kNumEqBands];
        BiquadState st_[kNumEqBands][2];
        SmoothedValue mix_[kNumEqBands];
        bool valid_ = false;
    };
}
