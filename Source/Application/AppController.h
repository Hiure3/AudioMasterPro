#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>

#include "Audio/AudioEngine.h"
#include "Audio/Parameters.h"

namespace amp
{
    // Liga o motor de audio (AudioEngine) ao dispositivo de audio do JUCE e guarda o estado
    // de parametros da UI (incluindo os snapshots A/B). Vive na thread de UI.
    class AppController : private juce::AudioIODeviceCallback
    {
    public:
        AppController()
        {
            engine.publishParams (params);
            stored_[0] = stored_[1] = params;

            std::unique_ptr<juce::XmlElement> saved = juce::parseXML (deviceStateFile());
            initError_ = deviceManager.initialise (2, 2, saved.get(), true);
            deviceManager.addAudioCallback (this);
        }

        ~AppController() override
        {
            deviceManager.removeAudioCallback (this);
            saveDeviceState();
            deviceManager.closeAudioDevice();
            engine.release();
        }

        AudioEngine engine;
        juce::AudioDeviceManager deviceManager;
        ParamSnapshot params;        // copia de trabalho (somente thread de UI)

        void pushParams()
        {
            sanitize (params);
            engine.publishParams (params);
        }

        // ---- A/B (bypass e alvo de loudness sao globais e nao trocam) ----
        int abSlot() const noexcept { return slot_; }

        void selectSlot (int s)
        {
            if (s == slot_ || s < 0 || s > 1) return;
            stored_[slot_] = params;
            ParamSnapshot next = stored_[s];
            next.bypassAll = params.bypassAll;
            next.targetLufs = params.targetLufs;
            next.maxTruePeakDb = params.maxTruePeakDb;
            slot_ = s;
            params = next;
            pushParams();
        }

        void copyActiveToOther()
        {
            stored_[1 - slot_] = params;
        }

        // Carrega um preset: so o "som" (HPF/EQ/comp/limiter/alvo). Niveis de entrada, pan, mute/solo,
        // master e bypass permanecem como estao.
        void applyPreset (const ParamSnapshot& preset)
        {
            ParamSnapshot p = preset;
            for (int i = 0; i < kNumInputs; ++i) p.in[i] = params.in[i];
            p.masterGainDb = params.masterGainDb;
            p.masterMute = params.masterMute;
            p.bypassAll = params.bypassAll;
            params = p;
            pushParams();
        }

        // ---- Informacoes do dispositivo ----
        juce::AudioIODevice* device() { return deviceManager.getCurrentAudioDevice(); }
        int xruns() { auto* d = device(); return d != nullptr ? d->getXRunCount() : -1; }
        const juce::String& initError() const noexcept { return initError_; }

        void saveDeviceState()
        {
            if (auto xml = deviceManager.createStateXml())
            {
                const auto f = deviceStateFile();
                f.getParentDirectory().createDirectory();
                f.replaceWithText (xml->toString());
            }
        }

    private:
        static juce::File deviceStateFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("AudioMasterPro").getChildFile ("audio-device.xml");
        }

        void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                               int numSamples, const juce::AudioIODeviceCallbackContext&) override
        {
            engine.process (in, numIn, out, numOut, numSamples);
        }

        void audioDeviceAboutToStart (juce::AudioIODevice* d) override
        {
            engine.prepare (d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples());
        }

        void audioDeviceStopped() override
        {
            engine.release();
        }

        ParamSnapshot stored_[2];
        int slot_ = 0;
        juce::String initError_;
    };
}
