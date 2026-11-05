#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * PluginRack
 * ----------
 * Un rack = lista ordenada de slots con un AudioPluginInstance (VST3/LV2) cada uno,
 * más estado por slot (bypass, volumen). El audio entra por el slot 0 y sale del último.
 *
 * Para el switching sin cortes, el PatchManager mantiene DOS racks (A/B): el inactivo se
 * prepara con prepareToPlay() fuera del hilo de audio y se "enciende" en el límite de buffer.
 */
struct RackSlot
{
    juce::AudioPluginInstance* plugin = nullptr; // no owned (format manager cache)
    bool bypassed = false;
    float gain = 1.0f;
};

class PluginRack
{
public:
    PluginRack() = default;
    ~PluginRack();

    /** Carga slots desde XML (lista de plugins por UID + estado). Rellena el rack dado. */
    static void fromXml (PluginRack& rack, const juce::XmlElement& rackXml,
                         juce::AudioPluginFormatManager& formatManager);

    juce::XmlElement toXml() const;

    void prepare (double sampleRate, int maxBlockSize);
    void releaseResources();

    /** Procesa un buffer. Llamar SOLO desde el hilo de audio. Sin allocations. */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    int getNumSlots() const { return static_cast<int> (slots.size()); }
    RackSlot& getSlot (int i) { return slots[(size_t) i]; }
    const juce::Array<RackSlot>& getSlots() const { return slots; }

    void setSlotBypassed (int i, bool b) { if (isPositiveAndBelow (i, getNumSlots())) slots[(size_t) i].bypassed = b; }

    /** Añade un slot con el plugin indicado (no owner). Llamar desde hilo de mensaje. */
    void addSlot (juce::AudioPluginInstance* plugin)
    {
        auto* slot = new RackSlot();
        slot->plugin = plugin;
        slots.add (slot);
    }

    bool removeSlot (int i)
    {
        if (! isPositiveAndBelow (i, getNumSlots()))
            return false;
        slots.remove (i);
        return true;
    }

private:
    juce::OwnedArray<RackSlot> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginRack)
};
