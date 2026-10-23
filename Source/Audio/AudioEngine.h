#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "DSP/Compressor.h"
#include "DSP/HighPassFilter.h"
#include "DSP/Limiter.h"
#include "DSP/ParametricEQ.h"
#include "DSP/StereoDelay.h"
#include "Metering/MeteringWorker.h"
#include "Parameters.h"
#include "Utilities/LockFreeFifo.h"
#include "Utilities/SmoothedValue.h"
#include "Utilities/TripleBuffer.h"

namespace amp
{
    // Motor de audio (sem dependencia de JUCE). Cadeia:
    //   INPUT 1/2 -> GAIN -> PAN/MUTE/SOLO -> soma (mix bus)
    //   mix bus -> HPF -> EQ(8) -> COMPRESSOR -> LIMITER -> (bypass crossfade) -> MASTER
    //   -> medidores LUFS/True Peak (thread propria) -> MONITOR -> saida
    // process() nao aloca, nao usa mutex e nao acessa disco/UI.
    class AudioEngine
    {
    public:
        AudioEngine();
        ~AudioEngine();

        // Nao-realtime: aloca tudo e (re)inicia a thread de metering.
        void prepare (double sampleRate, int maxBlockSize);
        void release();

        // Thread de UI: publica um novo estado de parametros (sanitizado internamente).
        void publishParams (const ParamSnapshot& p);

        // Thread de audio.
        void process (const float* const* in, int numIn, float* const* out, int numOut, int numSamples) noexcept;

        void setMonitorEnabled (bool on) noexcept { monitor_.store (on); }
        bool isMonitorEnabled() const noexcept { return monitor_.load(); }

        // ---- Leituras para a UI (thread-safe) ----
        float takeInputPeak (int ch) noexcept;           // linear, desde a ultima leitura
        float takeMasterPeak (int ch) noexcept;
        float takeCompressorGrDb() noexcept;
        float takeLimiterGrDb() noexcept;
        metering::LoudnessResults loudness() noexcept { return meter_.latest(); }
        void resetLoudness() noexcept { meter_.requestReset(); }
        LockFreeFifo<float>& preSpectrumFifo() noexcept { return preFifo_; }
        LockFreeFifo<float>& postSpectrumFifo() noexcept { return postFifo_; }

        double sampleRate() const noexcept { return sr_; }
        int latencySamples() const noexcept { return latency_.load(); }   // lookahead do limiter
        double audioCpuPercent() const noexcept { return cpu_.load(); }
        std::uint64_t dropoutCount() const noexcept { return dropouts_.load(); }
        std::uint64_t errorCount() const noexcept { return errors_.load(); }
        void resetCounters() noexcept { dropouts_.store (0); errors_.store (0); }

    private:
        void processChunk (const float* const* in, int numIn, float* const* out, int numOut, int n) noexcept;
        void applyParams (bool snap) noexcept;

        double sr_ = 48000.0;
        int maxBlock_ = 0;
        std::atomic<bool> prepared_ { false };
        bool needInit_ = true;

        TripleBuffer<ParamSnapshot> paramBuf_;
        ParamSnapshot params_;

        std::vector<float> mixL_, mixR_, dryL_, dryR_, mono_;
        SmoothedValue inGain_[kNumInputs], panL_[kNumInputs], panR_[kNumInputs];
        SmoothedValue procMix_, masterGain_, monitorMix_;

        dsp::HighPassFilter hpf_;
        dsp::ParametricEQ eq_;
        dsp::Compressor comp_;
        dsp::Limiter limiter_;
        dsp::StereoDelay dryDelay_;

        LockFreeFifo<float> preFifo_ { 1u << 16 }, postFifo_ { 1u << 16 };
        metering::MeteringWorker meter_;

        std::atomic<bool> monitor_ { false };
        std::atomic<float> inPeak_[kNumInputs], masterPeak_[2], compGr_ { 0.0f }, limGr_ { 0.0f };
        std::atomic<double> cpu_ { 0.0 };
        std::atomic<int> latency_ { 0 };
        std::atomic<std::uint64_t> dropouts_ { 0 }, errors_ { 0 };
    };
}
