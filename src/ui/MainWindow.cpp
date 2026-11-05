#include "MainWindow.h"
#include "../core/Persistence.h"

// Escanea todos los plugins VST3/LV2 detectables en el sistema
static juce::Array<juce::PluginDescription> scanAllPlugins (juce::AudioPluginFormatManager& fm)
{
    juce::KnownPluginList list;
    for (auto* fmt : fm.getFormats())
    {
        juce::PluginDirectoryScanner scanner (list, *fmt, fmt->getDefaultLocationsToSearch(),
                                               true, juce::File());
        juce::String name;
        while (scanner.scanNextFile (true, name)) {}
    }
    return list.getTypes();
}

MainWindow::MainWindow()
    : juce::DocumentWindow ("LiveKeys — Live Worship Performance",
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour (juce::ResizableWindow::backgroundColourId),
                            juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setResizable (true, false);

    // --- Motor de plugins (VST3 + LV2) ---
    formatManager.addDefaultFormats();
    patchManager.setFormatManager (&formatManager);
    knownTypes = scanAllPlugins (formatManager);
    patchManager.setKnownTypes (knownTypes);

    audioEngine.setPatchManager (&patchManager);
    audioEngine.setMidiRouter (&midiRouter);

    // --- Arranque de audio: JACK primero ---
    auto err = audioEngine.initialise (2, 2);

    // --- MIDI + sync + remoto ---
    midiRouter.setClockSync (&clockSync);
    midiRouter.setRemoteControl (&remoteControl);
    midiRouter.refreshDevices();
    remoteControl.start (&patchManager, 8000);

    // Al cambiar de patch (UI, OSC o MIDI) se refresca la UI
    patchManager.onPatchChanged = [this] (int)
    {
        juce::MessageManager::callAsync ([this] { refreshPatchList(); refreshLayerList(); });
    };

    // --- UI ---
    auto* content = new juce::Component();

    patchList.setModel (&listModel);
    content->addAndMakeVisible (patchList);

    layerList.setModel (&layerModel);
    content->addAndMakeVisible (layerList);

    addLayerButton.onClick = [this]
    {
        const int patchIndex = patchList.getSelectedRow() >= 0 ? patchList.getSelectedRow()
                                                               : patchManager.getActivePatchIndex();
        if (auto* p = patchManager.getPatch (patchIndex))
        {
            patchManager.addLayer (patchIndex, "Layer " + juce::String (p->layers.size() + 1));
            refreshLayerList();
        }
    };
    content->addAndMakeVisible (addLayerButton);

    addPluginButton.onClick = [this] { addPluginToSelectedLayer(); };
    content->addAndMakeVisible (addPluginButton);

    removeSlotButton.onClick = [this] { removeSelectedSlot(); };
    content->addAndMakeVisible (removeSlotButton);

    saveButton.onClick = [this] { saveConcert(); };
    content->addAndMakeVisible (saveButton);

    loadButton.onClick = [this] { loadConcert(); };
    content->addAndMakeVisible (loadButton);

    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setMinimumHorizontalScale (1.0f);
    statusLabel.setText ("Audio: " + (err.isEmpty() ? audioEngine.getCurrentDeviceName()
                                                  : "ERROR: " + err),
                         juce::dontSendNotification);
    content->addAndMakeVisible (statusLabel);

    startTimerHz (2); // refresco del medidor cada 500 ms

    // Patches de ejemplo para probar el switching
    patchManager.addPatch ("01 - Piano YC88");
    patchManager.addPatch ("02 - Strings");
    patchManager.addPatch ("03 - Pad Atmosphere");
    patchList.selectRow (0);
    patchManager.selectPatch (0);

    setContentOwned (content, true);
    centreWithSize (420, 560);
    resized(); // asegura el layout inicial
    setVisible (true);
}

void MainWindow::resized()
{
    auto* c = getContentComponent();
    if (c == nullptr) return;
    auto area = c->getLocalBounds().reduced (8);

    auto status = area.removeFromBottom (64);
    statusLabel.setBounds (status);

    auto buttons = area.removeFromBottom (30);
    const int bw = (buttons.getWidth() - 16) / 3;
    addLayerButton.setBounds   (buttons.removeFromLeft (bw).reduced (2));
    addPluginButton.setBounds  (buttons.removeFromLeft (bw).reduced (2));
    removeSlotButton.setBounds (buttons.removeFromLeft (bw).reduced (2));

    auto io = area.removeFromBottom (30);
    const int iw = (io.getWidth() - 8) / 2;
    saveButton.setBounds (io.removeFromLeft (iw).reduced (2));
    loadButton.setBounds (io.removeFromLeft (iw).reduced (2));

    auto layers = area.removeFromBottom (juce::jmax (80, area.getHeight() / 3));
    layerList.setBounds (layers);

    patchList.setBounds (area);
}

void MainWindow::refreshPatchList() { patchList.updateContent(); }
void MainWindow::refreshLayerList() { layerList.updateContent(); }

void MainWindow::timerCallback()
{
    const auto dev  = audioEngine.getCurrentDeviceName();
    const auto sr   = audioEngine.getCurrentSampleRate();
    const auto buf  = audioEngine.getCurrentBufferSize();
    const auto lat  = audioEngine.getMeasuredLatencyMs();
    const auto cpu  = audioEngine.getCpuUsage() * 100.0f;
    const auto sync = clockSync.getStatusText();

    statusLabel.setText ("Audio: " + dev + "  " + juce::String ((int) sr) + " Hz, bufer " + juce::String (buf)
                         + "\nLatencia: " + juce::String (lat, 2) + " ms   CPU: " + juce::String (cpu, 1) + " %"
                         + "\n" + sync,
                         juce::dontSendNotification);
}

void MainWindow::addPluginToSelectedLayer()
{
    const int patchIndex = patchList.getSelectedRow() >= 0 ? patchList.getSelectedRow()
                                                           : patchManager.getActivePatchIndex();
    const int layerIndex = layerList.getSelectedRow();
    if (patchIndex < 0 || layerIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Añadir plugin",
                                                "Selecciona primero un patch y un layer.");
        return;
    }

    auto types = knownTypes;
    if (types.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Añadir plugin",
                                                "No hay plugins VST3/LV2 detectados. "
                                                "Instala plugins en ~/.vst3 o ~/.lv2 (p.ej. sfizz para soundfonts).");
        return;
    }

    juce::StringArray names;
    for (auto& d : types)
        names.add (d.name + "  [" + d.pluginFormatName + "]");

    auto* picker = new juce::AlertWindow ("Elige un plugin", "", juce::AlertWindow::NoIcon);
    auto* list = new juce::ListBox ("plugins", nullptr);
    struct PickerModel final : public juce::ListBoxModel
    {
        juce::StringArray& names;
        explicit PickerModel (juce::StringArray& n) : names (n) {}
        int getNumRows() override { return names.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
        {
            g.fillAll (sel ? juce::Colours::darkblue : juce::Colours::black);
            g.setColour (juce::Colours::white);
            g.drawText (names[row], 8, 0, w - 8, h, juce::Justification::centredLeft);
        }
    };
    auto* model = new PickerModel (names);
    list->setModel (model);
    list->setBounds (0, 0, 380, 300);
    picker->addAndMakeVisible (list);
    picker->addButton ("Añadir", 1);
    picker->addButton ("Cancelar", 0);
    picker->setSize (400, 340);

    picker->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, picker, list, model, types, patchIndex, layerIndex] (int result) mutable
    {
        const int row = list->getSelectedRow();
        if (result == 1 && juce::isPositiveAndBelow (row, types.size()))
        {
            juce::String err;
            const double sr  = audioEngine.getCurrentSampleRate() > 0.0 ? audioEngine.getCurrentSampleRate() : 48000.0;
            const int    buf = audioEngine.getCurrentBufferSize() > 0 ? audioEngine.getCurrentBufferSize() : 512;
            if (auto instance = formatManager.createPluginInstance (types[(size_t) row], sr, buf, err))
            {
                patchManager.addPluginToLayer (patchIndex, layerIndex, std::move (instance));
                refreshLayerList();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                        "Error", "No se pudo crear el plugin: " + err);
            }
        }
        delete picker;
        delete model;
    }), true);
}

void MainWindow::removeSelectedSlot()
{
    const int patchIndex = patchList.getSelectedRow() >= 0 ? patchList.getSelectedRow()
                                                           : patchManager.getActivePatchIndex();
    const int layer = layerList.getSelectedRow();
    if (patchIndex < 0 || layer < 0)
        return;
    if (auto* p = patchManager.getPatch (patchIndex))
        if (auto* l = p->layers[(size_t) layer])
            patchManager.removeSlotFromLayer (patchIndex, layer, l->rack->getNumSlots() - 1);
    refreshLayerList();
}

void MainWindow::saveConcert()
{
    juce::FileChooser chooser ("Guardar concert como…",
                               juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                               "*.livekeys.xml");
    chooser.launchAsync (juce::FileBrowserComponent::saveMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this] (const juce::FileChooser& c)
    {
        if (c.getResult().existsAsFile() || c.getResult().getFileName().isNotEmpty())
            Persistence::saveConcert (patchManager, remoteControl, c.getResult());
    });
}

void MainWindow::loadConcert()
{
    juce::FileChooser chooser ("Cargar concert…",
                               juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                               "*.livekeys.xml");
    chooser.launchAsync (juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles,
                         [this] (const juce::FileChooser& c)
    {
        if (c.getResult().existsAsFile()
            && Persistence::loadConcert (patchManager, remoteControl, c.getResult()))
        {
            refreshPatchList();
            refreshLayerList();
        }
    });
}

void MainWindow::closeButtonPressed()
{
    remoteControl.stop();
    juce::JUCEApplicationBase::quit();
}
