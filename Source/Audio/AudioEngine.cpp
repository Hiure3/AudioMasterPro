#include "AudioEngine.h"

#include <chrono>
#include <cmath>

#include "Utilities/DbUtils.h"
#include "Utilities/DenormalGuard.h"

namespace amp
{
    namespace
    {
        inline void storeMax (std::atomic<float>& a, float v) noexcept
        {
            if (v > a.load (std::memory_order_relaxed))
                a.store (v, std::memory_order_relaxed);
        }

        inline float cleanSample (float x) noexcept
        {
            return (std::isfinite (x) && std::fabs (x) < 1.0e6f) ? x : 0.0f;
        }
    }

    AudioEngine::AudioEngine()
    {
        for (auto& p : inPeak_) p.store (0.0f);
        for (auto& p : masterPeak_) p.store (0.0f);
    }

    AudioEngine::~AudioEngine() { release(); }

    void AudioEngine::prepare (double sampleRate, int maxBlockSize)
    {
        prepared_.store (false);
        meter_.stop();

        sr_ = sampleRate > 1000.0 ? sampleRate : 48000.0;
        maxBlock_ = std::max (16, maxBlockSize);
        const auto sz = static_cast<size_t> (maxBlock_);
        mixL_.assign (sz, 0.0f);  mixR_.assign (sz, 0.0f);
        dryL_.assign (sz, 0.0f);  dryR_.assign (sz, 0.0f);
        mono_.assign (sz, 0.0f);

        for (int s = 0; s < kNumInputs; ++s)
        {
            inGain_[s].reset (sr_, 0.02);  inGain_[s].setCurrentAndTarget (1.0f);
            panL_[s].reset (sr_, 0.02);    panL_[s].setCurrentAndTarget (0.7071f);
            panR_[s].reset (sr_, 0.02);    panR_[s].setCurrentAndTarget (0.7071f);
        }
        procMix_.reset (sr_, 0.02);     procMix_.setCurrentAndTarget (1.0f);
        masterGain_.reset (sr_, 0.02);  masterGain_.setCurrentAndTarget (1.0f);
        monitorMix_.reset (sr_, 0.03);  monitorMix_.setCurrentAndTarget (0.0f);

        hpf_.prepare (sr_);
        eq_.prepare (sr_);
        comp_.prepare (sr_);
        limiter_.prepare (sr_);
        dryDelay_.prepare (static_cast<int> (std::ceil (dsp::Limiter::kMaxLookaheadMs * 0.001 * sr_)) + 4);
        preFifo_.clear();
        postFifo_.clear();

        paramBuf_.update();
        params_ = paramBuf_.read();
        sanitize (params_);
        needInit_ = true;
        cpu_.store (0.0);

        meter_.start (sr_);
        prepared_.store (true);
    }

    void AudioEngine::release()
    {
        prepared_.store (false);
        meter_.stop();
    }

    void AudioEngine::publishParams (const ParamSnapshot& p)
    {
        ParamSnapshot s = p;
        sanitize (s);
        paramBuf_.publish (s);
    }

    float AudioEngine::takeInputPeak (int ch) noexcept { return inPeak_[ch < 0 ? 0 : (ch >= kNumInputs ? kNumInputs - 1 : ch)].exchange (0.0f); }
    float AudioEngine::takeMasterPeak (int ch) noexcept { return masterPeak_[ch <= 0 ? 0 : 1].exchange (0.0f); }
    float AudioEngine::takeCompressorGrDb() noexcept { return compGr_.exchange (0.0f); }
    float AudioEngine::takeLimiterGrDb() noexcept { return limGr_.exchange (0.0f); }

    void AudioEngine::applyParams (bool snap) noexcept
    {
        const ParamSnapshot& p = params_;
        bool anySolo = false;
        for (const auto& c : p.in) anySolo = anySolo || c.solo;

        for (int s = 0; s < kNumInputs; ++s)
        {
            const auto& c = p.in[s];
            const bool audible = ! c.mute && (! anySolo || c.solo);
            const float g = audible ? dbToLinear (c.gainDb) : 0.0f;
            const float theta = static_cast<float> ((c.pan + 1.0f) * kPi / 4.0);   // lei de pan de -3 dB
            if (snap)
            {
                inGain_[s].setCurrentAndTarget (g);
                panL_[s].setCurrentAndTarget (std::cos (theta));
                panR_[s].setCurrentAndTarget (std::sin (theta));
            }
            inGain_[s].setTarget (g);
            panL_[s].setTarget (std::cos (theta));
            panR_[s].setTarget (std::sin (theta));
        }

        const float mg = p.masterMute ? 0.0f : dbToLinear (p.masterGainDb);
        const float pm = p.bypassAll ? 0.0f : 1.0f;
        if (snap)
        {
            masterGain_.setCurrentAndTarget (mg);
            procMix_.setCurrentAndTarget (pm);
        }
        masterGain_.setTarget (mg);
        procMix_.setTarget (pm);

        hpf_.setParams (p.hpfEnabled, p.hpfFreqHz, p.hpfSlope);
        eq_.setParams (p.eqEnabled, p.eq);
        comp_.setParams (p.compEnabled, p.compThresholdDb, p.compRatio, p.compAttackMs, p.compReleaseMs,
                         p.compKneeDb, p.compMakeupDb, p.compMix);
        limiter_.setParams (p.limEnabled, p.limThresholdDb, p.limCeilingDb, p.limReleaseMs, p.limLookaheadMs);
        dryDelay_.setDelay (limiter_.latencySamples());
        latency_.store (limiter_.latencySamples(), std::memory_order_relaxed);
    }

    void AudioEngine::process (const float* const* in, int numIn, float* const* out, int numOut, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        if (! prepared_.load (std::memory_order_acquire))
        {
            for (int c = 0; c < numOut; ++c)
                if (out[c] != nullptr)
                    for (int i = 0; i < numSamples; ++i) out[c][i] = 0.0f;
            return;
        }

        const auto t0 = std::chrono::steady_clock::now();
        ScopedNoDenormals noDenormals;

        if (paramBuf_.update())
        {
            params_ = paramBuf_.read();
            sanitize (params_);
        }

        int done = 0;
        const float* inPtrs[kNumInputs] = {};
        float* outPtrs[64] = {};
        const int nOut = numOut < 64 ? numOut : 64;
        while (done < numSamples)
        {
            const int n = std::min (maxBlock_, numSamples - done);
            for (int c = 0; c < kNumInputs; ++c)
                inPtrs[c] = (c < numIn && in != nullptr && in[c] != nullptr) ? in[c] + done : nullptr;
            for (int c = 0; c < nOut; ++c)
                outPtrs[c] = out[c] != nullptr ? out[c] + done : nullptr;
            processChunk (inPtrs, numIn < kNumInputs ? numIn : kNumInputs, outPtrs, nOut, n);
            done += n;
        }

        const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double budget = numSamples / sr_;
        const double load = 100.0 * elapsed / budget;
        cpu_.store (cpu_.load (std::memory_order_relaxed) * 0.9 + load * 0.1, std::memory_order_relaxed);
        if (elapsed > budget)
            dropouts_.fetch_add (1, std::memory_order_relaxed);
    }

    void AudioEngine::processChunk (const float* const* in, int numIn, float* const* out, int numOut, int n) noexcept
    {
        applyParams (needInit_);
        needInit_ = false;

        float* mixL = mixL_.data();
        float* mixR = mixR_.data();
        for (int i = 0; i < n; ++i) { mixL[i] = 0.0f; mixR[i] = 0.0f; }

        // ---- Mixer: gain -> pan/mute/solo -> soma ----
        std::uint64_t bad = 0;
        for (int s = 0; s < kNumInputs; ++s)
        {
            const float* src = (s < numIn) ? in[s] : nullptr;
            float peak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float x = 0.0f;
                if (src != nullptr)
                {
                    x = src[i];
                    if (! (std::isfinite (x) && std::fabs (x) < 1.0e6f)) { x = 0.0f; ++bad; }
                }
                const float y = x * inGain_[s].next();
                peak = std::max (peak, std::fabs (y));
                mixL[i] += y * panL_[s].next();
                mixR[i] += y * panR_[s].next();
            }
            storeMax (inPeak_[s], peak);
        }
        if (bad > 0)
            errors_.fetch_add (bad, std::memory_order_relaxed);

        // ---- Copia "original" (para o bypass) ----
        for (int i = 0; i < n; ++i) { dryL_[static_cast<size_t> (i)] = mixL[i]; dryR_[static_cast<size_t> (i)] = mixR[i]; }

        // ---- Espectro de entrada ----
        for (int i = 0; i < n; ++i) mono_[static_cast<size_t> (i)] = 0.5f * (mixL[i] + mixR[i]);
        preFifo_.push (mono_.data(), static_cast<size_t> (n));

        // ---- Cadeia de DSP ----
        hpf_.process (mixL, mixR, n);
        eq_.process (mixL, mixR, n);
        for (int i = 0; i < n; ++i) mono_[static_cast<size_t> (i)] = 0.5f * (mixL[i] + mixR[i]);
        postFifo_.push (mono_.data(), static_cast<size_t> (n));

        comp_.process (mixL, mixR, n);
        limiter_.process (mixL, mixR, n);
        storeMax (compGr_, comp_.lastBlockMaxGrDb());
        storeMax (limGr_, limiter_.lastBlockMaxGrDb());

        // ---- Bypass (original alinhado pela latencia do limiter) + master ----
        float* dryL = dryL_.data();
        float* dryR = dryR_.data();
        dryDelay_.process (dryL, dryR, n);

        float pkL = 0.0f, pkR = 0.0f;
        std::uint64_t badOut = 0;
        for (int i = 0; i < n; ++i)
        {
            const float b = procMix_.next();
            const float mg = masterGain_.next();
            float l = (dryL[i] + (mixL[i] - dryL[i]) * b) * mg;
            float r = (dryR[i] + (mixR[i] - dryR[i]) * b) * mg;
            if (! std::isfinite (l)) { l = 0.0f; ++badOut; }
            if (! std::isfinite (r)) { r = 0.0f; ++badOut; }
            mixL[i] = l;
            mixR[i] = r;
            pkL = std::max (pkL, std::fabs (l));
            pkR = std::max (pkR, std::fabs (r));
        }
        if (badOut > 0)
            errors_.fetch_add (badOut, std::memory_order_relaxed);
        storeMax (masterPeak_[0], pkL);
        storeMax (masterPeak_[1], pkR);

        // ---- Medicao de loudness / true peak (a saida final, antes do mute do monitor) ----
        meter_.push (mixL, mixR, n);

        // ---- Monitor (saida) ----
        monitorMix_.setTarget (monitor_.load (std::memory_order_relaxed) ? 1.0f : 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float mm = monitorMix_.next();
            const float l = mixL[i] * mm, r = mixR[i] * mm;
            if (numOut == 1)
            {
                if (out[0] != nullptr) out[0][i] = 0.5f * (l + r);
            }
            else
            {
                if (numOut > 0 && out[0] != nullptr) out[0][i] = l;
                if (numOut > 1 && out[1] != nullptr) out[1][i] = r;
                for (int c = 2; c < numOut; ++c)
                    if (out[c] != nullptr) out[c][i] = 0.0f;
            }
        }
    }
}
