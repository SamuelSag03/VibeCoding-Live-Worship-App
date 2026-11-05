#include "MainWindow.h"
#include "../core/Persistence.h"

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
    audioEngine.setPatchManager (&patchManager);
    audioEngine.setMidiRouter (&midiRouter);

    // --- Arranque de audio: JACK primero (48 kHz, 256 muestras ≈ 5.3 ms) ---
    auto err = audioEngine.initialise (2, 2, 48000, 256);

    // --- MIDI + sync + remoto ---
    midiRouter.setClockSync (&clockSync);
    midiRouter.setRemoteControl (&remoteControl);
    midiRouter.refreshDevices();
    remoteControl.start (&patchManager, 8000);

    // Al cambiar de patch (UI, OSC o MIDI) se aplica su config MIDI al router
    patchManager.onPatchChanged = [this] (int)
    {
        // El filtrado MIDI ahora es por layer (dentro de PatchManager)
        juce::MessageManager::callAsync ([this] { refreshPatchList(); refreshLayerList(); });
    };

    // --- UI ---
    auto* content = new juce::Component();

    patchList.setModel (&listModel);
    patchList.setBounds (10, 10, 280, 200);
    content->addAndMakeVisible (patchList);

    layerList.setModel (&layerModel);
    layerList.setBounds (10, 220, 280, 120);
    content->addAndMakeVisible (layerList);

    addLayerButton.setBounds (10, 350, 90, 24);
    addLayerButton.onClick = [this]
    {
        const int patchIndex = patchList.getSelectedRow() >= 0 ? patchList.getSelectedRow()
                                                               : patchManager.getActivePatchIndex();
        if (patchIndex >= 0)
        {
            patchManager.addLayer (patchIndex, "Layer " + juce::String (patchManager.getPatch (patchIndex)->layers.size() + 1));
            refreshLayerList();
        }
    };
    content->addAndMakeVisible (addLayerButton);

    addPluginButton.setBounds (105, 350, 90, 24);
    addPluginButton.onClick = [this] { addPluginToSelectedLayer(); };
    content->addAndMakeVisible (addPluginButton);

    removeSlotButton.setBounds (200, 350, 90, 24);
    removeSlotButton.onClick = [this] { removeSelectedSlot(); };
    content->addAndMakeVisible (removeSlotButton);

    saveButton.setBounds (10, 380, 130, 24);
    saveButton.onClick = [this] { saveConcert(); };
    content->addAndMakeVisible (saveButton);

    loadButton.setBounds (150, 380, 140, 24);
    loadButton.onClick = [this] { loadConcert(); };
    content->addAndMakeVisible (loadButton);

    statusLabel.setBounds (10, 412, 280, 40);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText ("Audio: " + (err.isEmpty() ? audioEngine.getCurrentDeviceName()
                                                  : "ERROR: " + err),
                         juce::dontSendNotification);
    content->addAndMakeVisible (statusLabel);

    startTimerHz (2); // refresco del medidor cada 500 ms

    // Patches de ejemplo para probar el switching
    patchManager.addPatch ("01 - Piano YC88");
    patchManager.addPatch ("02 - Strings");
    patchManager.addPatch ("03 - Pad Atmosphere");

    setContentOwned (content, true);
    centreWithSize (320, 470);
    setVisible (true);
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
    const int patchIndex = patchList.getSelectedRow();
    if (patchIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Añadir plugin",
                                                "Selecciona primero un patch de la lista.");
        return;
    }

    // Lista todos los plugins VST3/LV2 detectados en el sistema
    juce::StringArray names;
    juce::Array<const juce::PluginDescription*> descs;
    for (auto* fmt : formatManager.getFormats())
        for (auto& d : fmt->getPluginDescriptions())
        {
            names.add (d.name + "  [" + fmt->getName() + "]");
            descs.add (&d);
        }

    if (names.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Añadir plugin",
                                                "No hay plugins VST3/LV2 detectados. "
                                                "Instala p.ej. sf2/sfz soundfont players o plugins LV2 en ~/.lv2, ~/.vst3.");
        return;
    }

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

    picker->enterModalState (true, juce::ModalCallbackFunction::create ([this, picker, list, model, descs, patchIndex] (int result)
    {
        const int row = list->getSelectedRow();
        if (result == 1 && isPositiveAndBelow (row, descs.size()))
        {
            auto* fmt = formatManager.getFormats().getFirst(); // se busca el format correcto abajo
            for (auto* f : formatManager.getFormats())
                for (auto& d : f->getPluginDescriptions())
                    if (&d == descs[(size_t) row]) { fmt = f; break; }

            juce::String err;
            if (auto* instance = fmt->createInstance (*descs[(size_t) row],
                                                       audioEngine.getCurrentSampleRate() > 0.0 ? audioEngine.getCurrentSampleRate() : 48000.0,
                                                       audioEngine.getCurrentBufferSize() > 0 ? audioEngine.getCurrentBufferSize() : 512,
                                                       err))
            {
                const int layerIndex = layerList.getSelectedRow();
                if (layerIndex >= 0)
                    patchManager.addPluginToLayer (patchIndex, layerIndex, instance);
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
    // Quita el último slot del layer seleccionado
    if (auto* p = patchManager.getPatch (patchIndex))
        if (auto* l = p->layers[layer])
            patchManager.removeSlotFromLayer (patchIndex, layer, l->rack->getNumSlots() - 1);
    refreshLayerList();
}

void MainWindow::saveConcert()
{
    juce::FileChooser chooser ("Guardar concert como…", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.livekeys.xml");
    if (chooser.browseForFileToSave (true))
        Persistence::saveConcert (patchManager, remoteControl, chooser.getResult());
}

void MainWindow::loadConcert()
{
    juce::FileChooser chooser ("Cargar concert…", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.livekeys.xml");
    if (chooser.browseForFileToOpen())
    {
        if (Persistence::loadConcert (patchManager, chooser.getResult()))
        {
            refreshPatchList();
            refreshLayerList();
        }
    }
}

void MainWindow::closeButtonPressed()
{
    remoteControl.stop();
    juce::JUCEApplicationBase::quit();
}
