#pragma once

#include <string>
#include <type_traits>

#include "Utilities/DbUtils.h"

// Estado completo de parametros (POD). A UI edita uma copia e publica para a audio thread
// por um TripleBuffer; A/B e presets sao snapshots deste struct.
namespace amp
{
    inline constexpr int kNumInputs = 2;
    inline constexpr int kNumEqBands = 8;

    // Tipos de banda do EQ (mesma ordem de dsp::FilterShape).
    enum EqType { EqBell = 0, EqLowShelf, EqHighShelf, EqLowCut, EqHighCut, EqNotch, EqNumTypes };

    namespace ranges
    {
        inline constexpr float kGainMin = -60.0f, kGainMax = 24.0f;
        inline constexpr float kMasterMin = -60.0f, kMasterMax = 0.0f;
        inline constexpr float kHpfMin = 20.0f, kHpfMax = 2000.0f;
        inline constexpr float kEqFreqMin = 20.0f, kEqFreqMax = 20000.0f;
        inline constexpr float kEqGainMin = -18.0f, kEqGainMax = 18.0f;
        inline constexpr float kEqQMin = 0.1f, kEqQMax = 18.0f;
        inline constexpr float kThrMin = -60.0f, kThrMax = 0.0f;
        inline constexpr float kRatioMin = 1.0f, kRatioMax = 20.0f;
        inline constexpr float kAttackMin = 0.1f, kAttackMax = 200.0f;
        inline constexpr float kReleaseMin = 5.0f, kReleaseMax = 2000.0f;
        inline constexpr float kKneeMin = 0.0f, kKneeMax = 24.0f;
        inline constexpr float kMakeupMin = 0.0f, kMakeupMax = 24.0f;
        inline constexpr float kLimThrMin = -24.0f, kLimThrMax = 0.0f;
        inline constexpr float kLimCeilMin = -12.0f, kLimCeilMax = 0.0f;
        inline constexpr float kLimRelMin = 10.0f, kLimRelMax = 1000.0f;
        inline constexpr float kLookMin = 0.5f, kLookMax = 10.0f;
        inline constexpr float kTargetMin = -40.0f, kTargetMax = -6.0f;
        inline constexpr float kMaxTpMin = -3.0f, kMaxTpMax = 0.0f;
    }

    struct EqBandParams
    {
        int type = EqBell;
        float freqHz = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        bool enabled = true;
    };

    struct ChannelParams
    {
        float gainDb = 0.0f;
        float pan = 0.0f;       // -1 (esq) .. +1 (dir)
        bool mute = false;
        bool solo = false;
    };

    inline EqBandParams defaultEqBand (int i)
    {
        static constexpr float freqs[kNumEqBands] = { 80.f, 200.f, 500.f, 1000.f, 2500.f, 5000.f, 9000.f, 14000.f };
        EqBandParams b;
        b.freqHz = freqs[i < 0 ? 0 : (i >= kNumEqBands ? kNumEqBands - 1 : i)];
        b.type = (i == 0) ? EqLowShelf : (i == kNumEqBands - 1 ? EqHighShelf : EqBell);
        b.q = (b.type == EqBell) ? 1.0f : 0.707f;
        return b;
    }

    struct ParamSnapshot
    {
        ChannelParams in[kNumInputs];
        float masterGainDb = 0.0f;
        bool masterMute = false;

        bool hpfEnabled = false;
        float hpfFreqHz = 80.0f;
        int hpfSlope = 2;               // 0..4 = 12, 18, 24, 36, 48 dB/oct

        bool eqEnabled = true;
        EqBandParams eq[kNumEqBands];

        bool compEnabled = false;
        float compThresholdDb = -18.0f, compRatio = 3.0f, compAttackMs = 10.0f, compReleaseMs = 120.0f;
        float compKneeDb = 6.0f, compMakeupDb = 0.0f, compMix = 1.0f;

        bool limEnabled = true;
        float limThresholdDb = -1.0f, limCeilingDb = -1.0f, limReleaseMs = 100.0f, limLookaheadMs = 2.0f;

        float targetLufs = -14.0f;       // so UI / Loudness Target
        float maxTruePeakDb = -1.0f;

        bool bypassAll = false;

        ParamSnapshot() { for (int i = 0; i < kNumEqBands; ++i) eq[i] = defaultEqBand (i); }
    };

    // Coloca todos os valores em faixa valida (NaN/Inf viram o padrao). Seguro para a audio thread.
    inline void sanitize (ParamSnapshot& p) noexcept
    {
        using namespace ranges;
        const ParamSnapshot d;
        for (int i = 0; i < kNumInputs; ++i)
        {
            p.in[i].gainDb = clampFinite (p.in[i].gainDb, kGainMin, kGainMax, 0.f);
            p.in[i].pan = clampFinite (p.in[i].pan, -1.f, 1.f, 0.f);
        }
        p.masterGainDb = clampFinite (p.masterGainDb, kMasterMin, kMasterMax, 0.f);
        p.hpfFreqHz = clampFinite (p.hpfFreqHz, kHpfMin, kHpfMax, d.hpfFreqHz);
        p.hpfSlope = p.hpfSlope < 0 ? 0 : (p.hpfSlope > 4 ? 4 : p.hpfSlope);
        for (int i = 0; i < kNumEqBands; ++i)
        {
            auto& b = p.eq[i];
            b.type = b.type < 0 ? 0 : (b.type >= EqNumTypes ? EqNumTypes - 1 : b.type);
            b.freqHz = clampFinite (b.freqHz, kEqFreqMin, kEqFreqMax, defaultEqBand (i).freqHz);
            b.gainDb = clampFinite (b.gainDb, kEqGainMin, kEqGainMax, 0.f);
            b.q = clampFinite (b.q, kEqQMin, kEqQMax, 1.f);
        }
        p.compThresholdDb = clampFinite (p.compThresholdDb, kThrMin, kThrMax, d.compThresholdDb);
        p.compRatio = clampFinite (p.compRatio, kRatioMin, kRatioMax, d.compRatio);
        p.compAttackMs = clampFinite (p.compAttackMs, kAttackMin, kAttackMax, d.compAttackMs);
        p.compReleaseMs = clampFinite (p.compReleaseMs, kReleaseMin, kReleaseMax, d.compReleaseMs);
        p.compKneeDb = clampFinite (p.compKneeDb, kKneeMin, kKneeMax, d.compKneeDb);
        p.compMakeupDb = clampFinite (p.compMakeupDb, kMakeupMin, kMakeupMax, 0.f);
        p.compMix = clampFinite (p.compMix, 0.f, 1.f, 1.f);
        p.limThresholdDb = clampFinite (p.limThresholdDb, kLimThrMin, kLimThrMax, d.limThresholdDb);
        p.limCeilingDb = clampFinite (p.limCeilingDb, kLimCeilMin, kLimCeilMax, d.limCeilingDb);
        p.limReleaseMs = clampFinite (p.limReleaseMs, kLimRelMin, kLimRelMax, d.limReleaseMs);
        p.limLookaheadMs = clampFinite (p.limLookaheadMs, kLookMin, kLookMax, d.limLookaheadMs);
        p.targetLufs = clampFinite (p.targetLufs, kTargetMin, kTargetMax, d.targetLufs);
        p.maxTruePeakDb = clampFinite (p.maxTruePeakDb, kMaxTpMin, kMaxTpMax, d.maxTruePeakDb);
    }

    // Percorre todos os campos com um nome estavel (usado para salvar/carregar presets).
    template <typename V>
    void visitParams (ParamSnapshot& p, V&& v)
    {
        for (int i = 0; i < kNumInputs; ++i)
        {
            const std::string k = "in" + std::to_string (i) + ".";
            v (k + "gainDb", p.in[i].gainDb);
            v (k + "pan", p.in[i].pan);
            v (k + "mute", p.in[i].mute);
            v (k + "solo", p.in[i].solo);
        }
        v (std::string ("master.gainDb"), p.masterGainDb);
        v (std::string ("master.mute"), p.masterMute);
        v (std::string ("hpf.enabled"), p.hpfEnabled);
        v (std::string ("hpf.freqHz"), p.hpfFreqHz);
        v (std::string ("hpf.slope"), p.hpfSlope);
        v (std::string ("eq.enabled"), p.eqEnabled);
        for (int i = 0; i < kNumEqBands; ++i)
        {
            const std::string k = "eq" + std::to_string (i) + ".";
            v (k + "type", p.eq[i].type);
            v (k + "freqHz", p.eq[i].freqHz);
            v (k + "gainDb", p.eq[i].gainDb);
            v (k + "q", p.eq[i].q);
            v (k + "enabled", p.eq[i].enabled);
        }
        v (std::string ("comp.enabled"), p.compEnabled);
        v (std::string ("comp.thresholdDb"), p.compThresholdDb);
        v (std::string ("comp.ratio"), p.compRatio);
        v (std::string ("comp.attackMs"), p.compAttackMs);
        v (std::string ("comp.releaseMs"), p.compReleaseMs);
        v (std::string ("comp.kneeDb"), p.compKneeDb);
        v (std::string ("comp.makeupDb"), p.compMakeupDb);
        v (std::string ("comp.mix"), p.compMix);
        v (std::string ("lim.enabled"), p.limEnabled);
        v (std::string ("lim.thresholdDb"), p.limThresholdDb);
        v (std::string ("lim.ceilingDb"), p.limCeilingDb);
        v (std::string ("lim.releaseMs"), p.limReleaseMs);
        v (std::string ("lim.lookaheadMs"), p.limLookaheadMs);
        v (std::string ("target.lufs"), p.targetLufs);
        v (std::string ("target.maxTruePeakDb"), p.maxTruePeakDb);
        v (std::string ("bypassAll"), p.bypassAll);
    }
}
