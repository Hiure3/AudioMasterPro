#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "DbUtils.h"

// Geradores de sinais de teste (Sine, Pink/White Noise, Impulse, Silence).
namespace amp::testsignals
{
    inline std::vector<float> silence (int n) { return std::vector<float> (static_cast<size_t> (n), 0.0f); }

    inline std::vector<float> sine (double freq, double sampleRate, int n, double amplitude = 1.0, double phase = 0.0)
    {
        std::vector<float> v (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
            v[static_cast<size_t> (i)] = static_cast<float> (amplitude * std::sin (2.0 * kPi * freq * i / sampleRate + phase));
        return v;
    }

    inline std::vector<float> whiteNoise (int n, double amplitude = 1.0, std::uint32_t seed = 1)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<double> d (-1.0, 1.0);
        std::vector<float> v (static_cast<size_t> (n));
        for (auto& x : v) x = static_cast<float> (amplitude * d (rng));
        return v;
    }

    // Ruido rosa (filtro de Paul Kellet).
    inline std::vector<float> pinkNoise (int n, double amplitude = 1.0, std::uint32_t seed = 1)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<double> d (-1.0, 1.0);
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        std::vector<float> v (static_cast<size_t> (n));
        for (auto& x : v)
        {
            const double w = d (rng);
            b0 = 0.99886 * b0 + w * 0.0555179;
            b1 = 0.99332 * b1 + w * 0.0750759;
            b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856;
            b4 = 0.55000 * b4 + w * 0.5329522;
            b5 = -0.7616 * b5 - w * 0.0168980;
            const double pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
            b6 = w * 0.115926;
            x = static_cast<float> (amplitude * pink * 0.11);
        }
        return v;
    }

    inline std::vector<float> impulse (int n, int position = 0, double amplitude = 1.0)
    {
        std::vector<float> v (static_cast<size_t> (n), 0.0f);
        if (position >= 0 && position < n)
            v[static_cast<size_t> (position)] = static_cast<float> (amplitude);
        return v;
    }
}
