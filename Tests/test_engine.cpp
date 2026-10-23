#include <limits>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <random>
#include <thread>

#include "Audio/AudioEngine.h"
#include "Presets/Presets.h"
#include "TestHelpers.h"
#include "Utilities/TestSignals.h"

using Catch::Matchers::WithinAbs;
using namespace amp;

namespace
{
    struct Rig
    {
        AudioEngine engine;
        ParamSnapshot p;
        double sr;
        int block;

        explicit Rig (double sampleRate = 48000.0, int blockSize = 256) : sr (sampleRate), block (blockSize)
        {
            p.limEnabled = false;                       // por padrao: cadeia neutra
            p.eqEnabled = true;
            engine.prepare (sr, block);
            engine.setMonitorEnabled (true);
            engine.publishParams (p);
        }

        // Processa in1/in2 (podem ser vazios) e devolve L/R.
        void run (const std::vector<float>& in1, const std::vector<float>& in2, std::vector<float>& outL, std::vector<float>& outR)
        {
            const size_t total = std::max (in1.size(), in2.size());
            outL.assign (total, 0.f);  outR.assign (total, 0.f);
            std::vector<float> a (in1), b (in2);
            a.resize (total, 0.f);  b.resize (total, 0.f);
            for (size_t pos = 0; pos < total; pos += static_cast<size_t> (block))
            {
                const int n = static_cast<int> (std::min<size_t> (static_cast<size_t> (block), total - pos));
                const float* in[2] = { a.data() + pos, b.data() + pos };
                float* out[2] = { outL.data() + pos, outR.data() + pos };
                engine.process (in, 2, out, 2, n);
            }
        }
    };
}

TEST_CASE ("Engine: silencio digital -> sem NaN/Inf/crash", "[engine]")
{
    Rig rig;
    std::vector<float> l, r;
    auto z = testsignals::silence (48000);
    rig.run (z, z, l, r);
    CHECK (th::allFinite (l));
    CHECK (th::allFinite (r));
    CHECK (th::peak (l) == 0.0);
    CHECK (rig.engine.errorCount() == 0);
}

TEST_CASE ("Engine: gain +6 dB e pan hard-left roteiam so para a esquerda", "[engine][mixer]")
{
    Rig rig;
    rig.p.in[0].gainDb = 6.0206f;
    rig.p.in[0].pan = -1.0f;
    rig.engine.publishParams (rig.p);
    std::vector<float> l, r;
    rig.run (testsignals::sine (1000.0, 48000.0, 48000, 0.1), {}, l, r);
    CHECK_THAT (th::peak (l, 24000), WithinAbs (0.2, 0.004));
    CHECK (th::peak (r, 24000) < 1e-4);
}

TEST_CASE ("Engine: pan central tem -3 dB em cada lado; input 2 -> pan direito", "[engine][mixer]")
{
    Rig rig;
    rig.p.in[1].pan = 1.0f;
    rig.engine.publishParams (rig.p);
    std::vector<float> l, r;
    auto s = testsignals::sine (1000.0, 48000.0, 48000, 0.5);
    rig.run (s, s, l, r);   // in1 centro (0.3536 em cada lado) + in2 direita (0.5)
    CHECK_THAT (th::peak (l, 24000), WithinAbs (0.5 * 0.70711, 0.005));
    CHECK (th::peak (r, 24000) > 0.80);
}

TEST_CASE ("Engine: mute e solo", "[engine][mixer]")
{
    Rig rig;
    auto s = testsignals::sine (1000.0, 48000.0, 48000, 0.5);
    std::vector<float> l, r;

    rig.p.in[0].mute = true;
    rig.engine.publishParams (rig.p);
    rig.run (s, {}, l, r);
    CHECK (th::peak (l, 24000) < 1e-5);

    rig.p.in[0].mute = false;
    rig.p.in[1].solo = true;                 // solo no canal 2 silencia o canal 1
    rig.engine.publishParams (rig.p);
    rig.run (s, {}, l, r);
    CHECK (th::peak (l, 24000) < 1e-5);
    rig.run (s, s, l, r);
    CHECK (th::peak (l, 24000) > 0.2);
}

TEST_CASE ("Engine: monitor desligado = saida muda, medidores seguem funcionando", "[engine]")
{
    Rig rig;
    rig.engine.setMonitorEnabled (false);
    std::vector<float> l, r;
    rig.run (testsignals::sine (1000.0, 48000.0, 96000, 0.5), {}, l, r);
    CHECK (th::peak (l, 48000) < 1e-5);
    CHECK (rig.engine.takeMasterPeak (0) > 0.3f);
    CHECK (rig.engine.takeInputPeak (0) > 0.4f);
}

TEST_CASE ("Engine: bypass entrega o original alinhado pela latencia", "[engine][bypass]")
{
    Rig rig;
    rig.p.in[0].pan = -1.0f;
    rig.p = [&] { auto q = rig.p; q.hpfEnabled = true; q.hpfFreqHz = 2000.f; q.hpfSlope = 4;
                  q.limEnabled = true; q.limLookaheadMs = 2.f; q.bypassAll = true; return q; }();
    rig.engine.publishParams (rig.p);
    auto s = testsignals::pinkNoise (48000, 0.3, 5);
    std::vector<float> l, r;
    rig.run (s, {}, l, r);
    const int lat = rig.engine.latencySamples();
    CHECK (lat == 96);
    for (size_t i = static_cast<size_t> (lat) + 2000; i < s.size(); ++i)
        CHECK_THAT (static_cast<double> (l[i]), WithinAbs (static_cast<double> (s[i - static_cast<size_t> (lat)]), 1e-5));
}

TEST_CASE ("Engine: troca de bypass sem clicks (sem salto brusco)", "[engine][bypass]")
{
    Rig rig;
    rig.p.in[0].pan = -1.0f;
    rig.p.hpfEnabled = true; rig.p.hpfFreqHz = 1000.f;
    rig.p.limEnabled = false;
    rig.engine.publishParams (rig.p);
    auto s = testsignals::sine (200.0, 48000.0, 96000, 0.5);
    std::vector<float> l (s.size()), r (s.size());
    const float zeros[256] = {};
    for (size_t pos = 0; pos < s.size(); pos += 256)
    {
        if (pos == 48128) { rig.p.bypassAll = true; rig.engine.publishParams (rig.p); }
        const float* in[2] = { s.data() + pos, zeros };
        float* out[2] = { l.data() + pos, r.data() + pos };
        rig.engine.process (in, 2, out, 2, 256);
    }
    double maxJump = 0.0;
    for (size_t i = 1; i < l.size(); ++i) maxJump = std::max (maxJump, std::fabs (static_cast<double> (l[i] - l[i - 1])));
    CHECK (maxJump < 0.05);   // 200 Hz amp 0.5: passo natural ~0.013
}

TEST_CASE ("Engine: entradas NaN/Inf sao neutralizadas e contadas", "[engine]")
{
    Rig rig;
    auto s = testsignals::sine (1000.0, 48000.0, 9600, 0.3);
    s[100] = std::numeric_limits<float>::quiet_NaN();
    s[200] = std::numeric_limits<float>::infinity();
    std::vector<float> l, r;
    rig.run (s, s, l, r);
    CHECK (th::allFinite (l));
    CHECK (th::allFinite (r));
    CHECK (rig.engine.errorCount() >= 4);
}

TEST_CASE ("Engine: roteamento com 1 entrada / 1 saida / muitas saidas", "[engine][routing]")
{
    AudioEngine e;
    ParamSnapshot p;  p.limEnabled = false;
    e.prepare (48000.0, 128);
    e.setMonitorEnabled (true);
    e.publishParams (p);
    auto s = testsignals::sine (1000.0, 48000.0, 128 * 400, 0.5);
    std::vector<float> o0 (s.size()), o1 (s.size()), o2 (s.size());
    for (size_t pos = 0; pos < s.size(); pos += 128)
    {
        const float* in[1] = { s.data() + pos };
        float* out[3] = { o0.data() + pos, o1.data() + pos, o2.data() + pos };
        e.process (in, 1, out, 3, 128);
    }
    CHECK (th::peak (o0, 20000) > 0.3);
    CHECK (th::peak (o2, 20000) == 0.0);

    std::vector<float> mono (s.size());
    for (size_t pos = 0; pos < s.size(); pos += 128)
    {
        const float* in[1] = { s.data() + pos };
        float* out[1] = { mono.data() + pos };
        e.process (in, 1, out, 1, 128);
    }
    CHECK (th::peak (mono, 20000) > 0.3);
    float* none[1] = { nullptr };
    e.process (nullptr, 0, none, 1, 128);     // sem entrada e saida nula: nao pode travar
}

TEST_CASE ("Engine: loudness medido na saida (thread de metering)", "[engine][lufs]")
{
    Rig rig;
    rig.p.in[0].pan = -1.0f;
    rig.p.in[1].pan = 1.0f;
    rig.engine.publishParams (rig.p);
    auto s = testsignals::sine (1000.0, 48000.0, 48000 * 6, std::pow (10.0, -23.0 / 20.0));
    std::vector<float> l, r;
    rig.run (s, s, l, r);                      // L = R = -23 dBFS (cada canal vai para um lado)
    std::this_thread::sleep_for (std::chrono::milliseconds (600));
    const auto res = rig.engine.loudness();
    CHECK_THAT (res.integrated, WithinAbs (-23.0, 0.2));
    CHECK (std::isfinite (res.truePeakMaxDb));
    rig.engine.resetLoudness();
    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    CHECK (rig.engine.loudness().integrated == -120.0);
}

TEST_CASE ("Engine: todas as combinacoes de taxa x buffer x frequencia -> saida finita", "[engine][signals]")
{
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int block : { 32, 64, 128, 256, 512, 1024 })
        {
            Rig rig (sr, block);
            rig.p = presets::makeFactoryPreset (9);
            rig.engine.publishParams (rig.p);
            for (double f : { 20.0, 50.0, 100.0, 1000.0, 5000.0, 10000.0, 20000.0 })
            {
                std::vector<float> l, r;
                rig.run (testsignals::sine (f, sr, static_cast<int> (sr / 10), 0.8), {}, l, r);
                CHECK (th::allFinite (l));
                CHECK (th::allFinite (r));
                CHECK (th::peak (l) < 1.0);
            }
        }
}

TEST_CASE ("Engine: geradores (ruido branco, rosa, impulso) com cadeia completa", "[engine][signals]")
{
    Rig rig;
    rig.p = presets::makeFactoryPreset (7);
    rig.engine.publishParams (rig.p);
    std::vector<float> l, r;
    rig.run (testsignals::whiteNoise (48000, 1.0, 11), testsignals::pinkNoise (48000, 1.0, 12), l, r);
    CHECK (th::allFinite (l));
    CHECK (th::peak (l) <= std::pow (10.0, -1.0 / 20.0) * 1.0001);   // ceiling -1 dB do preset Master
    rig.run (testsignals::impulse (4800, 100, 1.0), {}, l, r);
    CHECK (th::allFinite (l));
}

TEST_CASE ("Engine: fuzz de parametros durante o processamento nao gera NaN/Inf", "[engine][stress]")
{
    Rig rig;
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> u (-1.f, 1.f);
    auto noise = testsignals::pinkNoise (256, 0.5, 21);
    std::vector<float> l (256), r (256);
    for (int iter = 0; iter < 600; ++iter)
    {
        ParamSnapshot p;
        p.in[0].gainDb = 40.f * u (rng);  p.in[1].gainDb = 40.f * u (rng);
        p.in[0].pan = 1.5f * u (rng);     p.in[1].pan = 1.5f * u (rng);
        p.in[0].mute = u (rng) > 0.8f;    p.in[1].solo = u (rng) > 0.8f;
        p.hpfEnabled = u (rng) > 0; p.hpfFreqHz = 1000.f + 1500.f * u (rng); p.hpfSlope = static_cast<int> (3.f * (u (rng) + 1.f));
        for (int b = 0; b < kNumEqBands; ++b)
        {
            p.eq[b].type = static_cast<int> (3.f * (u (rng) + 1.f)); p.eq[b].freqHz = 10000.f * (u (rng) + 1.1f);
            p.eq[b].gainDb = 30.f * u (rng); p.eq[b].q = 10.f * (u (rng) + 1.f); p.eq[b].enabled = u (rng) > -0.5f;
        }
        p.compEnabled = u (rng) > 0; p.compThresholdDb = -70.f * (u (rng) + 1.f) / 2.f; p.compRatio = 12.f * (u (rng) + 1.f);
        p.compAttackMs = 100.f * (u (rng) + 1.f); p.compReleaseMs = 1000.f * (u (rng) + 1.f); p.compKneeDb = 30.f * (u (rng) + 1.f);
        p.compMakeupDb = 12.f * (u (rng) + 1.f); p.compMix = (u (rng) + 1.f) / 2.f;
        p.limEnabled = u (rng) > -0.5f; p.limThresholdDb = -30.f * (u (rng) + 1.f) / 2.f; p.limCeilingDb = -12.f * (u (rng) + 1.f) / 2.f;
        p.limLookaheadMs = 12.f * (u (rng) + 1.f) / 2.f; p.limReleaseMs = 1200.f * (u (rng) + 1.f) / 2.f;
        p.bypassAll = u (rng) > 0.7f;
        if (iter % 50 == 0) p.compMakeupDb = std::numeric_limits<float>::quiet_NaN();   // valor invalido
        rig.engine.publishParams (p);
        const float* in[2] = { noise.data(), noise.data() };
        float* out[2] = { l.data(), r.data() };
        rig.engine.process (in, 2, out, 2, 256);
        REQUIRE (th::allFinite (l));
        REQUIRE (th::allFinite (r));
    }
}
