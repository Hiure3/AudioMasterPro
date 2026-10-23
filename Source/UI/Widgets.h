#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Utilities/DbUtils.h"

namespace amp::ui
{
    // Rotulo + knob rotativo (ou fader vertical) com caixa de valor numerico.
    // Duplo clique = reset; digitar na caixa = valor exato; roda do mouse = ajuste fino.
    class ParamKnob : public juce::Component
    {
    public:
        ParamKnob (const juce::String& name, double mn, double mx, double step, double def,
                   const juce::String& suffix, int decimals, std::function<void (double)> onChange,
                   double skewMid = 0.0)
            : onChange_ (std::move (onChange))
        {
            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (fnt (12.0f));
            label.setColour (juce::Label::textColourId, palette().textDim);
            addAndMakeVisible (label);

            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 18);
            slider.setRange (mn, mx, step);
            if (skewMid > mn && skewMid < mx)
                slider.setSkewFactorFromMidPoint (skewMid);
            slider.setDoubleClickReturnValue (true, def);
            slider.setTextValueSuffix (suffix);
            slider.setNumDecimalPlacesToDisplay (decimals);
            slider.setValue (def, juce::dontSendNotification);
            slider.onValueChange = [this] { if (onChange_) onChange_ (slider.getValue()); };
            addAndMakeVisible (slider);
        }

        void setValue (double v) { slider.setValue (v, juce::dontSendNotification); }
        double getValue() const { return slider.getValue(); }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (16));
            slider.setBounds (b);
        }

        juce::Label label;
        juce::Slider slider;

    private:
        std::function<void (double)> onChange_;
    };

    // Medidor de barra. Level: nivel em dBFS (cresce de baixo/esquerda). Reduction: reducao de ganho em dB (>= 0).
    class BarMeter : public juce::Component
    {
    public:
        enum Mode { Level, Reduction };

        BarMeter (Mode m, bool vertical, float minDb, float maxDb) : mode_ (m), vertical_ (vertical), min_ (minDb), max_ (maxDb)
        {
            cur_ = hold_ = (m == Level ? min_ : 0.0f);
        }

        void setValueDb (float db)
        {
            if (! std::isfinite (db)) db = (mode_ == Level ? min_ : 0.0f);
            if (mode_ == Level)
            {
                cur_ = db > cur_ ? db : std::max (db, cur_ - 1.2f);
                if (db >= hold_) { hold_ = db; holdFrames_ = 45; }
                else if (holdFrames_ > 0) --holdFrames_;
                else hold_ = std::max (db, hold_ - 0.6f);
            }
            else
                cur_ = db > cur_ ? db : std::max (db, cur_ - 1.5f);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& p = palette();
            auto b = getLocalBounds().toFloat();
            g.setColour (p.meterBg);
            g.fillRoundedRectangle (b, 3.0f);

            const float span = max_ - min_;
            const float frac = juce::jlimit (0.0f, 1.0f, mode_ == Level ? (cur_ - min_) / span : cur_ / max_);
            juce::Colour c = p.good;
            if (mode_ == Level) c = cur_ > -3.0f ? p.bad : (cur_ > -12.0f ? p.warn : p.good);
            else c = p.accent2;
            g.setColour (c);

            auto inner = b.reduced (1.5f);
            if (vertical_)
            {
                const float h = inner.getHeight() * frac;
                if (mode_ == Level) g.fillRect (inner.removeFromBottom (h));
                else g.fillRect (inner.removeFromTop (h));
            }
            else
                g.fillRect (inner.removeFromLeft (inner.getWidth() * frac));

            if (mode_ == Level)
            {
                const float hf = juce::jlimit (0.0f, 1.0f, (hold_ - min_) / span);
                g.setColour (p.text);
                auto in2 = b.reduced (1.5f);
                if (vertical_)
                    g.fillRect (in2.getX(), in2.getBottom() - in2.getHeight() * hf - 1.0f, in2.getWidth(), 2.0f);
                else
                    g.fillRect (in2.getX() + in2.getWidth() * hf - 1.0f, in2.getY(), 2.0f, in2.getHeight());
            }
        }

    private:
        Mode mode_;
        bool vertical_;
        float min_, max_, cur_ = 0.0f, hold_ = 0.0f;
        int holdFrames_ = 0;
    };

    inline void configToggle (juce::TextButton& b, const juce::String& text, juce::Colour onColour)
    {
        b.setButtonText (text);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, onColour);
    }
}
