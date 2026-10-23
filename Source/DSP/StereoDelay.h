#pragma once

#include <vector>

namespace amp::dsp
{
    class StereoDelay
    {
    public:
        void prepare (int maxDelaySamples)
        {
            cap_ = maxDelaySamples + 1;
            l_.assign (static_cast<size_t> (cap_), 0.0f);
            r_.assign (static_cast<size_t> (cap_), 0.0f);
            w_ = 0;
            d_ = 0;
        }

        // Muda o atraso (limpa o buffer se mudar; fora da faixa e limitado).
        void setDelay (int d) noexcept
        {
            d = d < 0 ? 0 : (d > cap_ - 1 ? cap_ - 1 : d);
            if (d != d_)
            {
                d_ = d;
                clear();
            }
        }

        int delay() const noexcept { return d_; }

        void clear() noexcept
        {
            std::fill (l_.begin(), l_.end(), 0.0f);
            std::fill (r_.begin(), r_.end(), 0.0f);
        }

        inline void pushPop (float inL, float inR, float& outL, float& outR) noexcept
        {
            l_[static_cast<size_t> (w_)] = inL;
            r_[static_cast<size_t> (w_)] = inR;
            int rd = w_ - d_;
            if (rd < 0) rd += cap_;
            outL = l_[static_cast<size_t> (rd)];
            outR = r_[static_cast<size_t> (rd)];
            if (++w_ >= cap_) w_ = 0;
        }

        void process (float* l, float* r, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
                pushPop (l[i], r[i], l[i], r[i]);
        }

    private:
        std::vector<float> l_, r_;
        int cap_ = 1, w_ = 0, d_ = 0;
    };
}
