#include "Presets.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace amp::presets
{
    namespace
    {
        struct CompSet { float thr, ratio, att, rel, knee, makeup, mix; };

        constexpr const char* kFactoryNames[] = { "Default", "Vocal", "Speech", "Bass", "Kick",
                                                  "Snare", "Drums", "Master", "Live", "Church Live" };
        constexpr const char* kCompNames[] = { "Vocal", "Bass", "Drums", "Speech", "Master" };
        constexpr CompSet kCompSets[] = {
            { -18.f, 3.0f,  8.f, 120.f,  6.f, 3.0f, 1.f },   // Vocal
            { -20.f, 4.0f, 30.f, 150.f,  6.f, 3.0f, 1.f },   // Bass
            { -14.f, 3.0f, 15.f, 100.f,  6.f, 2.0f, 1.f },   // Drums
            { -20.f, 4.0f,  5.f, 100.f,  4.f, 4.0f, 1.f },   // Speech
            { -14.f, 1.8f, 30.f, 200.f, 12.f, 1.5f, 1.f },   // Master
        };

        void setBand (ParamSnapshot& p, int i, int type, float f, float g, float q)
        {
            p.eq[i].type = type; p.eq[i].freqHz = f; p.eq[i].gainDb = g; p.eq[i].q = q; p.eq[i].enabled = true;
        }
        void setHpf (ParamSnapshot& p, float f, int slope) { p.hpfEnabled = true; p.hpfFreqHz = f; p.hpfSlope = slope; }
        void setComp (ParamSnapshot& p, int idx) { applyCompressorPreset (idx, p); p.compEnabled = true; }
        void setLim (ParamSnapshot& p, float thr, float ceil, float rel, float look)
        {
            p.limEnabled = true; p.limThresholdDb = thr; p.limCeilingDb = ceil; p.limReleaseMs = rel; p.limLookaheadMs = look;
        }
    }

    int numFactoryPresets() noexcept { return static_cast<int> (sizeof (kFactoryNames) / sizeof (kFactoryNames[0])); }
    const char* factoryPresetName (int i) noexcept { return (i >= 0 && i < numFactoryPresets()) ? kFactoryNames[i] : ""; }
    int numCompressorPresets() noexcept { return static_cast<int> (sizeof (kCompNames) / sizeof (kCompNames[0])); }
    const char* compressorPresetName (int i) noexcept { return (i >= 0 && i < numCompressorPresets()) ? kCompNames[i] : ""; }

    void applyCompressorPreset (int i, ParamSnapshot& p) noexcept
    {
        if (i < 0 || i >= numCompressorPresets()) return;
        const auto& c = kCompSets[i];
        p.compThresholdDb = c.thr; p.compRatio = c.ratio; p.compAttackMs = c.att; p.compReleaseMs = c.rel;
        p.compKneeDb = c.knee; p.compMakeupDb = c.makeup; p.compMix = c.mix;
    }

    ParamSnapshot makeFactoryPreset (int index)
    {
        ParamSnapshot p;   // Default: HPF off, EQ plano, compressor off, limiter -1 dB
        switch (index)
        {
            case 1:  // Vocal
                setHpf (p, 90.f, 1);
                setBand (p, 1, EqBell, 250.f, -2.5f, 1.2f);
                setBand (p, 3, EqBell, 3200.f, 2.0f, 1.0f);
                setBand (p, 6, EqHighShelf, 10000.f, 2.0f, 0.7f);
                setComp (p, 0);  setLim (p, -2.f, -1.f, 100.f, 2.f);  p.targetLufs = -16.f;
                break;
            case 2:  // Speech
                setHpf (p, 100.f, 2);
                setBand (p, 1, EqBell, 200.f, -2.0f, 1.0f);
                setBand (p, 4, EqBell, 2500.f, 2.5f, 1.0f);
                setBand (p, 5, EqBell, 5000.f, 1.5f, 1.0f);
                setComp (p, 3);  setLim (p, -3.f, -1.f, 100.f, 2.f);  p.targetLufs = -16.f;
                break;
            case 3:  // Bass
                setHpf (p, 30.f, 3);
                setBand (p, 0, EqLowShelf, 80.f, 2.0f, 0.7f);
                setBand (p, 2, EqBell, 250.f, -2.0f, 1.5f);
                setBand (p, 4, EqBell, 900.f, 1.5f, 1.0f);
                setComp (p, 1);  setLim (p, -2.f, -1.f, 120.f, 2.f);
                break;
            case 4:  // Kick
                setHpf (p, 30.f, 2);
                setBand (p, 0, EqBell, 60.f, 3.0f, 1.2f);
                setBand (p, 2, EqBell, 350.f, -4.0f, 1.5f);
                setBand (p, 5, EqBell, 4000.f, 3.0f, 1.0f);
                applyCompressorPreset (2, p); p.compEnabled = true; p.compRatio = 4.f; p.compAttackMs = 20.f; p.compReleaseMs = 80.f; p.compKneeDb = 2.f;
                setLim (p, -2.f, -1.f, 80.f, 1.5f);
                break;
            case 5:  // Snare
                setHpf (p, 80.f, 2);
                setBand (p, 1, EqBell, 200.f, 2.0f, 1.0f);
                setBand (p, 2, EqBell, 500.f, -3.0f, 2.0f);
                setBand (p, 5, EqBell, 5000.f, 3.0f, 0.9f);
                applyCompressorPreset (2, p); p.compEnabled = true; p.compRatio = 4.f; p.compAttackMs = 5.f; p.compReleaseMs = 80.f; p.compKneeDb = 3.f;
                setLim (p, -2.f, -1.f, 80.f, 1.5f);
                break;
            case 6:  // Drums
                setHpf (p, 40.f, 2);
                setBand (p, 2, EqBell, 400.f, -2.5f, 1.2f);
                setBand (p, 6, EqHighShelf, 9000.f, 2.0f, 0.7f);
                setComp (p, 2);  setLim (p, -3.f, -1.f, 100.f, 2.f);
                break;
            case 7:  // Master
                setHpf (p, 25.f, 2);
                setBand (p, 0, EqLowShelf, 60.f, 1.0f, 0.7f);
                setBand (p, 6, EqHighShelf, 12000.f, 1.0f, 0.7f);
                setComp (p, 4);  setLim (p, -4.f, -1.f, 150.f, 4.f);
                p.targetLufs = -14.f;  p.maxTruePeakDb = -1.f;
                break;
            case 8:  // Live
                setHpf (p, 80.f, 2);
                setBand (p, 1, EqBell, 300.f, -2.0f, 1.0f);
                setComp (p, 0);  p.compThresholdDb = -18.f; p.compReleaseMs = 150.f;
                setLim (p, -2.f, -1.f, 80.f, 1.5f);
                p.targetLufs = -18.f;
                break;
            case 9:  // Church Live
                setHpf (p, 100.f, 3);
                setBand (p, 1, EqBell, 250.f, -3.0f, 1.0f);
                setBand (p, 3, EqBell, 2800.f, 2.0f, 1.0f);
                setBand (p, 6, EqHighShelf, 8000.f, 1.0f, 0.7f);
                setComp (p, 3);  p.compThresholdDb = -22.f; p.compRatio = 3.5f; p.compAttackMs = 8.f; p.compReleaseMs = 200.f; p.compKneeDb = 8.f;
                setLim (p, -3.f, -1.f, 120.f, 3.f);
                p.targetLufs = -16.f;
                break;
            default: break;
        }
        sanitize (p);
        return p;
    }

    namespace
    {
        struct Writer
        {
            std::ostringstream os;
            void operator() (const std::string& k, float& v) { char b[64]; std::snprintf (b, sizeof b, "%.6g", static_cast<double> (v)); os << k << '=' << b << '\n'; }
            void operator() (const std::string& k, int& v)   { os << k << '=' << v << '\n'; }
            void operator() (const std::string& k, bool& v)  { os << k << '=' << (v ? 1 : 0) << '\n'; }
        };

        struct Reader
        {
            const std::string* key = nullptr;
            const std::string* val = nullptr;
            void operator() (const std::string& k, float& v) { if (k == *key) v = static_cast<float> (std::strtod (val->c_str(), nullptr)); }
            void operator() (const std::string& k, int& v)   { if (k == *key) v = static_cast<int> (std::strtol (val->c_str(), nullptr, 10)); }
            void operator() (const std::string& k, bool& v)  { if (k == *key) v = std::strtol (val->c_str(), nullptr, 10) != 0; }
        };
    }

    std::string serialize (const ParamSnapshot& p)
    {
        ParamSnapshot copy = p;
        sanitize (copy);
        Writer w;
        w.os << "AMP_PRESET 1\n";
        visitParams (copy, w);
        return w.os.str();
    }

    bool parse (const std::string& text, ParamSnapshot& out)
    {
        std::istringstream is (text);
        std::string line;
        if (! std::getline (is, line) || line.rfind ("AMP_PRESET", 0) != 0)
            return false;

        ParamSnapshot p;
        while (std::getline (is, line))
        {
            const auto eq = line.find ('=');
            if (eq == std::string::npos || eq == 0 || line.size() > 200) continue;
            const std::string key = line.substr (0, eq), val = line.substr (eq + 1);
            Reader r;  r.key = &key;  r.val = &val;
            visitParams (p, r);
        }
        sanitize (p);
        out = p;
        return true;
    }
}
