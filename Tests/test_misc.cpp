#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>
#include <thread>

#include "Metering/SpectrumAnalyzer.h"
#include "Presets/Presets.h"
#include "TestHelpers.h"
#include "Utilities/LockFreeFifo.h"
#include "Utilities/SmoothedValue.h"
#include "Utilities/TestSignals.h"
#include "Utilities/TripleBuffer.h"

using Catch::Matchers::WithinAbs;
using namespace amp;

TEST_CASE ("FFT/Spectrum: seno de 1 kHz aparece no bin certo com ~0 dBFS", "[fft]")
{
    for (int size : { 1024, 2048, 4096, 8192, 16384, 32768 })
    {
        LockFreeFifo<float> fifo (1u << 17);
        metering::SpectrumAnalyzer sa (fifo);
        sa.setSampleRate (48000.0);
        sa.setFftSize (size);
        auto s = testsignals::sine (1000.0, 48000.0, size * 2, 1.0);
        fifo.push (s.data(), s.size());
        sa.update();
        const auto& m = sa.magnitudesDb();
        int best = 0;
        for (int k = 1; k < static_cast<int> (m.size()); ++k) if (m[static_cast<size_t> (k)] > m[static_cast<size_t> (best)]) best = k;
        CHECK_THAT (sa.binToHz (best), WithinAbs (1000.0, 48000.0 / size * 1.01));
        CHECK (static_cast<double> (m[static_cast<size_t> (best)]) > -1.6);
        CHECK (static_cast<double> (m[static_cast<size_t> (best)]) < 0.1);
        for (float v : m) CHECK (std::isfinite (v));
    }
}

TEST_CASE ("Spectrum: silencio -> piso finito", "[fft]")
{
    LockFreeFifo<float> fifo (1u << 14);
    metering::SpectrumAnalyzer sa (fifo);
    auto z = testsignals::silence (8192);
    fifo.push (z.data(), z.size());
    sa.update();
    for (float v : sa.magnitudesDb()) CHECK (v <= -100.0f);
}

TEST_CASE ("LockFreeFifo: ordem preservada entre threads, sem perdas", "[fifo]")
{
    LockFreeFifo<float> fifo (1u << 10);
    constexpr int total = 200000;
    std::thread producer ([&]
    {
        for (int i = 0; i < total;)
        {
            const float v = static_cast<float> (i);
            if (fifo.push (&v, 1) == 1) ++i;
        }
    });
    int expected = 0;
    bool ok = true;
    while (expected < total)
    {
        float v;
        if (fifo.pop (&v, 1) == 1) { if (v != static_cast<float> (expected)) ok = false; ++expected; }
    }
    producer.join();
    CHECK (ok);
}

TEST_CASE ("TripleBuffer: leitor sempre ve valores completos e nunca antigos demais", "[fifo]")
{
    struct Big { int a[16]; };
    TripleBuffer<Big> tb;
    std::atomic<bool> stop { false };
    std::thread producer ([&]
    {
        for (int i = 1; i <= 100000; ++i) { Big b; for (int& x : b.a) x = i; tb.publish (b); }
        stop = true;
    });
    int last = 0;
    bool consistent = true, monotonic = true;
    while (! stop.load())
        if (tb.update())
        {
            const Big& b = tb.read();
            for (int x : b.a) if (x != b.a[0]) consistent = false;
            if (b.a[0] < last) monotonic = false;
            last = b.a[0];
        }
    producer.join();
    CHECK (consistent);
    CHECK (monotonic);
}

TEST_CASE ("SmoothedValue: rampa linear e chega ao alvo", "[util]")
{
    SmoothedValue s;
    s.reset (48000.0, 0.01);          // 480 amostras
    s.setCurrentAndTarget (0.0f);
    s.setTarget (1.0f);
    float v = 0.f;
    for (int i = 0; i < 240; ++i) v = s.next();
    CHECK_THAT (static_cast<double> (v), WithinAbs (0.5, 0.01));
    for (int i = 0; i < 300; ++i) v = s.next();
    CHECK (v == 1.0f);
    CHECK (! s.isSmoothing());
}

TEST_CASE ("Presets de fabrica: 10 presets validos, estaveis em sanitize e ida/volta em texto", "[presets]")
{
    CHECK (presets::numFactoryPresets() == 10);
    CHECK (std::string (presets::factoryPresetName (0)) == "Default");
    CHECK (std::string (presets::factoryPresetName (9)) == "Church Live");
    for (int i = 0; i < presets::numFactoryPresets(); ++i)
    {
        ParamSnapshot p = presets::makeFactoryPreset (i);
        const std::string text = presets::serialize (p);
        ParamSnapshot q;
        REQUIRE (presets::parse (text, q));
        CHECK (presets::serialize (q) == text);        // ida e volta identica
        ParamSnapshot s = p;
        sanitize (s);
        CHECK (presets::serialize (s) == text);        // ja estava em faixa
    }
}

TEST_CASE ("Presets: texto invalido/malicioso e rejeitado ou sanitizado", "[presets]")
{
    ParamSnapshot q;
    CHECK_FALSE (presets::parse ("", q));
    CHECK_FALSE (presets::parse ("nao e um preset\nfoo=bar", q));
    REQUIRE (presets::parse ("AMP_PRESET 1\nin0.gainDb=nan\nlim.ceilingDb=999\nhpf.slope=77\neq3.freqHz=-5\nchave.desconhecida=1\n=\n", q));
    CHECK (std::isfinite (q.in[0].gainDb));
    CHECK (q.limCeilingDb <= 0.0f);
    CHECK (q.hpfSlope == 4);
    CHECK (q.eq[3].freqHz >= 20.0f);
}

TEST_CASE ("Compressor presets alteram so os parametros do compressor", "[presets]")
{
    ParamSnapshot p;
    const float lim = p.limCeilingDb;
    presets::applyCompressorPreset (4, p);   // Master
    CHECK_THAT (static_cast<double> (p.compRatio), WithinAbs (1.8, 1e-6));
    CHECK (p.limCeilingDb == lim);
}

TEST_CASE ("Geradores de sinal: amplitude e finitude", "[signals]")
{
    for (double f : { 20.0, 50.0, 100.0, 1000.0, 5000.0, 10000.0, 20000.0 })
    {
        auto s = testsignals::sine (f, 48000.0, 48000, 0.5);
        CHECK (th::allFinite (s));
        CHECK_THAT (th::peak (s), WithinAbs (0.5, 1e-3));
    }
    CHECK (th::peak (testsignals::silence (100)) == 0.0);
    CHECK (th::allFinite (testsignals::pinkNoise (48000, 1.0)));
    CHECK (th::peak (testsignals::impulse (100, 10, 0.7)) > 0.69);
}
