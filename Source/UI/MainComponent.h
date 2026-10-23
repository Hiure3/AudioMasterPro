#pragma once

#include <memory>

#include "Application/AppController.h"
#include "AudioSettingsPanel.h"
#include "DynamicsPanel.h"
#include "EqPanel.h"
#include "LoudnessPanel.h"
#include "MixerPanel.h"
#include "Theme.h"
#include "Widgets.h"

namespace amp
{
    // Janela principal: barra superior (bypass, A/B, monitor, presets), mixer, EQ, dinamica,
    // loudness e barra de status (CPU, memoria, latencia, dropouts).
    class MainComponent : public juce::Component, private juce::Timer
    {
    public:
        MainComponent();
        ~MainComponent() override;

        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        void timerCallback() override;
        void syncAll();
        void openAudioSettings();
        void savePreset();
        void loadPreset();
        void applyTheme();
        void updateStats();

        ui::AmpLookAndFeel laf;
        AppController controller;

        ui::MixerPanel mixer;
        ui::EqPanel eqPanel;
        ui::CompressorPanel compPanel;
        ui::LimiterPanel limPanel;
        ui::LoudnessPanel loudPanel;

        juce::TextButton bypassBtn, abA, abB, copyBtn, monitorBtn, settingsBtn, themeBtn, saveBtn, loadBtn;
        juce::ComboBox presetBox;
        std::unique_ptr<juce::FileChooser> chooser;

        juce::String statsText;
        bool statsWarning = false;
        int statsTick = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
