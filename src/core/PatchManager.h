#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginRack.h"

/**
 * PatchManager — jerarquía CONCERT → PATCH → LAYERS (+ variaciones estilo GP).
 *
 *   CONCERT (setlist)
 *     └── PATCH  = unión de 2+ sonidos (layers/splits) en el mismo teclado
 *           ├── LAYER "Piano"   → rack VST3/LV2 + zona MIDI + bus de salida
 *           ├── LAYER "Strings" → idem
 *           └── VARIACIONES: cambian gain/bypass de layers sin cambiar de patch
 *
 * Switching de patches sin cortes: A/B con crossfade en el límite de buffer y
 * cola del patch saliente (patch persist).
 */
class PatchManager
{
public:
    struct Layer
    {
        juce::String name;
        std::unique_ptr<PluginRack> rack;
        int keyLow = 0, keyHigh = 127;  // rango de teclas (split); 0-127 = todo (layer)
        int channelFilter = 0;          // 0 = todos los canales
        int transpose = 0;
        float gain = 1.0f;
        float pan  = 0.0f;              // -1..1
        int outputBus = 0;              // par de canales de salida
    };

    struct LayerDef // copia copiable para el runtime
    {
        int keyLow = 0, keyHigh = 127;
        int channelFilter = 0;
        int transpose = 0;
        float gain = 1.0f;
        float pan = 0.0f;
        int outputBus = 0;
    };

    struct Variation
    {
        juce::String name;
        struct Override { int layerIndex; float gain; bool bypass; };
        juce::Array<Override> overrides;
    };

    struct Patch
    {
        juce::String name;
        juce::OwnedArray<Layer> layers;
        juce::Array<Variation> variations;
        int activeVariation = -1;
    };

    PatchManager();

    void setFormatManager (juce::AudioPluginFormatManager* fm) { formatManager = fm; }
    void setKnownTypes (const juce::Array<juce::PluginDescription>& kt) { knownTypes = kt; }

    // --- concert / patches ---
    int addPatch (const juce::String& name);
    bool removePatch (int index);
    int getNumPatches() const { return (int) patches.size(); }
    Patch* getPatch (int index);
    const Patch* getPatch (int index) const;
    juce::StringArray getPatchNames() const;

    // --- layers ---
    int addLayer (int patchIndex, const juce::String& name);
    bool addPluginToLayer (int patchIndex, int layerIndex, std::unique_ptr<juce::AudioPluginInstance> plugin);
    bool removeSlotFromLayer (int patchIndex, int layerIndex, int slotIndex);

    // --- bypass de layer activo (control remoto) ---
    void setActiveLayerBypassed (int layerIndex, bool bypassed);

    // --- variaciones ---
    int addVariation (int patchIndex, const juce::String& name);
    void selectVariation (int patchIndex, int variationIndex);
    int getActiveVariation (int patchIndex) const;

    // --- switching de patches (thread-safe, seamless) ---
    void selectPatch (int index);
    void selectNextPatch();
    void selectPreviousPatch();
    int getActivePatchIndex() const { return activeIndex.load(); }
    std::function<void(int)> onPatchChanged;

    // --- audio ---
    void prepareToPlay (double sampleRate, int maxBlockSize);
    void releaseResources();
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    // --- persistencia ---
    juce::XmlElement toXml() const;
    bool fromXml (const juce::XmlElement& concertXml);

private:
    struct PatchRuntime
    {
        juce::Array<PluginRack*> layerRacks; // no-owner: los racks viven en el patch
        juce::Array<LayerDef> layerDefs;
    };

    static std::unique_ptr<PatchRuntime> makeRuntime (const Patch& p);
    void performSwapIfNeeded();
    static void processLayer (const LayerDef& def, PluginRack& rack,
                              juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn,
                              juce::AudioBuffer<float>& scratchBuf, int numSamples);

    juce::AudioPluginFormatManager* formatManager = nullptr;
    juce::Array<juce::PluginDescription> knownTypes;
    std::vector<Patch> patches;

    std::unique_ptr<PatchRuntime> runtimeA;
    std::unique_ptr<PatchRuntime> runtimeB;
    std::unique_ptr<PatchRuntime> tail;

    std::atomic<int> activeIndex { -1 };
    std::atomic<int> pendingIndex { -1 };
    std::atomic<bool> swapPending { false };

    int tailBuffersRemaining = 0;
    double lastSampleRate = 48000.0;
    int lastMaxBlockSize = 512;
    juce::AudioBuffer<float> scratch;

    static constexpr int crossfadeSamples = 256;  // ~5.3 ms @ 48 kHz
    static constexpr int tailBuffers = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchManager)
};
