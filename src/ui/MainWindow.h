#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/AudioEngine.h"
#include "../core/PatchManager.h"
#include "../core/MidiRouter.h"
#include "../core/MidiClockSync.h"
#include "../core/RemoteControl.h"

class MainWindow final : public juce::DocumentWindow,
                         private juce::Timer
{
public:
    MainWindow();

    void closeButtonPressed() override;
    void resized() override;

private:
    void timerCallback() override; // medidor de CPU/latencia/sync en vivo
    void refreshPatchList();
    void refreshLayerList();
    void addPluginToSelectedLayer();
    void removeSelectedSlot();
    void saveConcert();
    void loadConcert();

    AudioEngine audioEngine;
    juce::AudioPluginFormatManager formatManager;
    juce::Array<juce::PluginDescription> knownTypes;
    PatchManager patchManager;
    MidiRouter midiRouter;
    MidiClockSync clockSync;
    RemoteControl remoteControl;

    // UI
    juce::ListBox patchList { "patches", nullptr };
    juce::ListBox layerList { "layers", nullptr };
    juce::TextButton addLayerButton    { "Añadir layer" };
    juce::TextButton addPluginButton   { "Añadir plugin" };
    juce::TextButton removeSlotButton  { "Quitar slot" };
    juce::TextButton saveButton        { "Guardar concert" };
    juce::TextButton loadButton        { "Cargar concert" };
    juce::Label statusLabel;

    struct PatchListModel final : public juce::ListBoxModel
    {
        MainWindow& owner;
        explicit PatchListModel (MainWindow& o) : owner (o) {}
        int getNumRows() override { return owner.patchManager.getNumPatches(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            g.fillAll (selected ? juce::Colours::darkblue : juce::Colours::black);
            g.setColour (juce::Colours::white);
            auto names = owner.patchManager.getPatchNames();
            if (juce::isPositiveAndBelow (row, names.size()))
                g.drawText (juce::String (row + 1) + ". " + names[row], 8, 0, w - 8, h,
                            juce::Justification::centredLeft);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            owner.patchManager.selectPatch (row);
        }
    } listModel { *this };

    struct LayerListModel final : public juce::ListBoxModel
    {
        MainWindow& owner;
        explicit LayerListModel (MainWindow& o) : owner (o) {}
        int getNumRows() override
        {
            if (auto* p = owner.patchManager.getPatch (owner.patchManager.getActivePatchIndex()))
                return p->layers.size();
            return 0;
        }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            g.fillAll (selected ? juce::Colours::darkblue : juce::Colours::black);
            g.setColour (juce::Colours::white);
            if (auto* p = owner.patchManager.getPatch (owner.patchManager.getActivePatchIndex()))
                if (juce::isPositiveAndBelow (row, p->layers.size()))
                {
                    auto* layer = p->layers[row];
                    auto name = layer->name + "  [" + juce::String (layer->keyLow) + "-" + juce::String (layer->keyHigh) + "]";
                    int nPlugins = owner.patchManager.getLayerSlotCount (owner.patchManager.getActivePatchIndex(), row);
                    name += "  (" + juce::String (nPlugins) + " plugins)";
                    g.drawText (juce::String (row + 1) + ". " + name, 8, 0, w - 8, h,
                                juce::Justification::centredLeft);
                }
        }
    } layerModel { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};
