#include "MainComponent.h"

#include "Presets/Presets.h"
#include "Utilities/SystemInfo.h"

namespace amp
{
    MainComponent::MainComponent()
        : mixer (controller), eqPanel (controller), compPanel (controller), limPanel (controller), loudPanel (controller)
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);
        setLookAndFeel (&laf);

        addAndMakeVisible (mixer);
        addAndMakeVisible (eqPanel);
        addAndMakeVisible (compPanel);
        addAndMakeVisible (limPanel);
        addAndMakeVisible (loudPanel);

        ui::configToggle (bypassBtn, "BYPASS ALL", ui::palette().bad);
        bypassBtn.onClick = [this] { controller.params.bypassAll = bypassBtn.getToggleState(); controller.pushParams(); };
        addAndMakeVisible (bypassBtn);

        ui::configToggle (abA, "A", ui::palette().accent);
        ui::configToggle (abB, "B", ui::palette().accent);
        abA.setRadioGroupId (777);
        abB.setRadioGroupId (777);
        abA.setToggleState (true, juce::dontSendNotification);
        abA.onClick = [this] { controller.selectSlot (0); syncAll(); };
        abB.onClick = [this] { controller.selectSlot (1); syncAll(); };
        addAndMakeVisible (abA);
        addAndMakeVisible (abB);

        copyBtn.setButtonText ("COPY A/B");
        copyBtn.onClick = [this] { controller.copyActiveToOther(); };
        addAndMakeVisible (copyBtn);

        ui::configToggle (monitorBtn, "MONITOR", ui::palette().accent2);
        monitorBtn.setToggleState (false, juce::dontSendNotification);
        monitorBtn.onClick = [this] { controller.engine.setMonitorEnabled (monitorBtn.getToggleState()); repaint(); };
        addAndMakeVisible (monitorBtn);

        presetBox.addItem ("Preset...", 100);
        for (int i = 0; i < presets::numFactoryPresets(); ++i)
            presetBox.addItem (presets::factoryPresetName (i), i + 1);
        presetBox.setSelectedId (100, juce::dontSendNotification);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id >= 1 && id <= presets::numFactoryPresets())
            {
                controller.applyPreset (presets::makeFactoryPreset (id - 1));
                syncAll();
            }
        };
        addAndMakeVisible (presetBox);

        saveBtn.setButtonText ("SAVE");
        saveBtn.onClick = [this] { savePreset(); };
        addAndMakeVisible (saveBtn);
        loadBtn.setButtonText ("LOAD");
        loadBtn.onClick = [this] { loadPreset(); };
        addAndMakeVisible (loadBtn);

        settingsBtn.setButtonText ("AUDIO SETTINGS");
        settingsBtn.onClick = [this] { openAudioSettings(); };
        addAndMakeVisible (settingsBtn);

        themeBtn.setButtonText ("LIGHT");
        themeBtn.onClick = [this]
        {
            ui::isLightTheme() = ! ui::isLightTheme();
            themeBtn.setButtonText (ui::isLightTheme() ? "DARK" : "LIGHT");
            applyTheme();
        };
        addAndMakeVisible (themeBtn);

        syncAll();
        updateStats();
        setSize (1600, 900);
        startTimerHz (30);
    }

    MainComponent::~MainComponent()
    {
        stopTimer();
        setLookAndFeel (nullptr);
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void MainComponent::applyTheme()
    {
        laf.applyPalette();
        for (auto* b : { &abA, &abB }) b->setColour (juce::TextButton::buttonOnColourId, ui::palette().accent);
        bypassBtn.setColour (juce::TextButton::buttonOnColourId, ui::palette().bad);
        monitorBtn.setColour (juce::TextButton::buttonOnColourId, ui::palette().accent2);
        sendLookAndFeelChange();
        repaint();
    }

    void MainComponent::syncAll()
    {
        bypassBtn.setToggleState (controller.params.bypassAll, juce::dontSendNotification);
        mixer.syncFromParams();
        eqPanel.syncFromParams();
        compPanel.syncFromParams();
        limPanel.syncFromParams();
        loudPanel.syncFromParams();
    }

    void MainComponent::openAudioSettings()
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new ui::AudioSettingsPanel (controller));
        o.dialogTitle = "Audio Settings";
        o.dialogBackgroundColour = ui::palette().bg;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void MainComponent::savePreset()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("Audio Master Pro").getChildFile ("Presets");
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Salvar preset", dir, "*.ampreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  f = f.withFileExtension ("ampreset");
                                  f.replaceWithText (juce::String::fromUTF8 (presets::serialize (controller.params).c_str()));
                              });
    }

    void MainComponent::loadPreset()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("Audio Master Pro").getChildFile ("Presets");
        chooser = std::make_unique<juce::FileChooser> ("Carregar preset", dir, "*.ampreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  ParamSnapshot p;
                                  if (presets::parse (f.loadFileAsString().toStdString(), p))
                                  {
                                      controller.applyPreset (p);
                                      syncAll();
                                  }
                                  else
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "Preset invalido",
                                                                              "Este arquivo nao e um preset valido do Audio Master Pro.");
                              });
    }

    void MainComponent::updateStats()
    {
        auto& d = controller;
        juce::String s;
        const double cpu = d.deviceManager.getCpuUsage() * 100.0;
        const double audioCpu = d.engine.audioCpuPercent();
        const double memMb = static_cast<double> (processMemoryBytes()) / (1024.0 * 1024.0);
        s << "CPU " << juce::String (cpu, 0) << "%   |   Audio CPU " << juce::String (audioCpu, 1) << "%   |   Mem "
          << juce::String (memMb, 0) << " MB";
        if (auto* dev = d.device())
        {
            const double sr = dev->getCurrentSampleRate();
            const int buf = dev->getCurrentBufferSizeSamples();
            s << "   |   " << juce::String (sr, 0) << " Hz   |   Buffer " << buf << "   |   Lat in/out "
              << juce::String (1000.0 * dev->getInputLatencyInSamples() / sr, 1) << "/"
              << juce::String (1000.0 * dev->getOutputLatencyInSamples() / sr, 1) << " ms";
        }
        else
            s << "   |   SEM DISPOSITIVO DE AUDIO";
        const int xr = d.xruns();
        s << "   |   Dropouts " << static_cast<int> (d.engine.dropoutCount()) << "   XRuns " << (xr < 0 ? juce::String ("n/d") : juce::String (xr))
          << "   Erros " << static_cast<int> (d.engine.errorCount());
        statsWarning = cpu > 80.0 || audioCpu > 70.0;
        if (statsWarning) s << "   |   AVISO: CPU ALTA";
        statsText = s;
    }

    void MainComponent::timerCallback()
    {
        mixer.refresh();
        eqPanel.refresh();
        compPanel.refresh();
        limPanel.refresh();
        loudPanel.refresh();
        if (++statsTick % 10 == 0)
        {
            updateStats();
            repaint (getLocalBounds().removeFromBottom (30));
        }
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        const auto& pal = ui::palette();
        g.fillAll (pal.bg);

        g.setColour (pal.text);
        g.setFont (ui::fnt (20.0f, true));
        g.drawText ("AUDIO MASTER PRO", 16, 8, 260, 30, juce::Justification::centredLeft);
        g.setColour (pal.textDim);
        g.setFont (ui::fnt (10.0f));
        g.drawText ("Professional Audio. Precision Loudness. One Platform.", 16, 32, 300, 12, juce::Justification::centredLeft);

        auto bar = getLocalBounds().removeFromBottom (28);
        g.setColour (pal.panel);
        g.fillRect (bar);
        g.setColour (statsWarning ? pal.warn : pal.textDim);
        g.setFont (ui::fnt (12.0f));
        g.drawText (statsText, bar.reduced (12, 0), juce::Justification::centredLeft);

        if (! controller.engine.isMonitorEnabled())
        {
            g.setColour (pal.accent2);
            g.setFont (ui::fnt (11.0f, true));
            g.drawText ("MONITOR OFF - clique em MONITOR para ouvir (use fones para evitar microfonia)",
                        getWidth() - 560, 30, 540, 14, juce::Justification::centredRight);
        }
    }

    void MainComponent::resized()
    {
        auto b = getLocalBounds();
        auto top = b.removeFromTop (48).reduced (330, 8);
        b.removeFromBottom (28);

        // barra superior (comeca depois do titulo)
        bypassBtn.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (10);
        abA.setBounds (top.removeFromLeft (36));
        abB.setBounds (top.removeFromLeft (36));
        copyBtn.setBounds (top.removeFromLeft (84).reduced (2, 0));
        top.removeFromLeft (10);
        monitorBtn.setBounds (top.removeFromLeft (96));
        top.removeFromLeft (10);
        presetBox.setBounds (top.removeFromLeft (150));
        saveBtn.setBounds (top.removeFromLeft (56).reduced (2, 0));
        loadBtn.setBounds (top.removeFromLeft (56).reduced (2, 0));
        themeBtn.setBounds (top.removeFromRight (64));
        top.removeFromRight (8);
        settingsBtn.setBounds (top.removeFromRight (130));

        b = b.reduced (8, 4);
        mixer.setBounds (b.removeFromLeft (310));
        b.removeFromLeft (8);
        loudPanel.setBounds (b.removeFromRight (400));
        b.removeFromRight (8);

        auto bottom = b.removeFromBottom (b.getHeight() * 40 / 100);
        eqPanel.setBounds (b);
        bottom.removeFromTop (8);
        compPanel.setBounds (bottom.removeFromLeft (bottom.getWidth() * 62 / 100));
        bottom.removeFromLeft (8);
        limPanel.setBounds (bottom);
    }
}
