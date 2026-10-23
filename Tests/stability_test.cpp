// Teste de estabilidade prolongado do motor de audio.
//   AMP_StabilityTest --seconds 20        (rapido; roda no ctest)
//   AMP_StabilityTest --minutes 30        (longo; mais rapido que tempo real)
//   AMP_StabilityTest --minutes 30 --realtime   (30 min de verdade, com sleeps)
// Verifica: NaN/Inf, crash, vazamento de memoria, picos de CPU e dropouts.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

#include "Audio/AudioEngine.h"
#include "Presets/Presets.h"
#include "Utilities/SystemInfo.h"
#include "Utilities/TestSignals.h"

int main (int argc, char** argv)
{
    double seconds = 20.0;
    bool realtime = false;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--seconds") && i + 1 < argc) seconds = std::atof (argv[++i]);
        else if (! std::strcmp (argv[i], "--minutes") && i + 1 < argc) seconds = 60.0 * std::atof (argv[++i]);
        else if (! std::strcmp (argv[i], "--realtime")) realtime = true;
    }

    constexpr double sr = 48000.0;
    constexpr int block = 256;
    amp::AudioEngine engine;
    engine.prepare (sr, block);
    engine.setMonitorEnabled (true);
    amp::ParamSnapshot p = amp::presets::makeFactoryPreset (7);
    engine.publishParams (p);

    auto pink = amp::testsignals::pinkNoise (static_cast<int> (sr), 0.5, 1);
    auto sine = amp::testsignals::sine (997.0, sr, static_cast<int> (sr), 0.5);
    std::vector<float> l (block), r (block);
    std::mt19937 rng (123);

    const long long totalBlocks = static_cast<long long> (seconds * sr / block);
    long long nonFinite = 0, slowBlocks = 0;
    double maxMs = 0.0, sumMs = 0.0;
    std::size_t memStart = 0, memEnd = 0;
    const auto wall0 = std::chrono::steady_clock::now();

    for (long long b = 0; b < totalBlocks; ++b)
    {
        const size_t off = static_cast<size_t> ((b * block) % (static_cast<long long> (sr) - block));
        const float* in[2] = { pink.data() + off, sine.data() + off };
        float* out[2] = { l.data(), r.data() };

        if (b % 400 == 0)                       // mexe nos parametros de vez em quando
        {
            p = amp::presets::makeFactoryPreset (static_cast<int> (rng() % 10));
            p.in[0].gainDb = static_cast<float> (static_cast<int> (rng() % 20)) - 10.f;
            p.bypassAll = (rng() % 7 == 0);
            engine.publishParams (p);
        }

        const auto t0 = std::chrono::steady_clock::now();
        engine.process (in, 2, out, 2, block);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        maxMs = std::max (maxMs, ms);
        sumMs += ms;
        if (ms > 0.8 * 1000.0 * block / sr) ++slowBlocks;

        for (int i = 0; i < block; ++i)
            if (! std::isfinite (l[static_cast<size_t> (i)]) || ! std::isfinite (r[static_cast<size_t> (i)])) ++nonFinite;

        if (b == totalBlocks / 10) memStart = amp::processMemoryBytes();     // depois do aquecimento
        if (realtime) std::this_thread::sleep_for (std::chrono::duration<double> (static_cast<double> (block) / sr));
        if (b % 100000 == 0 && b > 0) { std::printf ("  %.0f%%\n", 100.0 * static_cast<double> (b) / static_cast<double> (totalBlocks)); std::fflush (stdout); }
    }
    memEnd = amp::processMemoryBytes();

    const double growthMb = (static_cast<double> (memEnd) - static_cast<double> (memStart)) / (1024.0 * 1024.0);
    const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - wall0).count();
    std::printf ("Audio simulado: %.0f s em %.1f s (%.0fx tempo real)\n", seconds, wall, seconds / wall);
    std::printf ("Tempo/bloco: medio %.4f ms, max %.3f ms (orcamento %.2f ms)\n",
                 sumMs / static_cast<double> (totalBlocks), maxMs, 1000.0 * block / sr);
    std::printf ("NaN/Inf: %lld | blocos lentos (>80%% do orcamento): %lld | dropouts: %llu | erros: %llu\n",
                 nonFinite, slowBlocks, static_cast<unsigned long long> (engine.dropoutCount()),
                 static_cast<unsigned long long> (engine.errorCount()));
    std::printf ("Memoria: %.1f MB -> %.1f MB (crescimento %.2f MB)\n",
                 static_cast<double> (memStart) / 1048576.0, static_cast<double> (memEnd) / 1048576.0, growthMb);

    bool ok = nonFinite == 0 && engine.errorCount() == 0 && growthMb < 8.0;
    if (realtime) ok = ok && engine.dropoutCount() == 0;      // so em tempo real o dropout e significativo
    std::printf (ok ? "RESULTADO: OK\n" : "RESULTADO: FALHOU\n");
    return ok ? 0 : 1;
}
