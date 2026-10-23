#pragma once

#include <algorithm>
#include <cmath>

#include "Utilities/DbUtils.h"
#include "Utilities/SmoothedValue.h"

namespace amp::dsp
{
    // Compressor feed-forward, detector de pico com link estereo, joelho suave (soft knee).
    // (Preparado para RMS / sidechain externo no futuro.)
    class Compressor
    {
    public:
        void prepare (double sr)
        {
            sr_ = sr;
            enable_.reset (sr, 0.01);  enable_.setCurrentAndTarget (0.0f);
            makeup_.reset (sr, 0.02);  makeup_.setCurrentAndTarget (1.0f);
            mix_.reset (sr, 0.02);     mix_.setCurrentAndTarget (1.0f);
            env_ = 0.0;
            blockMaxGr_ = 0.0f;
        }

        void setParams (bool enabled, float thrDb, float ratio, float attMs, float relMs,
                        float kneeDb, float makeupDb, float mix) noexcept
        {
            thr_ = thrDb;
            ratio_ = std::max (1.0f, ratio);
            knee_ = std::max (0.0f, kneeDb);
            attCoef_ = std::exp (-1.0 / (std::max (0.01f, attMs) * 0.001 * sr_));
            relCoef_ = std::exp (-1.0 / (std::max (1.0f, relMs) * 0.001 * sr_));
            enable_.setTarget (enabled ? 1.0f : 0.0f);
            makeup_.setTarget (dbToLinear (makeupDb));
            mix_.setTarget (std::clamp (mix, 0.0f, 1.0f));
        }

        // Maxima reducao de ganho (dB, >= 0) do ultimo bloco.
        float lastBlockMaxGrDb() const noexcept { return blockMaxGr_; }

        void process (float* l, float* r, int n) noexcept
        {
            blockMaxGr_ = 0.0f;
            if (enable_.target() <= 0.0f && ! enable_.isSmoothing())
            {
                env_ = 0.0;
                return;
            }

            const double W = knee_;
            const double slope = 1.0 - 1.0 / static_cast<double> (ratio_);

            for (int i = 0; i < n; ++i)
            {
                const float peak = std::max (std::fabs (l[i]), std::fabs (r[i]));
                const double lvDb = 20.0 * std::log10 (std::max (static_cast<double> (peak), 1e-9));
                const double over = lvDb - thr_;

                double gr = 0.0;   // reducao desejada em dB (>= 0)
                if (W <= 0.0)
                    gr = over > 0.0 ? slope * over : 0.0;
                else if (2.0 * over < -W)
                    gr = 0.0;
                else if (2.0 * std::fabs (over) <= W)
                {
                    const double x = over + W / 2.0;
                    gr = slope * x * x / (2.0 * W);
                }
                else
                    gr = slope * over;

                env_ = (gr > env_) ? attCoef_ * env_ + (1.0 - attCoef_) * gr
                                   : relCoef_ * env_ + (1.0 - relCoef_) * gr;

                const float g = static_cast<float> (std::pow (10.0, -env_ / 20.0)) * makeup_.next();
                const float mix = mix_.next();
                const float en = enable_.next();

                const float dl = l[i], dr = r[i];
                const float wl = dl * (1.0f - mix) + dl * g * mix;
                const float wr = dr * (1.0f - mix) + dr * g * mix;
                l[i] = dl + (wl - dl) * en;
                r[i] = dr + (wr - dr) * en;

                if (env_ > blockMaxGr_)
                    blockMaxGr_ = static_cast<float> (env_);
            }
        }

    private:
        double sr_ = 48000.0, env_ = 0.0, attCoef_ = 0.0, relCoef_ = 0.0;
        float thr_ = -18.0f, ratio_ = 3.0f, knee_ = 6.0f, blockMaxGr_ = 0.0f;
        SmoothedValue enable_, makeup_, mix_;
    };
}
