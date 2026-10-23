#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

#include "Utilities/DbUtils.h"

namespace amp::dsp
{
    // Coeficientes ja normalizados (a0 = 1). Forma transposta direta II, estado em double.
    struct BiquadCoeffs { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };
    struct BiquadState  { double z1 = 0.0, z2 = 0.0; };

    enum class FilterShape : int { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };

    inline double tick (const BiquadCoeffs& c, BiquadState& s, double x) noexcept
    {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    // Formulas do "Audio EQ Cookbook" (R. Bristow-Johnson).
    inline BiquadCoeffs makeBiquad (FilterShape shape, double fs, double freq, double q, double gainDb) noexcept
    {
        freq = std::clamp (freq, 10.0, 0.49 * fs);
        q = std::clamp (q, 0.05, 50.0);
        gainDb = std::clamp (gainDb, -36.0, 36.0);

        const double w0 = 2.0 * kPi * freq / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (shape)
        {
            case FilterShape::Bell:
                b0 = 1 + alpha * A;  b1 = -2 * cs;  b2 = 1 - alpha * A;
                a0 = 1 + alpha / A;  a1 = -2 * cs;  a2 = 1 - alpha / A;
                break;
            case FilterShape::LowShelf:
            {
                const double sA = 2.0 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cs + sA);
                b1 = 2 * A * ((A - 1) - (A + 1) * cs);
                b2 = A * ((A + 1) - (A - 1) * cs - sA);
                a0 = (A + 1) + (A - 1) * cs + sA;
                a1 = -2 * ((A - 1) + (A + 1) * cs);
                a2 = (A + 1) + (A - 1) * cs - sA;
                break;
            }
            case FilterShape::HighShelf:
            {
                const double sA = 2.0 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cs + sA);
                b1 = -2 * A * ((A - 1) + (A + 1) * cs);
                b2 = A * ((A + 1) + (A - 1) * cs - sA);
                a0 = (A + 1) - (A - 1) * cs + sA;
                a1 = 2 * ((A - 1) - (A + 1) * cs);
                a2 = (A + 1) - (A - 1) * cs - sA;
                break;
            }
            case FilterShape::LowCut:   // passa-altas 2a ordem
                b0 = (1 + cs) / 2;  b1 = -(1 + cs);  b2 = (1 + cs) / 2;
                a0 = 1 + alpha;     a1 = -2 * cs;    a2 = 1 - alpha;
                break;
            case FilterShape::HighCut:  // passa-baixas 2a ordem
                b0 = (1 - cs) / 2;  b1 = 1 - cs;     b2 = (1 - cs) / 2;
                a0 = 1 + alpha;     a1 = -2 * cs;    a2 = 1 - alpha;
                break;
            case FilterShape::Notch:
                b0 = 1;  b1 = -2 * cs;  b2 = 1;
                a0 = 1 + alpha;  a1 = -2 * cs;  a2 = 1 - alpha;
                break;
        }

        BiquadCoeffs c;
        c.b0 = b0 / a0;  c.b1 = b1 / a0;  c.b2 = b2 / a0;
        c.a1 = a1 / a0;  c.a2 = a2 / a0;
        return c;
    }

    // Passa-altas de 1a ordem (transformada bilinear).
    inline BiquadCoeffs makeFirstOrderHighPass (double fs, double freq) noexcept
    {
        freq = std::clamp (freq, 10.0, 0.49 * fs);
        const double K = std::tan (kPi * freq / fs);
        BiquadCoeffs c;
        c.b0 = 1.0 / (1.0 + K);
        c.b1 = -c.b0;
        c.a1 = (K - 1.0) / (1.0 + K);
        return c;
    }

    // Resposta em magnitude (dB) na frequencia f.
    inline double magnitudeDb (const BiquadCoeffs& c, double fs, double f) noexcept
    {
        const double w = 2.0 * kPi * f / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
        const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
        const double m = std::abs (num) / std::max (std::abs (den), 1e-30);
        return 20.0 * std::log10 (std::max (m, 1e-12));
    }
}
