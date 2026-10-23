#pragma once

#include <vector>

#include "Application/AppController.h"
#include "Widgets.h"

namespace amp::ui
{
    // Painel Master Meter: LUFS-I / S / M, LRA, True Peak, Loudness Target, status e historico.
    class LoudnessPanel : public juce::Component
    {
    public:
        explicit LoudnessPanel (AppController& c)
            : ctrl (c),
              targetKnob ("TARGET", -40.0, -6.0, 0.1, -14.0, " LUFS", 1, [this] (double v)
                          { ctrl.params.targetLufs = static_cast<float> (v); ctrl.pushParams(); })
        {
            addAndMakeVisible (targetKnob);

            for (int i = 0; i < 6; ++i)
                maxTpBox.addItem (juce::String (kTpValues[i], 1) + " dBTP", i + 1);
            maxTpBox.onChange = [this]
            {
                ctrl.params.maxTruePeakDb = kTpValues[juce::jlimit (0, 5, maxTpBox.getSelectedId() - 1)];
                ctrl.pushParams();
            };
            addAndMakeVisible (maxTpBox);

            refBox.addItem ("Ref: Short-term (3 s)", 1);
            refBox.addItem ("Ref: Integrated", 2);
            refBox.addItem ("Ref: Momentary", 3);
            refBox.setSelectedId (1, juce::dontSendNotification);
            addAndMakeVisible (refBox);

            resetBtn.setButtonText ("RESET METER");
            resetBtn.onClick = [this] { ctrl.engine.resetLoudness(); histS_.clear(); histM_.clear(); };
            addAndMakeVisible (resetBtn);
            syncFromParams();
        }

        void syncFromParams()
        {
            targetKnob.setValue (ctrl.params.targetLufs);
            int best = 3;
            float bd = 1000.0f;
            for (int i = 0; i < 6; ++i)
                if (std::abs (kTpValues[i] - ctrl.params.maxTruePeakDb) < bd) { bd = std::abs (kTpValues[i] - ctrl.params.maxTruePeakDb); best = i; }
            maxTpBox.setSelectedId (best + 1, juce::dontSendNotification);
        }

        void refresh()
        {
            res_ = ctrl.engine.loudness();
            if (++frame_ % 15 == 0)
            {
                histS_.push_back (static_cast<float> (res_.shortTerm));
                histM_.push_back (static_cast<float> (res_.momentary));
                if (histS_.size() > kHistCap) { histS_.erase (histS_.begin()); histM_.erase (histM_.begin()); }
            }
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& pal = palette();
            paintPanel (g, getLocalBounds(), "LOUDNESS  -  ITU-R BS.1770 / EBU R128");

            const double tp = res_.truePeakDb;
            const float maxTp = ctrl.params.maxTruePeakDb;
            const juce::Colour tpColour = tp >= -0.05 ? pal.bad : (tp > maxTp ? pal.warn : pal.text);

            card (g, cardI_, "LUFS-I  (INTEGRATED)", lufs (res_.integrated), "LUFS", 48.0f, pal.text);
            card (g, cardS_, "LUFS-S  (SHORT-TERM)", lufs (res_.shortTerm), "LUFS", 30.0f, pal.text);
            card (g, cardM_, "LUFS-M  (MOMENTARY)", lufs (res_.momentary), "LUFS", 30.0f, pal.text);
            card (g, cardLra_, "LRA", res_.integrated <= -119.5 ? juce::String ("---") : juce::String (res_.lra, 1), "LU", 26.0f, pal.text);
            card (g, cardTp_, "TRUE PEAK", lufs (tp), "dBTP", 26.0f, tpColour);
            card (g, cardTpMax_, "TRUE PEAK MAX", lufs (res_.truePeakMaxDb), "dBTP", 26.0f,
                  res_.truePeakMaxDb >= -0.05 ? pal.bad : (res_.truePeakMaxDb > maxTp ? pal.warn : pal.text));

            // ---- LOUDNESS TARGET ----
            g.setColour (pal.panelAlt);
            g.fillRoundedRectangle (targetRect_.toFloat(), 6.0f);
            const double cur = currentValue();
            const double diff = cur - ctrl.params.targetLufs;
            auto t = targetRect_.reduced (8, 6);
            g.setColour (pal.textDim);
            g.setFont (fnt (11.0f, true));
            g.drawText ("LOUDNESS TARGET", t.removeFromTop (14), juce::Justification::centredLeft);
            auto cols = t.removeFromTop (46);
            const int cw = cols.getWidth() / 3;
            auto drawCol = [&] (juce::Rectangle<int> r, const juce::String& lab, const juce::String& val, juce::Colour col)
            {
                g.setColour (pal.textDim);
                g.setFont (fnt (10.0f));
                g.drawText (lab, r.removeFromTop (13), juce::Justification::centred);
                g.setColour (col);
                g.setFont (fnt (22.0f, true));
                g.drawText (val, r, juce::Justification::centred);
            };
            drawCol (cols.removeFromLeft (cw), "CURRENT", lufs (cur), pal.text);
            drawCol (cols.removeFromLeft (cw), "TARGET", juce::String (ctrl.params.targetLufs, 1), pal.accent);
            drawCol (cols, "DIFFERENCE", cur <= -119.5 ? juce::String ("---") : ((diff >= 0 ? "+" : "") + juce::String (diff, 1) + " LU"),
                     std::abs (diff) <= 1.0 ? pal.good : pal.text);

            juce::String statusText;
            juce::Colour statusColour;
            status (statusText, statusColour);
            auto badge = t.removeFromBottom (28).reduced (40, 2);
            g.setColour (statusColour);
            g.fillRoundedRectangle (badge.toFloat(), 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.setFont (fnt (15.0f, true));
            g.drawText (statusText, badge, juce::Justification::centred);

            // ---- historico ----
            g.setColour (pal.meterBg);
            g.fillRoundedRectangle (histRect_.toFloat(), 4.0f);
            auto h = histRect_.toFloat().reduced (4.0f, 4.0f);
            g.setFont (fnt (9.0f));
            for (int db = -40; db <= -10; db += 10)
            {
                const float y = h.getBottom() - h.getHeight() * (static_cast<float> (db) + 45.0f) / 40.0f;
                g.setColour (pal.grid);
                g.drawHorizontalLine (juce::roundToInt (y), h.getX(), h.getRight());
                g.setColour (pal.textDim);
                g.drawText (juce::String (db), juce::Rectangle<float> (h.getX() + 2.0f, y - 11.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
            }
            const float ty = h.getBottom() - h.getHeight() * (ctrl.params.targetLufs + 45.0f) / 40.0f;
            g.setColour (pal.accent.withAlpha (0.8f));
            g.drawHorizontalLine (juce::roundToInt (ty), h.getX(), h.getRight());
            drawHistory (g, h, histM_, pal.textDim);
            drawHistory (g, h, histS_, pal.accent2);
            g.setColour (pal.textDim);
            g.setFont (fnt (10.0f));
            auto legend = histRect_;
            g.drawText ("HISTORY (10 min):  S = orange  |  M = grey  |  target = blue", legend.removeFromBottom (14).reduced (6, 0), juce::Justification::centredRight);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (10);
            b.removeFromTop (20);
            cardI_ = b.removeFromTop (110);
            b.removeFromTop (8);
            auto row = b.removeFromTop (66);
            cardS_ = row.removeFromLeft (row.getWidth() / 2 - 4);
            row.removeFromLeft (8);
            cardM_ = row;
            b.removeFromTop (8);
            row = b.removeFromTop (66);
            const int w3 = (row.getWidth() - 16) / 3;
            cardLra_ = row.removeFromLeft (w3);   row.removeFromLeft (8);
            cardTp_ = row.removeFromLeft (w3);    row.removeFromLeft (8);
            cardTpMax_ = row;
            b.removeFromTop (8);
            targetRect_ = b.removeFromTop (118);
            b.removeFromTop (8);
            auto ctl = b.removeFromBottom (90);
            targetKnob.setBounds (ctl.removeFromLeft (110));
            ctl.removeFromLeft (6);
            maxTpBox.setBounds (ctl.removeFromTop (28).reduced (1));
            refBox.setBounds (ctl.removeFromTop (28).reduced (1));
            resetBtn.setBounds (ctl.removeFromTop (28).reduced (1));
            b.removeFromBottom (6);
            histRect_ = b;
        }

    private:
        static constexpr float kTpValues[6] = { 0.0f, -0.1f, -0.5f, -1.0f, -1.5f, -2.0f };
        static constexpr size_t kHistCap = 1200;

        static juce::String lufs (double v) { return v <= -119.5 ? juce::String ("---") : juce::String (v, 1); }

        double currentValue() const
        {
            switch (refBox.getSelectedId())
            {
                case 2:  return res_.integrated;
                case 3:  return res_.momentary;
                default: return res_.shortTerm;
            }
        }

        void status (juce::String& text, juce::Colour& colour) const
        {
            const auto& pal = palette();
            const double cur = currentValue();
            const double diff = cur - ctrl.params.targetLufs;
            if (res_.truePeakDb >= -0.05 || res_.truePeakMaxDb >= 0.0 + 1e-9) { text = "CLIPPING"; colour = pal.bad; return; }
            if (cur <= -69.0) { text = "LOW"; colour = pal.accent; return; }
            if (diff > 3.0) { text = "TOO LOUD"; colour = pal.bad; return; }
            if (res_.truePeakDb > ctrl.params.maxTruePeakDb || diff > 1.0) { text = "WARNING"; colour = pal.warn; return; }
            if (diff < -1.0) { text = "LOW"; colour = pal.accent; return; }
            text = "GOOD";
            colour = pal.good;
        }

        static void card (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, const juce::String& value,
                          const juce::String& unit, float size, juce::Colour colour)
        {
            const auto& pal = palette();
            g.setColour (pal.panelAlt);
            g.fillRoundedRectangle (r.toFloat(), 6.0f);
            auto in = r.reduced (8, 5);
            g.setColour (pal.textDim);
            g.setFont (fnt (10.0f, true));
            g.drawText (label, in.removeFromTop (13), juce::Justification::centredLeft);
            g.setColour (colour);
            g.setFont (fnt (size, true));
            g.drawText (value, in, juce::Justification::centred);
            g.setColour (pal.textDim);
            g.setFont (fnt (10.0f));
            g.drawText (unit, r.reduced (8, 5), juce::Justification::bottomRight);
        }

        static void drawHistory (juce::Graphics& g, juce::Rectangle<float> h, const std::vector<float>& v, juce::Colour c)
        {
            if (v.size() < 2) return;
            const float step = h.getWidth() / static_cast<float> (kHistCap - 1);
            juce::Path p;
            bool started = false;
            for (size_t i = 0; i < v.size(); ++i)
            {
                const float x = h.getRight() - static_cast<float> (v.size() - 1 - i) * step;
                const float val = juce::jlimit (-45.0f, -5.0f, v[i] <= -119.5f ? -45.0f : v[i]);
                const float y = h.getBottom() - h.getHeight() * (val + 45.0f) / 40.0f;
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }

        AppController& ctrl;
        metering::LoudnessResults res_;
        std::vector<float> histS_, histM_;
        int frame_ = 0;
        juce::Rectangle<int> cardI_, cardS_, cardM_, cardLra_, cardTp_, cardTpMax_, targetRect_, histRect_;
        ParamKnob targetKnob;
        juce::ComboBox maxTpBox, refBox;
        juce::TextButton resetBtn;
    };
}
