#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/Compressor.h"
#include "DSP/HighPassFilter.h"
#include "DSP/Limiter.h"
#include "DSP/ParametricEQ.h"
#include "DSP/StereoDelay.h"
#include "TestHelpers.h"
#include "Utilities/TestSignals.h"

using Catch::Matchers::WithinAbs;
using namespace amp;
using namespace amp::dsp;

namespace
{
    constexpr double kSr = 48000.0;

    // Amplitude (pico) do trecho final do canal esquerdo apos processar seno de amplitude 1.
    template <typename F>
    double measureGainDb (double freq, double sr, int seconds, F&& process)
    {
        const int n = static_cast<int> (sr * seconds);
        auto l = testsignals::sine (freq, sr, n, 1.0);
        auto r = l;
        th::processInBlocks (l, r, 256, process);
        const size_t from = static_cast<size_t> (n) * 3 / 4;
        return 20.0 * std::log10 (th::rms (l, from, l.size()) * std::sqrt (2.0) + 1e-12);   // amplitude de pico estimada via RMS
    }
}

TEST_CASE ("Biquad: bell, shelves, notch, low cut", "[dsp][eq]")
{
    CHECK_THAT (magnitudeDb (makeBiquad (FilterShape::Bell, kSr, 1000, 1.0, 6.0), kSr, 1000), WithinAbs (6.0, 0.01));
    CHECK_THAT (magnitudeDb (makeBiquad (FilterShape::Bell, kSr, 1000, 1.0, -9.0), kSr, 1000), WithinAbs (-9.0, 0.01));
    const auto ls = makeBiquad (FilterShape::LowShelf, kSr, 200, 0.707, 6.0);
    CHECK_THAT (magnitudeDb (ls, kSr, 20), WithinAbs (6.0, 0.15));
    CHECK_THAT (magnitudeDb (ls, kSr, 10000), WithinAbs (0.0, 0.05));
    const auto hs = makeBiquad (FilterShape::HighShelf, kSr, 4000, 0.707, -6.0);
    CHECK_THAT (magnitudeDb (hs, kSr, 18000), WithinAbs (-6.0, 0.3));
    CHECK_THAT (magnitudeDb (hs, kSr, 100), WithinAbs (0.0, 0.05));
    CHECK (magnitudeDb (makeBiquad (FilterShape::Notch, kSr, 1000, 4.0, 0.0), kSr, 1000) < -60.0);
    CHECK_THAT (magnitudeDb (makeBiquad (FilterShape::LowCut, kSr, 100, 0.7071068, 0.0), kSr, 100), WithinAbs (-3.01, 0.02));
    CHECK_THAT (magnitudeDb (makeBiquad (FilterShape::HighCut, kSr, 5000, 0.7071068, 0.0), kSr, 5000), WithinAbs (-3.01, 0.02));
}

TEST_CASE ("HPF: resposta Butterworth analitica (12/18/24/36/48 dB/oct)", "[dsp][hpf]")
{
    const int orders[5] = { 2, 3, 4, 6, 8 };
    for (int slope = 0; slope < 5; ++slope)
        for (double f : { 50.0, 100.0, 200.0, 400.0, 1000.0 })
        {
            const double fc = 200.0;
            const double expected = -10.0 * std::log10 (1.0 + std::pow (fc / f, 2.0 * orders[slope]));
            CHECK_THAT (HighPassFilter::magnitudeDb (kSr, fc, slope, f), WithinAbs (expected, 0.05));
        }
}

TEST_CASE ("HPF: processamento real == resposta teorica", "[dsp][hpf]")
{
    for (int slope : { 0, 2, 4 })
    {
        HighPassFilter hpf;
        hpf.prepare (kSr);
        hpf.setParams (true, 200.0f, slope);
        const double measured = measureGainDb (100.0, kSr, 2, [&] (float* l, float* r, int n) { hpf.process (l, r, n); });
        CHECK_THAT (measured, WithinAbs (HighPassFilter::magnitudeDb (kSr, 200.0, slope, 100.0), 0.15));
    }
}

TEST_CASE ("HPF: desligado e transparente (bit-exato)", "[dsp][hpf]")
{
    HighPassFilter hpf;
    hpf.prepare (kSr);
    hpf.setParams (false, 500.0f, 2);
    auto l = testsignals::pinkNoise (4800, 0.5);
    auto r = l;
    auto ref = l;
    th::processInBlocks (l, r, 128, [&] (float* a, float* b, int n) { hpf.process (a, b, n); });
    CHECK (l == ref);
}

TEST_CASE ("EQ: bell +6 dB em 1 kHz e passagem plana", "[dsp][eq]")
{
    ParametricEQ eq;
    eq.prepare (kSr);
    EqBandParams bands[kNumEqBands];
    for (int i = 0; i < kNumEqBands; ++i) bands[i] = defaultEqBand (i);
    bands[3].type = EqBell; bands[3].freqHz = 1000.f; bands[3].gainDb = 6.f; bands[3].q = 1.f;
    eq.setParams (true, bands);
    const double g1k = measureGainDb (1000.0, kSr, 2, [&] (float* l, float* r, int n) { eq.process (l, r, n); });
    CHECK_THAT (g1k, WithinAbs (6.0, 0.1));
    CHECK_THAT (ParametricEQ::responseDb (true, bands, kSr, 1000.0), WithinAbs (6.0, 0.01));

    ParametricEQ flat;
    flat.prepare (kSr);
    EqBandParams fb[kNumEqBands];
    for (int i = 0; i < kNumEqBands; ++i) fb[i] = defaultEqBand (i);
    flat.setParams (true, fb);
    for (double f : { 100.0, 1000.0, 8000.0 })
        CHECK_THAT (measureGainDb (f, kSr, 1, [&] (float* l, float* r, int n) { flat.process (l, r, n); }), WithinAbs (0.0, 0.05));
}

TEST_CASE ("Compressor: curva estatica (DC) com joelho duro", "[dsp][comp]")
{
    Compressor c;
    c.prepare (kSr);
    c.setParams (true, -20.f, 4.f, 1.f, 50.f, 0.f, 0.f, 1.f);
    std::vector<float> l (48000, 0.5f), r (48000, 0.5f);
    th::processInBlocks (l, r, 256, [&] (float* a, float* b, int n) { c.process (a, b, n); });
    // entrada 0.5 = -6.02 dBFS; reducao = (1 - 1/4) * (-6.02 + 20) = 10.485 dB
    const double expectedDb = -6.0206 - 10.485;
    CHECK_THAT (20.0 * std::log10 (static_cast<double> (l.back())), WithinAbs (expectedDb, 0.05));
    CHECK_THAT (static_cast<double> (c.lastBlockMaxGrDb()), WithinAbs (10.485, 0.05));
}

TEST_CASE ("Compressor: abaixo do threshold nao altera; desligado e transparente", "[dsp][comp]")
{
    Compressor c;
    c.prepare (kSr);
    c.setParams (true, -10.f, 4.f, 1.f, 50.f, 6.f, 0.f, 1.f);
    std::vector<float> l (24000, 0.01f), r (24000, 0.01f);
    th::processInBlocks (l, r, 256, [&] (float* a, float* b, int n) { c.process (a, b, n); });
    CHECK_THAT (static_cast<double> (l.back()), WithinAbs (0.01, 1e-6));

    Compressor off;
    off.prepare (kSr);
    off.setParams (false, -40.f, 20.f, 1.f, 50.f, 0.f, 12.f, 1.f);
    auto n1 = testsignals::pinkNoise (4800, 0.7);
    auto n2 = n1, ref = n1;
    th::processInBlocks (n1, n2, 100, [&] (float* a, float* b, int n) { off.process (a, b, n); });
    CHECK (n1 == ref);
}

TEST_CASE ("Compressor: knee suave reduz menos que joelho duro perto do threshold", "[dsp][comp]")
{
    auto settle = [] (float knee)
    {
        Compressor c;
        c.prepare (kSr);
        c.setParams (true, -20.f, 4.f, 1.f, 50.f, knee, 0.f, 1.f);
        std::vector<float> l (48000, 0.1f), r (48000, 0.1f);   // -20 dBFS = no threshold
        th::processInBlocks (l, r, 256, [&] (float* a, float* b, int n) { c.process (a, b, n); });
        return static_cast<double> (c.lastBlockMaxGrDb());
    };
    CHECK (settle (0.f) < 0.05);
    CHECK (settle (12.f) > 0.3);   // com joelho ja ha reducao no threshold
}

TEST_CASE ("Limiter: nunca excede o ceiling (ruido branco 0 dBFS)", "[dsp][limiter]")
{
    for (double ceilDb : { -0.1, -1.0, -6.0 })
    {
        Limiter lim;
        lim.prepare (kSr);
        lim.setParams (true, static_cast<float> (ceilDb), static_cast<float> (ceilDb), 100.f, 2.f);
        auto l = testsignals::whiteNoise (96000, 1.0, 7);
        auto r = testsignals::whiteNoise (96000, 1.0, 9);
        th::processInBlocks (l, r, 237, [&] (float* a, float* b, int n) { lim.process (a, b, n); });
        const double ceil = std::pow (10.0, ceilDb / 20.0);
        CHECK (th::peak (l) <= ceil * 1.00001);
        CHECK (th::peak (r) <= ceil * 1.00001);
        CHECK (th::allFinite (l));
    }
}

TEST_CASE ("Limiter: sinal abaixo do ceiling passa intacto (com latencia)", "[dsp][limiter]")
{
    Limiter lim;
    lim.prepare (kSr);
    lim.setParams (true, -1.f, -1.f, 100.f, 2.f);
    const int lat = lim.latencySamples();
    CHECK (lat == 96);
    auto in = testsignals::sine (440.0, kSr, 48000, 0.1);
    auto l = in, r = in;
    th::processInBlocks (l, r, 128, [&] (float* a, float* b, int n) { lim.process (a, b, n); });
    for (size_t i = static_cast<size_t> (lat) + 10; i < in.size(); ++i)
        CHECK_THAT (static_cast<double> (l[i]), WithinAbs (static_cast<double> (in[i - static_cast<size_t> (lat)]), 1e-6));
}

TEST_CASE ("Limiter: threshold < ceiling aplica ganho (maximizer)", "[dsp][limiter]")
{
    Limiter lim;
    lim.prepare (kSr);
    lim.setParams (true, -6.f, -1.f, 100.f, 2.f);   // +5 dB
    auto l = testsignals::sine (440.0, kSr, 48000, 0.1);
    auto r = l;
    th::processInBlocks (l, r, 128, [&] (float* a, float* b, int n) { lim.process (a, b, n); });
    CHECK_THAT (th::peak (l, 24000), WithinAbs (0.1 * std::pow (10.0, 5.0 / 20.0), 0.002));
}

TEST_CASE ("Limiter: sem descontinuidade (click) em sinal que excede o ceiling", "[dsp][limiter]")
{
    Limiter lim;
    lim.prepare (kSr);
    lim.setParams (true, -1.f, -1.f, 100.f, 2.f);
    auto in = testsignals::sine (100.0, kSr, 48000, 1.5);
    auto l = in, r = in;
    th::processInBlocks (l, r, 128, [&] (float* a, float* b, int n) { lim.process (a, b, n); });
    double maxJump = 0.0;
    for (size_t i = 1; i < l.size(); ++i) maxJump = std::max (maxJump, std::fabs (static_cast<double> (l[i] - l[i - 1])));
    CHECK (maxJump < 0.05);   // seno de 100 Hz a 48 kHz: passo natural ~0.013
}

TEST_CASE ("Limiter: silencio e entradas extremas nao geram NaN/Inf", "[dsp][limiter]")
{
    Limiter lim;
    lim.prepare (kSr);
    lim.setParams (true, -1.f, -1.f, 100.f, 2.f);
    std::vector<float> l (4800, 0.0f), r (4800, 0.0f);
    th::processInBlocks (l, r, 100, [&] (float* a, float* b, int n) { lim.process (a, b, n); });
    CHECK (th::allFinite (l));
    CHECK (th::peak (l) == 0.0);
    std::vector<float> big (4800, 1000.0f), big2 (4800, -1000.0f);
    th::processInBlocks (big, big2, 100, [&] (float* a, float* b, int n) { lim.process (a, b, n); });
    CHECK (th::allFinite (big));
    CHECK (th::peak (big) <= 0.8913);
}

TEST_CASE ("StereoDelay: atraso exato", "[dsp]")
{
    StereoDelay d;
    d.prepare (200);
    d.setDelay (37);
    auto l = testsignals::impulse (200, 5), r = testsignals::impulse (200, 9);
    d.process (l.data(), r.data(), 200);
    CHECK (l[42] == 1.0f);
    CHECK (r[46] == 1.0f);
    CHECK (th::peak (l, 0, 42) == 0.0);
}
