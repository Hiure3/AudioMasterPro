#pragma once

#include "Application/AppController.h"
#include "Presets/Presets.h"
#include "Widgets.h"

namespace amp::ui
{
    class CompressorPanel : public juce::Component
    {
    public:
        explicit CompressorPanel (AppController& c)
            : ctrl (c), grMeter (BarMeter::Reduction, false, 0.0f, 24.0f),
              thr ("THRESHOLD", -60.0, 0.0, 0.1, -18.0, " dB", 1, [this] (double v) { ctrl.params.compThresholdDb = static_cast<float> (v); changed(); }),
              ratio ("RATIO", 1.0, 20.0, 0.1, 3.0, ":1", 1, [this] (double v) { ctrl.params.compRatio = static_cast<float> (v); changed(); }, 4.0),
              attack ("ATTACK", 0.1, 200.0, 0.1, 10.0, " ms", 1, [this] (double v) { ctrl.params.compAttackMs = static_cast<float> (v); changed(); }, 20.0),
              release ("RELEASE", 5.0, 2000.0, 1.0, 120.0, " ms", 0, [this] (double v) { ctrl.params.compReleaseMs = static_cast<float> (v); changed(); }, 200.0),
              knee ("KNEE", 0.0, 24.0, 0.1, 6.0, " dB", 1, [this] (double v) { ctrl.params.compKneeDb = static_cast<float> (v); changed(); }),
              makeup ("MAKEUP", 0.0, 24.0, 0.1, 0.0, " dB", 1, [this] (double v) { ctrl.params.compMakeupDb = static_cast<float> (v); changed(); }),
              mix ("MIX", 0.0, 100.0, 1.0, 100.0, " %", 0, [this] (double v) { ctrl.params.compMix = static_cast<float> (v / 100.0); changed(); })
        {
            configToggle (on, "COMP ON", palette().good);
            on.onClick = [this] { ctrl.params.compEnabled = on.getToggleState(); changed(); };
            addAndMakeVisible (on);

            presetBox.addItem ("Preset...", 100);
            for (int i = 0; i < presets::numCompressorPresets(); ++i)
                presetBox.addItem (presets::compressorPresetName (i), i + 1);
            presetBox.setSelectedId (100, juce::dontSendNotification);
            presetBox.onChange = [this]
            {
                const int id = presetBox.getSelectedId();
                if (id >= 1 && id <= presets::numCompressorPresets())
                {
                    presets::applyCompressorPreset (id - 1, ctrl.params);
                    ctrl.params.compEnabled = true;
                    changed();
                    syncFromParams();
                }
            };
            addAndMakeVisible (presetBox);

            for (auto* k : { &thr, &ratio, &attack, &release, &knee, &makeup, &mix }) addAndMakeVisible (*k);
            addAndMakeVisible (grMeter);
            syncFromParams();
        }

        void syncFromParams()
        {
            const auto& p = ctrl.params;
            on.setToggleState (p.compEnabled, juce::dontSendNotification);
            thr.setValue (p.compThresholdDb);   ratio.setValue (p.compRatio);
            attack.setValue (p.compAttackMs);   release.setValue (p.compReleaseMs);
            knee.setValue (p.compKneeDb);       makeup.setValue (p.compMakeupDb);
            mix.setValue (p.compMix * 100.0f);
        }

        void refresh() { grMeter.setValueDb (ctrl.engine.takeCompressorGrDb()); }

        void paint (juce::Graphics& g) override
        {
            paintPanel (g, getLocalBounds(), "COMPRESSOR");
            g.setColour (palette().textDim);
            g.setFont (fnt (11.0f));
            g.drawText ("GAIN REDUCTION", grBounds.withHeight (14).translated (0, -15), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (8);
            b.removeFromTop (20);
            auto top = b.removeFromTop (28);
            on.setBounds (top.removeFromLeft (100).reduced (1));
            top.removeFromLeft (6);
            presetBox.setBounds (top.removeFromLeft (150).reduced (1));
            b.removeFromTop (20);
            grBounds = b.removeFromBottom (18).reduced (4, 2);
            grMeter.setBounds (grBounds);
            b.removeFromBottom (18);
            const int w = b.getWidth() / 7;
            for (auto* k : { &thr, &ratio, &attack, &release, &knee, &makeup })
                k->setBounds (b.removeFromLeft (w));
            mix.setBounds (b);
        }

    private:
        void changed() { ctrl.pushParams(); }

        AppController& ctrl;
        BarMeter grMeter;
        juce::TextButton on;
        juce::ComboBox presetBox;
        ParamKnob thr, ratio, attack, release, knee, makeup, mix;
        juce::Rectangle<int> grBounds;
    };

    class LimiterPanel : public juce::Component
    {
    public:
        explicit LimiterPanel (AppController& c)
            : ctrl (c), grMeter (BarMeter::Reduction, false, 0.0f, 12.0f),
              thr ("THRESHOLD", -24.0, 0.0, 0.1, -1.0, " dB", 1, [this] (double v) { ctrl.params.limThresholdDb = static_cast<float> (v); changed(); }),
              ceiling ("CEILING", -12.0, 0.0, 0.1, -1.0, " dB", 1, [this] (double v) { ctrl.params.limCeilingDb = static_cast<float> (v); changed(); }),
              rel ("RELEASE", 10.0, 1000.0, 1.0, 100.0, " ms", 0, [this] (double v) { ctrl.params.limReleaseMs = static_cast<float> (v); changed(); }, 100.0),
              look ("LOOKAHEAD", 0.5, 10.0, 0.1, 2.0, " ms", 1, [this] (double v) { ctrl.params.limLookaheadMs = static_cast<float> (v); changed(); })
        {
            configToggle (on, "LIMITER ON", palette().good);
            on.onClick = [this] { ctrl.params.limEnabled = on.getToggleState(); changed(); };
            addAndMakeVisible (on);
            for (auto* k : { &thr, &ceiling, &rel, &look }) addAndMakeVisible (*k);
            addAndMakeVisible (grMeter);
            syncFromParams();
        }

        void syncFromParams()
        {
            const auto& p = ctrl.params;
            on.setToggleState (p.limEnabled, juce::dontSendNotification);
            thr.setValue (p.limThresholdDb);  ceiling.setValue (p.limCeilingDb);
            rel.setValue (p.limReleaseMs);    look.setValue (p.limLookaheadMs);
        }

        void refresh() { grMeter.setValueDb (ctrl.engine.takeLimiterGrDb()); }

        void paint (juce::Graphics& g) override
        {
            paintPanel (g, getLocalBounds(), "LIMITER");
            g.setColour (palette().textDim);
            g.setFont (fnt (11.0f));
            g.drawText ("GAIN REDUCTION", grBounds.withHeight (14).translated (0, -15), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (8);
            b.removeFromTop (20);
            on.setBounds (b.removeFromTop (28).removeFromLeft (120).reduced (1));
            b.removeFromTop (20);
            grBounds = b.removeFromBottom (18).reduced (4, 2);
            grMeter.setBounds (grBounds);
            b.removeFromBottom (18);
            const int w = b.getWidth() / 4;
            thr.setBounds (b.removeFromLeft (w));
            ceiling.setBounds (b.removeFromLeft (w));
            rel.setBounds (b.removeFromLeft (w));
            look.setBounds (b);
        }

    private:
        void changed() { ctrl.pushParams(); }

        AppController& ctrl;
        BarMeter grMeter;
        juce::TextButton on;
        ParamKnob thr, ceiling, rel, look;
        juce::Rectangle<int> grBounds;
    };
}
