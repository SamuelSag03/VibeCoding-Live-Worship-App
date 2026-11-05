#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginRack.h"

/**
 * PatchManager — modelo de jerarquía (punto intermedio entre Gig Performer y MainStage):
 *
 *   CONCERT (setlist)
 *     └── PATCH  = unión de 2+ sonidos (layers/splits) en el mismo teclado
 *           ├── LAYER 1 ("Piano")  -> rack de plugins + zona MIDI (key range/canal/transpose) + bus de salida
 *           ├── LAYER 2 ("Strings") -> idem
 *           └── VARIACIONES (estilo GP): cambian volumen/bypass de layers sin cambiar de patch
 *
 * - Switching de PATCHES sin cortes: sistema A/B con crossfade en el límite de buffer;
 *   el patch saliente entra en "cola" (patch persist) para que reverbs y notas
 *   sostenidas terminen de sonar.
 * - Switching de VARIACIONES: instantáneo, solo aplica gain/bypass a los layers activos.
 * - Cada layer puede mandar su audio a un bus de salida distinto (p.ej. salidas 3-4 de la
 *   interfaz para mandarla a la mesa por otro canal), como los channel strips de MainStage.
 */
class PatchManager
{
public:
    struct Layer
    {
        juce::String name;
        std::unique_ptr<PluginRack> rack;

        // Zona MIDI (estilo Layer Editor de MainStage)
        int keyLow = 0, keyHigh = 127;  // rango de teclas (split); 0-127 = todo el teclado (layer)
        int channelFilter = 0;          // 0 = todos los canales MIDI
        int transpose = 0;

        // Mezcla / ruteo
        float gain = 1.0f;
        float pan  = 0.0f;              // -1..1
        int outputBus = 0;              // par de canales de salida (0 = 1-2, 1 = 3-4, ...)
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

    // --- concert / patches ---
    int addPatch (const juce::String& name);
    bool removePatch (int index);
    int getNumPatches() const { return (int) patches.size(); }
    Patch* getPatch (int index);
    const Patch* getPatch (int index) const;
    juce::StringArray getPatchNames() const;

    // --- layers del patch ---
    int addLayer (int patchIndex, const juce::String& name);
    bool addPluginToLayer (int patchIndex, int layerIndex, juce::AudioPluginInstance* plugin);
    bool removeSlotFromLayer (int patchIndex, int layerIndex, int slotIndex);

    // --- variaciones (switch instantáneo, estilo GP) ---
    int addVariation (int patchIndex, const juce::String& name);
    void selectVariation (int patchIndex, int variationIndex); // -1 = sin variación (estado base)
    int getActiveVariation (int patchIndex) const;

    // --- switching de patches (thread-safe, seamless) ---
    void selectPatch (int index);
    void selectNextPatch();
    void selectPreviousPatch();
    int getActivePatchIndex() const { return activeIndex.load(); }
    std::function<void(int)> onPatchChanged;

    // --- ciclo de vida de audio ---
    void prepareToPlay (double sampleRate, int maxBlockSize);
    void releaseResources();
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    // --- persistencia ---
    juce::XmlElement toXml() const;
    bool fromXml (const juce::XmlElement& concertXml);

private:
    // Runtime de un patch: una instancia por layer, ya preparada
    struct PatchRuntime
    {
        juce::OwnedArray<PluginRack> layerRacks;
        juce::Array<Layer> layerDefs; // copia ligera de zonas/mezcla/ruteo
    };

    void performSwapIfNeeded();
    static void processLayer (const Layer& def, PluginRack& rack,
                              juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn,
                              juce::AudioBuffer<float>& scratch, int numSamples);

    juce::AudioPluginFormatManager* formatManager = nullptr;
    std::vector<Patch> patches;

    std::unique_ptr<PatchRuntime> runtimeA;  // activo
    std::unique_ptr<PatchRuntime> runtimeB;  // preparado para el siguiente
    std::unique_ptr<PatchRuntime> tail;      // patch saliente (patch persist)

    std::atomic<int> activeIndex { -1 };
    std::atomic<int> pendingIndex { -1 };
    std::atomic<bool> swapPending { false };

    int tailBuffersRemaining = 0;
    double lastSampleRate = 48000.0;
    int lastMaxBlockSize = 512;
    juce::AudioBuffer<float> scratch; // buffer scratch pre-reservado (hilo de audio)

    static constexpr int crossfadeSamples = 256;  // ~5.3 ms @ 48 kHz
    static constexpr int tailBuffers = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchManager)
};
