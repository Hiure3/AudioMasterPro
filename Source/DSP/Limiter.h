#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "StereoDelay.h"
#include "Utilities/DbUtils.h"
#include "Utilities/SmoothedValue.h"

namespace amp::dsp
{
    // Limiter brickwall com lookahead: filtro de minimo deslizante + media movel (rampa de
    // ataque de duracao = lookahead) + release exponencial. Garante pico <= ceiling.
    // Threshold: nivel que sera "empurrado" ate o Ceiling (ganho de entrada = ceiling - threshold).
    // A latencia (lookahead) existe sempre, mesmo desligado, para o bypass ficar alinhado.
    class Limiter
    {
    public:
        static constexpr double kMaxLookaheadMs = 10.0;

        void prepare (double sr)
        {
            sr_ = sr;
            maxN_ = static_cast<int> (std::ceil (kMaxLookaheadMs * 0.001 * sr)) + 2;
            delay_.prepare (maxN_);
            minVal_.assign (static_cast<size_t> (maxN_ + 2), 1.0f);
            minIdx_.assign (static_cast<size_t> (maxN_ + 2), 0);
            boxHist_.assign (static_cast<size_t> (maxN_ + 2), 1.0f);
            enable_.reset (sr, 0.01);   enable_.setCurrentAndTarget (0.0f);
            preGain_.reset (sr, 0.02);  preGain_.setCurrentAndTarget (1.0f);
            n_ = 0;
            haveParams_ = false;
            resetState();
        }

        void setParams (bool enabled, float thresholdDb, float ceilingDb, float releaseMs, float lookaheadMs) noexcept
        {
            const int la = std::clamp (static_cast<int> (std::lround (lookaheadMs * 0.001 * sr_)), 1, maxN_);
            const bool first = ! haveParams_;
            if (first || la != n_)
            {
                n_ = la;
                resetState();
                haveParams_ = true;
            }
            delay_.setDelay (n_);
            ceilLin_ = dbToLinear (ceilingDb);
            relCoef_ = std::exp (-1.0 / (std::max (1.0f, releaseMs) * 0.001 * sr_));
            const float en = enabled ? 1.0f : 0.0f;
            const float pg = enabled ? dbToLinear (ceilingDb - thresholdDb) : 1.0f;
            if (first)   // primeiro estado: sem rampa (o limiter ja nasce protegendo a saida)
            {
                enable_.setCurrentAndTarget (en);
                preGain_.setCurrentAndTarget (pg);
            }
            enable_.setTarget (en);
            preGain_.setTarget (pg);
        }

        int latencySamples() const noexcept { return n_; }
        float lastBlockMaxGrDb() const noexcept { return blockMaxGr_; }

        void process (float* l, float* r, int n) noexcept
        {
            blockMaxGr_ = 0.0f;
            const int win = n_ + 1;
            const size_t cap = minVal_.size();
            const double invWin = 1.0 / static_cast<double> (win);

            for (int i = 0; i < n; ++i)
            {
                const float pg = preGain_.next();
                const float en = enable_.next();
                const float xl = l[i] * pg, xr = r[i] * pg;
                const float peak = std::max (std::fabs (xl), std::fabs (xr));
                const float req = peak > ceilLin_ ? ceilLin_ / peak : 1.0f;

                // minimo deslizante das ultimas `win` amostras (deque monotono)
                while (cnt_ > 0)
                {
                    const size_t b = (head_ + cnt_ - 1) % cap;
                    if (minVal_[b] >= req) --cnt_; else break;
                }
                const size_t pos = (head_ + cnt_) % cap;
                minVal_[pos] = req;  minIdx_[pos] = idx_;  ++cnt_;
                while (minIdx_[head_] <= idx_ - win) { head_ = (head_ + 1) % cap; --cnt_; }
                const float m = minVal_[head_];
                ++idx_;

                // media movel de `win` amostras do minimo
                boxSum_ += static_cast<double> (m) - static_cast<double> (boxHist_[boxPos_]);
                boxHist_[boxPos_] = m;
                boxPos_ = (boxPos_ + 1) % static_cast<size_t> (win);
                const float s = static_cast<float> (std::min (1.0, boxSum_ * invWin));

                // release: reducao imediata (ja suave), recuperacao exponencial
                g_ = (s < g_) ? s : static_cast<float> (relCoef_ * g_ + (1.0 - relCoef_) * s);

                float dl, dr;
                delay_.pushPop (xl, xr, dl, dr);

                const float gApplied = 1.0f + (g_ - 1.0f) * en;
                float ol = dl * gApplied, orr = dr * gApplied;
                if (en >= 1.0f)   // rede de seguranca contra erro de arredondamento
                {
                    ol = std::clamp (ol, -ceilLin_, ceilLin_);
                    orr = std::clamp (orr, -ceilLin_, ceilLin_);
                }
                l[i] = ol;
                r[i] = orr;

                if (gApplied < 1.0f)
                {
                    const float grDb = -20.0f * std::log10 (std::max (gApplied, 1e-6f));
                    if (grDb > blockMaxGr_) blockMaxGr_ = grDb;
                }
            }
        }

    private:
        void resetState() noexcept
        {
            delay_.clear();
            std::fill (minVal_.begin(), minVal_.end(), 1.0f);
            std::fill (boxHist_.begin(), boxHist_.end(), 1.0f);
            boxSum_ = static_cast<double> (n_ + 1);
            head_ = cnt_ = boxPos_ = 0;
            idx_ = 0;
            g_ = 1.0f;
            for (auto& v : minIdx_) v = 0;
        }

        double sr_ = 48000.0, relCoef_ = 0.99, boxSum_ = 1.0;
        int maxN_ = 480, n_ = 96;
        float ceilLin_ = 0.89f, g_ = 1.0f, blockMaxGr_ = 0.0f;
        StereoDelay delay_;
        std::vector<float> minVal_, boxHist_;
        std::vector<long long> minIdx_;
        size_t head_ = 0, cnt_ = 0, boxPos_ = 0;
        long long idx_ = 0;
        SmoothedValue enable_, preGain_;
        bool haveParams_ = false;
    };
}
