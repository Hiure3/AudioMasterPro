#pragma once

#include "Application/AppController.h"
#include "Widgets.h"

namespace amp::ui
{
    // Tela AUDIO SETTINGS: driver (WASAPI / DirectSound / ASIO quando compilado com o SDK),
    // dispositivos de entrada e saida, canais, sample rate e buffer (seletor do JUCE) + latencias.
    class AudioSettingsPanel : public juce::Component, private juce::Timer
    {
    public:
        explicit AudioSettingsPanel (AppController& c)
            : ctrl (c), selector (c.deviceManager, 0, 2, 0, 2, false, false, false, false)
        {
            addAndMakeVisible (selector);
            info.setJustificationType (juce::Justification::topLeft);
            info.setFont (fnt (13.0f));
            addAndMakeVisible (info);
            setSize (620, 620);
            updateInfo();
            startTimerHz (2);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (10);
            info.setBounds (b.removeFromBottom (150));
            selector.setBounds (b);
        }

        void paint (juce::Graphics& g) override { g.fillAll (palette().bg); }

    private:
        void timerCallback() override { updateInfo(); }

        void updateInfo()
        {
            juce::String s;
            if (auto* d = ctrl.deviceManager.getCurrentAudioDevice())
            {
                const double sr = d->getCurrentSampleRate();
                const int buf = d->getCurrentBufferSizeSamples();
                const int inLat = d->getInputLatencyInSamples(), outLat = d->getOutputLatencyInSamples();
                auto ms = [sr] (int samples) { return juce::String (1000.0 * samples / sr, 1) + " ms"; };
                s << "Driver:  " << ctrl.deviceManager.getCurrentAudioDeviceType() << "\n"
                  << "Dispositivo:  " << d->getName() << "\n"
                  << "Sample Rate:  " << juce::String (sr, 0) << " Hz\n"
                  << "Buffer Size:  " << buf << " samples (" << ms (buf) << ")\n"
                  << "Input Latency:  " << inLat << " samples (" << ms (inLat) << ")\n"
                  << "Output Latency:  " << outLat << " samples (" << ms (outLat) << ")\n"
                  << "Canais ativos:  " << d->getActiveInputChannels().countNumberOfSetBits() << " in / "
                  << d->getActiveOutputChannels().countNumberOfSetBits() << " out\n"
                  << "Latencia extra do limiter (lookahead):  " << ctrl.engine.latencySamples() << " samples";
            }
            else
                s << "Nenhum dispositivo de audio aberto.\n" << ctrl.initError();
            info.setText (s, juce::dontSendNotification);
        }

        AppController& ctrl;
        juce::AudioDeviceSelectorComponent selector;
        juce::Label info;
    };
}
