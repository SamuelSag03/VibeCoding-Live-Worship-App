#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * PluginRack — cadena ordenada de slots con un plugin (VST3/LV2) cada uno.
 * El rack es OWNER de las instancias de plugin (unique_ptr).
 *
 * Para el switching sin cortes, el PatchManager prepara el rack destino fuera
 * del hilo de audio y lo activa en el límite de buffer.
 */
struct RackSlot
{
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    bool bypassed = false;
    float gain = 1.0f;
};

class PluginRack
{
public:
    PluginRack() = default;
    ~PluginRack();

    /** Carga slots desde XML usando la lista de descripciones conocidas. */
    static void fromXml (PluginRack& rack, const juce::XmlElement& rackXml,
                         juce::AudioPluginFormatManager& formatManager,
                         const juce::Array<juce::PluginDescription>& knownTypes);

    juce::XmlElement toXml() const;

    void prepare (double sampleRate, int maxBlockSize);
    void releaseResources();

    /** Procesa un buffer. SOLO desde el hilo de audio. Sin allocations. */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    int getNumSlots() const { return slots.size(); }
    RackSlot& getSlot (int i) { return *slots[(size_t) i]; }
    const RackSlot& getSlot (int i) const { return *slots[(size_t) i]; }

    void setSlotBypassed (int i, bool b)
    {
        if (juce::isPositiveAndBelow (i, getNumSlots()))
            slots[(size_t) i]->bypassed = b;
    }

    /** Añade un slot con el plugin indicado (toma ownership). Hilo de mensaje. */
    void addSlot (std::unique_ptr<juce::AudioPluginInstance> plugin)
    {
        auto* slot = new RackSlot();
        slot->plugin = std::move (plugin);
        slots.add (slot);
    }

    bool removeSlot (int i)
    {
        if (! juce::isPositiveAndBelow (i, getNumSlots()))
            return false;
        slots.remove (i);
        return true;
    }

private:
    juce::OwnedArray<RackSlot> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginRack)
};
