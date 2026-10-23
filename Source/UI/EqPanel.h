#pragma once

#include "Application/AppController.h"
#include "DSP/HighPassFilter.h"
#include "DSP/ParametricEQ.h"
#include "Metering/SpectrumAnalyzer.h"
#include "Widgets.h"

namespace amp::ui
{
    inline bool eqTypeUsesGain (int type) { return type == EqBell || type == EqLowShelf || type == EqHighShelf; }

    // Grafico do EQ: curva (EQ + HPF), pontos arrastaveis, espectro de entrada e pos-EQ.
    //  arrastar = frequencia (horizontal) / ganho (vertical); roda do mouse = Q; duplo clique = reset da banda.
    class EqGraph : public juce::Component
    {
    public:
        explicit EqGraph (AppController& c)
            : ctrl (c), pre (c.engine.preSpectrumFifo()), post (c.engine.postSpectrumFifo())
        {
            setFftSize (4096);
        }

        std::function<void()> onParamsChanged;
        std::function<void (int)> onBandSelected;

        int selectedBand() const noexcept { return selected_; }
        void setSelectedBand (int b) { selected_ = juce::jlimit (0, kNumEqBands - 1, b); repaint(); }

        void setFftSize (int n) { pre.setFftSize (n); post.setFftSize (n); }

        void refresh()
        {
            const double sr = ctrl.engine.sampleRate();
            pre.setSampleRate (sr);
            post.setSampleRate (sr);
            pre.update();
            post.update();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& pal = palette();
            computePlot();
            g.setColour (pal.meterBg);
            g.fillRoundedRectangle (plot_, 4.0f);

            // grade
            g.setFont (fnt (10.0f));
            for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                const float x = freqToX (f);
                g.setColour (pal.grid);
                g.drawVerticalLine (juce::roundToInt (x), plot_.getY(), plot_.getBottom());
                g.setColour (pal.textDim);
                const juce::String t = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (juce::roundToInt (f));
                g.drawText (t, juce::Rectangle<float> (x - 20.0f, plot_.getBottom() + 2.0f, 40.0f, 14.0f), juce::Justification::centred);
            }
            for (int db = -18; db <= 18; db += 6)
            {
                const float y = dbToY (db);
                g.setColour (db == 0 ? pal.textDim.withAlpha (0.6f) : pal.grid);
                g.drawHorizontalLine (juce::roundToInt (y), plot_.getX(), plot_.getRight());
                g.setColour (pal.textDim);
                g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 7.0f, plot_.getX() - 4.0f, 14.0f), juce::Justification::centredRight);
            }

            // espectros
            drawSpectrum (g, pre, pal.textDim.withAlpha (0.25f), true);
            drawSpectrum (g, post, pal.accent.withAlpha (0.9f), false);

            // curva
            const auto& p = ctrl.params;
            const double sr = ctrl.engine.sampleRate() > 1000.0 ? ctrl.engine.sampleRate() : 48000.0;
            juce::Path curve;
            for (float x = plot_.getX(); x <= plot_.getRight(); x += 2.0f)
            {
                const double f = xToFreq (x);
                double db = dsp::ParametricEQ::responseDb (p.eqEnabled, p.eq, sr, f);
                if (p.hpfEnabled) db += dsp::HighPassFilter::magnitudeDb (sr, p.hpfFreqHz, p.hpfSlope, f);
                const float y = juce::jlimit (plot_.getY(), plot_.getBottom(), dbToY (db));
                if (x == plot_.getX()) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
            }
            g.setColour (pal.accent2);
            g.strokePath (curve, juce::PathStrokeType (2.2f));

            // pontos
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const auto pt = nodePos (b);
                const bool en = p.eqEnabled && p.eq[b].enabled;
                g.setColour (en ? pal.accent2 : pal.textDim.withAlpha (0.5f));
                g.fillEllipse (pt.x - kNodeR, pt.y - kNodeR, kNodeR * 2.0f, kNodeR * 2.0f);
                if (b == selected_)
                {
                    g.setColour (pal.text);
                    g.drawEllipse (pt.x - kNodeR - 2.0f, pt.y - kNodeR - 2.0f, kNodeR * 2.0f + 4.0f, kNodeR * 2.0f + 4.0f, 1.8f);
                }
                g.setColour (juce::Colours::black);
                g.setFont (fnt (10.0f, true));
                g.drawText (juce::String (b + 1), juce::Rectangle<float> (pt.x - kNodeR, pt.y - kNodeR, kNodeR * 2.0f, kNodeR * 2.0f), juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            computePlot();
            drag_ = hitBand (e.position);
            if (drag_ >= 0) select (drag_);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (drag_ < 0) return;
            computePlot();
            auto& b = ctrl.params.eq[drag_];
            b.freqHz = static_cast<float> (juce::jlimit (static_cast<double> (ranges::kEqFreqMin), static_cast<double> (ranges::kEqFreqMax), xToFreq (e.position.x)));
            if (eqTypeUsesGain (b.type))
                b.gainDb = static_cast<float> (juce::jlimit (static_cast<double> (ranges::kEqGainMin), static_cast<double> (ranges::kEqGainMax), yToDb (e.position.y)));
            b.enabled = true;
            changed();
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
        {
            computePlot();
            int b = hitBand (e.position);
            if (b < 0) b = selected_;
            auto& band = ctrl.params.eq[b];
            const float factor = wheel.deltaY > 0.0f ? 1.12f : (wheel.deltaY < 0.0f ? 1.0f / 1.12f : 1.0f);
            band.q = juce::jlimit (ranges::kEqQMin, ranges::kEqQMax, band.q * factor);
            select (b);
            changed();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            computePlot();
            const int b = hitBand (e.position);
            if (b < 0) return;
            ctrl.params.eq[b] = defaultEqBand (b);
            select (b);
            changed();
        }

    private:
        static constexpr float kNodeR = 8.0f;

        void computePlot()
        {
            plot_ = getLocalBounds().toFloat();
            plot_.removeFromLeft (30.0f);
            plot_.removeFromBottom (18.0f);
            plot_.reduce (4.0f, 4.0f);
        }

        float freqToX (double f) const { return plot_.getX() + plot_.getWidth() * static_cast<float> (std::log (f / 20.0) / std::log (1000.0)); }
        double xToFreq (float x) const { return 20.0 * std::pow (1000.0, static_cast<double> ((x - plot_.getX()) / plot_.getWidth())); }
        float dbToY (double db) const { return plot_.getY() + plot_.getHeight() * static_cast<float> (0.5 - db / 36.0); }
        double yToDb (float y) const { return (0.5 - static_cast<double> ((y - plot_.getY()) / plot_.getHeight())) * 36.0; }

        juce::Point<float> nodePos (int b) const
        {
            const auto& band = ctrl.params.eq[b];
            return { freqToX (band.freqHz), dbToY (eqTypeUsesGain (band.type) ? band.gainDb : 0.0f) };
        }

        int hitBand (juce::Point<float> p) const
        {
            int best = -1;
            float bestD = 16.0f;
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const float d = nodePos (b).getDistanceFrom (p);
                if (d <= bestD) { bestD = d; best = b; }
            }
            return best;
        }

        void select (int b)
        {
            selected_ = b;
            if (onBandSelected) onBandSelected (b);
        }

        void changed()
        {
            ctrl.pushParams();
            if (onParamsChanged) onParamsChanged();
            repaint();
        }

        void drawSpectrum (juce::Graphics& g, const metering::SpectrumAnalyzer& sa, juce::Colour colour, bool filled)
        {
            const auto& mags = sa.magnitudesDb();
            const int nBins = static_cast<int> (mags.size());
            if (nBins < 8) return;
            const double hzPerBin = sa.sampleRate() / sa.fftSize();
            juce::Path path;
            bool started = false;
            for (float x = plot_.getX(); x <= plot_.getRight(); x += 2.0f)
            {
                const double f0 = xToFreq (x - 1.0f), f1 = xToFreq (x + 1.0f);
                int b0 = juce::jlimit (1, nBins - 1, static_cast<int> (std::floor (f0 / hzPerBin)));
                int b1 = juce::jlimit (b0, nBins - 1, static_cast<int> (std::ceil (f1 / hzPerBin)));
                float m = -120.0f;
                for (int b = b0; b <= b1; ++b) m = std::max (m, mags[static_cast<size_t> (b)]);
                const float y = plot_.getBottom() - plot_.getHeight() * juce::jlimit (0.0f, 1.0f, (m + 90.0f) / 90.0f);
                if (! started) { path.startNewSubPath (x, y); started = true; } else path.lineTo (x, y);
            }
            g.setColour (colour);
            if (filled)
            {
                path.lineTo (plot_.getRight(), plot_.getBottom());
                path.lineTo (plot_.getX(), plot_.getBottom());
                path.closeSubPath();
                g.fillPath (path);
            }
            else
                g.strokePath (path, juce::PathStrokeType (1.4f));
        }

        AppController& ctrl;
        metering::SpectrumAnalyzer pre, post;
        juce::Rectangle<float> plot_;
        int selected_ = 3, drag_ = -1;
    };

    class EqPanel : public juce::Component
    {
    public:
        explicit EqPanel (AppController& c)
            : ctrl (c), graph (c),
              freqKnob ("FREQ", 20.0, 20000.0, 1.0, 1000.0, " Hz", 0, [this] (double v) { band().freqHz = static_cast<float> (v); changed(); }, 1000.0),
              gainKnob ("GAIN", -18.0, 18.0, 0.1, 0.0, " dB", 1, [this] (double v) { band().gainDb = static_cast<float> (v); changed(); }),
              qKnob ("Q", 0.1, 18.0, 0.01, 1.0, "", 2, [this] (double v) { band().q = static_cast<float> (v); changed(); }, 1.0),
              hpfFreq ("HPF FREQ", 20.0, 2000.0, 1.0, 80.0, " Hz", 0, [this] (double v) { ctrl.params.hpfFreqHz = static_cast<float> (v); changed(); }, 200.0)
        {
            addAndMakeVisible (graph);
            graph.onParamsChanged = [this] { syncBandControls(); };
            graph.onBandSelected = [this] (int b) { selectBand (b); };

            for (int b = 0; b < kNumEqBands; ++b)
            {
                auto& btn = bandBtn[b];
                btn.setButtonText (juce::String (b + 1));
                btn.setClickingTogglesState (true);
                btn.setRadioGroupId (4242);
                btn.setColour (juce::TextButton::buttonOnColourId, palette().accent2);
                btn.onClick = [this, b] { selectBand (b); };
                addAndMakeVisible (btn);
            }

            const char* names[EqNumTypes] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
            for (int t = 0; t < EqNumTypes; ++t) typeBox.addItem (names[t], t + 1);
            typeBox.onChange = [this] { band().type = typeBox.getSelectedId() - 1; changed(); };
            addAndMakeVisible (typeBox);

            configToggle (bandOn, "BAND ON", palette().good);
            bandOn.onClick = [this] { band().enabled = bandOn.getToggleState(); changed(); };
            addAndMakeVisible (bandOn);

            configToggle (eqOn, "EQ ON", palette().good);
            eqOn.onClick = [this] { ctrl.params.eqEnabled = eqOn.getToggleState(); changed(); };
            addAndMakeVisible (eqOn);

            configToggle (hpfOn, "HPF ON", palette().good);
            hpfOn.onClick = [this] { ctrl.params.hpfEnabled = hpfOn.getToggleState(); changed(); };
            addAndMakeVisible (hpfOn);

            const char* slopes[5] = { "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
            for (int i = 0; i < 5; ++i) slopeBox.addItem (slopes[i], i + 1);
            slopeBox.onChange = [this] { ctrl.params.hpfSlope = slopeBox.getSelectedId() - 1; changed(); };
            addAndMakeVisible (slopeBox);

            int id = 1;
            for (int n : { 1024, 2048, 4096, 8192, 16384, 32768 }) fftBox.addItem ("FFT " + juce::String (n), id++);
            fftBox.setSelectedId (3, juce::dontSendNotification);
            fftBox.onChange = [this]
            {
                static constexpr int sizes[6] = { 1024, 2048, 4096, 8192, 16384, 32768 };
                graph.setFftSize (sizes[juce::jlimit (0, 5, fftBox.getSelectedId() - 1)]);
            };
            addAndMakeVisible (fftBox);

            addAndMakeVisible (freqKnob);
            addAndMakeVisible (gainKnob);
            addAndMakeVisible (qKnob);
            addAndMakeVisible (hpfFreq);
            syncFromParams();
        }

        void syncFromParams()
        {
            const auto& p = ctrl.params;
            eqOn.setToggleState (p.eqEnabled, juce::dontSendNotification);
            hpfOn.setToggleState (p.hpfEnabled, juce::dontSendNotification);
            hpfFreq.setValue (p.hpfFreqHz);
            slopeBox.setSelectedId (p.hpfSlope + 1, juce::dontSendNotification);
            selectBand (graph.selectedBand());
        }

        void refresh() { graph.refresh(); }

        void paint (juce::Graphics& g) override { paintPanel (g, getLocalBounds(), "PARAMETRIC EQ  +  HPF  +  SPECTRUM ANALYZER"); }

        void resized() override
        {
            auto b = getLocalBounds().reduced (8);
            b.removeFromTop (20);
            auto bottom = b.removeFromBottom (112);
            graph.setBounds (b);

            // linha inferior: seletor de banda | controles da banda | HPF
            auto sel = bottom.removeFromLeft (bottom.getWidth() * 36 / 100);
            auto btnRow = sel.removeFromTop (30);
            for (int i = 0; i < kNumEqBands; ++i)
                bandBtn[i].setBounds (btnRow.removeFromLeft (btnRow.getWidth() / (kNumEqBands - i)).reduced (1));
            sel.removeFromTop (4);
            auto r1 = sel.removeFromTop (28);
            typeBox.setBounds (r1.removeFromLeft (r1.getWidth() / 2).reduced (1));
            bandOn.setBounds (r1.reduced (1));
            sel.removeFromTop (4);
            auto r2 = sel.removeFromTop (28);
            eqOn.setBounds (r2.removeFromLeft (r2.getWidth() / 2).reduced (1));
            fftBox.setBounds (r2.reduced (1));

            const int kw = std::max (60, bottom.getWidth() / 5);
            freqKnob.setBounds (bottom.removeFromLeft (kw));
            gainKnob.setBounds (bottom.removeFromLeft (kw));
            qKnob.setBounds (bottom.removeFromLeft (kw));
            auto hp = bottom.reduced (4, 0);
            hpfOn.setBounds (hp.removeFromTop (28).reduced (1));
            hp.removeFromTop (2);
            slopeBox.setBounds (hp.removeFromTop (26).reduced (1));
            hpfFreq.setBounds (hp);
        }

    private:
        EqBandParams& band() { return ctrl.params.eq[graph.selectedBand()]; }

        void changed() { ctrl.pushParams(); graph.repaint(); }

        void selectBand (int b)
        {
            graph.setSelectedBand (b);
            bandBtn[b].setToggleState (true, juce::dontSendNotification);
            syncBandControls();
        }

        void syncBandControls()
        {
            const auto& bd = ctrl.params.eq[graph.selectedBand()];
            freqKnob.setValue (bd.freqHz);
            gainKnob.setValue (bd.gainDb);
            gainKnob.setEnabled (eqTypeUsesGain (bd.type));
            qKnob.setValue (bd.q);
            typeBox.setSelectedId (bd.type + 1, juce::dontSendNotification);
            bandOn.setToggleState (bd.enabled, juce::dontSendNotification);
        }

        AppController& ctrl;
        EqGraph graph;
        juce::TextButton bandBtn[kNumEqBands];
        juce::ComboBox typeBox, slopeBox, fftBox;
        juce::TextButton bandOn, eqOn, hpfOn;
        ParamKnob freqKnob, gainKnob, qKnob, hpfFreq;
    };
}
