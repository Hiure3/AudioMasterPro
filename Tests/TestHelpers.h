#pragma once

#include <cmath>
#include <vector>

#include "Utilities/DbUtils.h"

namespace th
{
    inline bool allFinite (const std::vector<float>& v)
    {
        for (float x : v) if (! std::isfinite (x)) return false;
        return true;
    }

    inline double rms (const std::vector<float>& v, size_t from, size_t to)
    {
        double s = 0.0;
        for (size_t i = from; i < to; ++i) s += static_cast<double> (v[i]) * v[i];
        return std::sqrt (s / static_cast<double> (to - from));
    }

    inline double peak (const std::vector<float>& v, size_t from = 0, size_t to = 0)
    {
        if (to == 0 || to > v.size()) to = v.size();
        double p = 0.0;
        for (size_t i = from; i < to; ++i) p = std::max (p, std::fabs (static_cast<double> (v[i])));
        return p;
    }

    // Aplica f(l, r, n) em blocos de `block` amostras (L e R iguais ao sinal dado).
    template <typename F>
    inline void processInBlocks (std::vector<float>& l, std::vector<float>& r, int block, F&& f)
    {
        const int total = static_cast<int> (l.size());
        for (int pos = 0; pos < total; pos += block)
            f (l.data() + pos, r.data() + pos, std::min (block, total - pos));
    }
}
