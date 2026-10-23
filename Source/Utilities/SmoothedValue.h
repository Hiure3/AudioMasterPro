#pragma once

#include <algorithm>

namespace amp
{
    // Rampa linear de um parametro (evita clicks). Uso: setTarget() e next() por amostra.
    class SmoothedValue
    {
    public:
        void reset (double sampleRate, double rampSeconds) noexcept
        {
            rampSamples_ = std::max (1, static_cast<int> (sampleRate * rampSeconds));
            steps_ = 0;
            cur_ = target_;
        }

        void setCurrentAndTarget (float v) noexcept { cur_ = target_ = v; steps_ = 0; }

        void setTarget (float t) noexcept
        {
            if (t == target_)
                return;
            target_ = t;
            steps_ = rampSamples_;
            inc_ = (t - cur_) / static_cast<float> (steps_);
        }

        float next() noexcept
        {
            if (steps_ > 0)
            {
                cur_ += inc_;
                if (--steps_ == 0)
                    cur_ = target_;
            }
            return cur_;
        }

        float current() const noexcept { return cur_; }
        float target() const noexcept { return target_; }
        bool isSmoothing() const noexcept { return steps_ > 0; }

    private:
        float cur_ = 0.0f, target_ = 0.0f, inc_ = 0.0f;
        int steps_ = 0, rampSamples_ = 480;
    };
}
