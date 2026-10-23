#include <limits>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Metering/LoudnessMeter.h"
#include "Metering/TruePeak.h"
#include "TestHelpers.h"
#include "Utilities/TestSignals.h"

using Catch::Matchers::WithinAbs;
using namespace amp;
using namespace amp::metering;

namespace
{
    LoudnessResults measure (const std::vector<float>& l, const std::vector<float>& r, double sr)
    {
        LoudnessMeter m;
        m.prepare (sr);
        for (size_t pos = 0; pos < l.size(); pos += 480)
            m.process (l.data() + pos, r.data() + pos, static_cast<int> (std::min<size_t> (480, l.size() - pos)));
        return m.results();
    }

    // Entrada gradual (evita o transitorio de um inicio abrupto, que e um pico "real" legitimo).
    void fadeIn (std::vector<float>& v, size_t n)
    {
        for (size_t i = 0; i < n && i < v.size(); ++i) v[i] *= static_cast<float> (i) / static_cast<float> (n);
    }

    std::vector<float> concat (std::vector<float> a, const std::vector<float>& b)
    {
        a.insert (a.end(), b.begin(), b.end());
        return a;
    }
}

TEST_CASE ("K-weighting a 48 kHz == coeficientes da norma BS.1770", "[lufs]")
{
    const auto s1 = LoudnessMeter::kWeightStage1 (48000.0);
    CHECK_THAT (s1.b0, WithinAbs (1.53512485958697, 1e-6));
    CHECK_THAT (s1.b1, WithinAbs (-2.69169618940638, 1e-6));
    CHECK_THAT (s1.b2, WithinAbs (1.19839281085285, 1e-6));
    CHECK_THAT (s1.a1, WithinAbs (-1.69065929318241, 1e-6));
    CHECK_THAT (s1.a2, WithinAbs (0.73248077421585, 1e-6));
    const auto s2 = LoudnessMeter::kWeightStage2 (48000.0);
    CHECK_THAT (s2.a1, WithinAbs (-1.99004745483398, 1e-6));
    CHECK_THAT (s2.a2, WithinAbs (0.99007225036621, 1e-6));
}

TEST_CASE ("Seno 1 kHz estereo a -23 dBFS mede -23.0 LUFS (EBU Tech 3341)", "[lufs]")
{
    const double sr = 48000.0;
    auto s = testsignals::sine (1000.0, sr, static_cast<int> (sr * 20), std::pow (10.0, -23.0 / 20.0));
    const auto r = measure (s, s, sr);
    CHECK_THAT (r.momentary, WithinAbs (-23.0, 0.1));
    CHECK_THAT (r.shortTerm, WithinAbs (-23.0, 0.1));
    CHECK_THAT (r.integrated, WithinAbs (-23.0, 0.1));
    CHECK_THAT (r.lra, WithinAbs (0.0, 0.2));
}

TEST_CASE ("Digital full scale: seno 0 dBFS estereo = 0 LUFS e 0 dBTP", "[lufs][tp]")
{
    const double sr = 48000.0;
    auto s = testsignals::sine (1000.0, sr, static_cast<int> (sr * 10), 1.0);
    const auto r = measure (s, s, sr);
    CHECK_THAT (r.integrated, WithinAbs (0.0, 0.1));
    CHECK_THAT (r.truePeakMaxDb, WithinAbs (0.0, 0.1));
}

TEST_CASE ("Silencio digital: sem NaN/Inf e sem valores invalidos", "[lufs]")
{
    const double sr = 48000.0;
    auto z = testsignals::silence (static_cast<int> (sr * 5));
    const auto r = measure (z, z, sr);
    for (double v : { r.momentary, r.shortTerm, r.integrated, r.lra, r.truePeakDb, r.truePeakMaxDb })
        CHECK (std::isfinite (v));
    CHECK (r.momentary == -120.0);
    CHECK (r.integrated == -120.0);
    CHECK (r.truePeakMaxDb == -120.0);
}

TEST_CASE ("Entradas NaN/Inf nao contaminam o medidor", "[lufs]")
{
    const double sr = 48000.0;
    auto s = testsignals::sine (1000.0, sr, static_cast<int> (sr * 5), 0.1);
    s[1000] = std::numeric_limits<float>::quiet_NaN();
    s[2000] = std::numeric_limits<float>::infinity();
    const auto r = measure (s, s, sr);
    CHECK (std::isfinite (r.integrated));
    CHECK (std::isfinite (r.truePeakMaxDb));
}

TEST_CASE ("Gating relativo: trecho 20 LU abaixo nao entra no Integrated", "[lufs]")
{
    const double sr = 48000.0;
    const int n = static_cast<int> (sr * 20);
    auto loud = testsignals::sine (1000.0, sr, n, std::pow (10.0, -20.0 / 20.0));
    auto quiet = testsignals::sine (1000.0, sr, n, std::pow (10.0, -40.0 / 20.0));
    auto s = concat (loud, quiet);
    const auto r = measure (s, s, sr);
    CHECK_THAT (r.integrated, WithinAbs (-20.0, 0.1));
}

TEST_CASE ("Gating absoluto: abaixo de -70 LUFS e ignorado", "[lufs]")
{
    const double sr = 48000.0;
    auto s = testsignals::sine (1000.0, sr, static_cast<int> (sr * 10), std::pow (10.0, -80.0 / 20.0));
    const auto r = measure (s, s, sr);
    CHECK (r.integrated == -120.0);
}

TEST_CASE ("LRA: 20 s a -20 LUFS + 20 s a -30 LUFS ~ 10 LU (EBU Tech 3342, +-1)", "[lufs]")
{
    const double sr = 48000.0;
    const int n = static_cast<int> (sr * 20);
    auto a = testsignals::sine (1000.0, sr, n, std::pow (10.0, -20.0 / 20.0));
    auto b = testsignals::sine (1000.0, sr, n, std::pow (10.0, -30.0 / 20.0));
    auto s = concat (a, b);
    const auto r = measure (s, s, sr);
    CHECK_THAT (r.lra, WithinAbs (10.0, 1.0));
}

TEST_CASE ("Ruido rosa: linearidade (+6.02 dB de ganho = +6.02 LU) e valores finitos", "[lufs]")
{
    const double sr = 48000.0;
    auto p1 = testsignals::pinkNoise (static_cast<int> (sr * 15), 0.1, 3);
    auto p2 = p1;
    for (auto& x : p2) x *= 2.0f;
    const auto r1 = measure (p1, p1, sr), r2 = measure (p2, p2, sr);
    CHECK (std::isfinite (r1.integrated));
    CHECK (r1.integrated > -40.0);
    CHECK_THAT (r2.integrated - r1.integrated, WithinAbs (6.0206, 0.01));
}

TEST_CASE ("LUFS em varias taxas de amostragem (1 kHz, -23 dBFS)", "[lufs]")
{
    for (double sr : { 44100.0, 88200.0, 96000.0 })
    {
        auto s = testsignals::sine (1000.0, sr, static_cast<int> (sr * 12), std::pow (10.0, -23.0 / 20.0));
        const auto r = measure (s, s, sr);
        CHECK_THAT (r.integrated, WithinAbs (-23.0, 0.15));
    }
}

TEST_CASE ("True Peak: seno fs/4 a 45 graus (pico de amostra -3 dB, pico real 0 dBTP)", "[tp]")
{
    const double sr = 48000.0;
    auto s = testsignals::sine (sr / 4.0, sr, 48000, 1.0, kPi / 4.0);
    fadeIn (s, 4800);
    CHECK_THAT (20.0 * std::log10 (th::peak (s, 10000)), WithinAbs (-3.01, 0.05));
    const auto r = measure (s, s, sr);
    CHECK_THAT (r.truePeakMaxDb, WithinAbs (0.0, 0.1));
}

TEST_CASE ("True Peak: senos de varias frequencias/fases nunca subestimam mais que 0.15 dB", "[tp]")
{
    const double sr = 48000.0;
    for (double f : { 100.0, 1000.0, 5000.0, 10000.0, 15000.0 })
        for (double ph : { 0.0, 0.7, 1.9, 3.0 })
        {
            auto s = testsignals::sine (f, sr, 24000, 0.9, ph);
            fadeIn (s, 4800);
            const auto r = measure (s, s, sr);
            const double expected = 20.0 * std::log10 (0.9);
            CHECK (r.truePeakMaxDb >= expected - 0.15);
            CHECK (r.truePeakMaxDb <= expected + 0.05);
        }
}

TEST_CASE ("Reset zera o medidor", "[lufs]")
{
    const double sr = 48000.0;
    auto s = testsignals::sine (1000.0, sr, static_cast<int> (sr * 5), 0.5);
    LoudnessMeter m;
    m.prepare (sr);
    m.process (s.data(), s.data(), static_cast<int> (s.size()));
    CHECK (m.results().integrated > -20.0);
    m.reset();
    CHECK (m.results().integrated == -120.0);
    CHECK (m.results().truePeakMaxDb == -120.0);
}
