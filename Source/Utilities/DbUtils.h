#pragma once

#include <algorithm>
#include <cmath>

namespace amp
{
    // Valor usado como "silencio" em dB (evita -infinito e NaN).
    inline constexpr float kSilenceDb = -120.0f;
    inline constexpr double kPi = 3.14159265358979323846;

    // dB -> ganho linear. 0 dB = 1.0, -6.02 dB ~ 0.5.
    template <typename T>
    inline T dbToLinear (T db) noexcept
    {
        return static_cast<T> (std::pow (static_cast<T> (10), db / static_cast<T> (20)));
    }

    // Ganho linear -> dB. Valores <= 0 (ou invalidos) retornam kSilenceDb.
    template <typename T>
    inline T linearToDb (T linear) noexcept
    {
        if (! (linear > static_cast<T> (0)))   // trata 0, negativos e NaN
            return static_cast<T> (kSilenceDb);

        const auto db = static_cast<T> (20) * std::log10 (linear);
        return db < static_cast<T> (kSilenceDb) ? static_cast<T> (kSilenceDb) : db;
    }

    // Limita v a [lo, hi]; se v nao for finito (NaN/Inf) devolve fallback.
    inline float clampFinite (float v, float lo, float hi, float fallback) noexcept
    {
        if (! std::isfinite (v))
            return fallback;
        return std::min (hi, std::max (lo, v));
    }
}
