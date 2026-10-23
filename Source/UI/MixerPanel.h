#pragma once

#include "Application/AppController.h"
#include "Widgets.h"

namespace amp::ui
{
    // Uma tira de canal: GAIN (fader), PAN, MUTE, SOLO e medidor. index 0/1 = inputs, 2 = master.
    class ChannelStripUi : public juce::Component
    {
    public:
        ChannelStripUi (AppController& c, int index)
            : ctrl (c), idx (index), isMaster (index == 2),
              meterL (BarMeter::Level, true, -60.0f, 6.0f), meterR (BarMeter::Level, true, -60.0f, 6.0f)
        {
            title.setText (isMaster ? juce::String ("MASTER") : "INPUT " + juce::String (idx + 1), juce::dontSendNotification);
            title.setJustificationType (juce::Justification::centred);
            title.setFont (fnt (13.0f, true));
            addAndMakeVisible (title);

            gain.setSliderStyle (juce::Slider::LinearVertical);
            gain.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 18);
            gain.setRange (-60.0, isMaster ? 0.0 : 24.0, 0.1);
            gain.setNumDecimalPlacesToDisplay (1);
            gain.setTextValueSuffix (" dB");
            gain.setDoubleClickReturnValue (true, 0.0);
            gain.onValueChange = [this]
            {
                const float v = static_cast<float> (gain.getValue());
                if (isMaster) ctrl.params.masterGainDb = v; else ctrl.params.in[idx].gainDb = v;
                ctrl.pushParams();
            };
            addAndMakeVisible (gain);

            if (! isMaster)
            {
                pan.setSliderStyle (juce::Slider::LinearHorizontal);
                pan.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
                pan.setRange (-1.0, 1.0, 0.01);
                pan.setDoubleClickReturnValue (true, 0.0);
                pan.setTextBoxIsEditable (false);
                pan.textFromValueFunction = [] (double v)
                {
                    if (std::abs (v) < 0.005) return juce::String ("C");
                    return juce::String (juce::roundToInt (std::abs (v) * 100.0)) + (v < 0 ? " L" : " R");
                };
                pan.onValueChange = [this]
                {
                    ctrl.params.in[idx].pan = static_cast<float> (pan.getValue());
                    ctrl.pushParams();
                };
                addAndMakeVisible (pan);

                configToggle (solo, "SOLO", palette().warn);
                solo.onClick = [this] { ctrl.params.in[idx].solo = solo.getToggleState(); ctrl.pushParams(); };
                addAndMakeVisible (solo);
            }

            configToggle (mute, "MUTE", palette().bad);
            mute.onClick = [this]
            {
                if (isMaster) ctrl.params.masterMute = mute.getToggleState();
                else ctrl.params.in[idx].mute = mute.getToggleState();
                ctrl.pushParams();
            };
            addAndMakeVisible (mute);

            addAndMakeVisible (meterL);
            if (isMaster) addAndMakeVisible (meterR);
            syncFromParams();
        }

        void syncFromParams()
        {
            const auto& p = ctrl.params;
            gain.setValue (isMaster ? p.masterGainDb : p.in[idx].gainDb, juce::dontSendNotification);
            mute.setToggleState (isMaster ? p.masterMute : p.in[idx].mute, juce::dontSendNotification);
            if (! isMaster)
            {
                pan.setValue (p.in[idx].pan, juce::dontSendNotification);
                solo.setToggleState (p.in[idx].solo, juce::dontSendNotification);
            }
        }

        void refresh()
        {
            if (isMaster)
            {
                meterL.setValueDb (linearToDb (ctrl.engine.takeMasterPeak (0)));
                meterR.setValueDb (linearToDb (ctrl.engine.takeMasterPeak (1)));
            }
            else
                meterL.setValueDb (linearToDb (ctrl.engine.takeInputPeak (idx)));
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (palette().panelAlt);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (8);
            title.setBounds (b.removeFromTop (22));
            auto row = b.removeFromTop (28);
            if (isMaster) mute.setBounds (row);
            else { mute.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (1)); solo.setBounds (row.reduced (1)); }
            b.removeFromTop (4);
            if (! isMaster) { pan.setBounds (b.removeFromTop (26)); b.removeFromTop (4); }
            auto meters = b.removeFromRight (isMaster ? 34 : 16);
            if (isMaster) { meterL.setBounds (meters.removeFromLeft (15)); meterR.setBounds (meters.removeFromRight (15)); }
            else meterL.setBounds (meters);
            b.removeFromRight (4);
            gain.setBounds (b);
        }

    private:
        AppController& ctrl;
        int idx;
        bool isMaster;
        juce::Label title;
        juce::Slider gain, pan;
        juce::TextButton mute, solo;
        BarMeter meterL, meterR;
    };

    class MixerPanel : public juce::Component
    {
    public:
        explicit MixerPanel (AppController& c) : s1 (c, 0), s2 (c, 1), sm (c, 2)
        {
            addAndMakeVisible (s1);
            addAndMakeVisible (s2);
            addAndMakeVisible (sm);
        }

        void syncFromParams() { s1.syncFromParams(); s2.syncFromParams(); sm.syncFromParams(); }
        void refresh() { s1.refresh(); s2.refresh(); sm.refresh(); }

        void paint (juce::Graphics& g) override { paintPanel (g, getLocalBounds(), "MIXER"); }

        void resized() override
        {
            auto b = getLocalBounds().reduced (6);
            b.removeFromTop (22);
            const int w = b.getWidth() / 3;
            s1.setBounds (b.removeFromLeft (w));
            s2.setBounds (b.removeFromLeft (w));
            sm.setBounds (b);
        }

    private:
        ChannelStripUi s1, s2, sm;
    };
}
