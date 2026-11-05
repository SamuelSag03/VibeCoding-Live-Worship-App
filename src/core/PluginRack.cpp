#include "PluginRack.h"

PluginRack::~PluginRack()
{
    releaseResources();
}

void PluginRack::fromXml (PluginRack& rack, const juce::XmlElement& rackXml,
                          juce::AudioPluginFormatManager& formatManager,
                          const juce::Array<juce::PluginDescription>& knownTypes)
{
    for (auto* slotXml : rackXml.getChildIterator())
    {
        if (! slotXml->hasTagName ("slot"))
            continue;

        auto slot = std::make_unique<RackSlot>();
        slot->bypassed = slotXml->getBoolAttribute ("bypass", false);
        slot->gain     = (float) slotXml->getDoubleAttribute ("gain", 1.0);

        const auto uid  = slotXml->getStringAttribute ("uid");
        const auto name = slotXml->getStringAttribute ("name");
        for (auto& desc : knownTypes)
        {
            if (desc.uniqueId == uid || (! name.isEmpty() && desc.name == name))
            {
                juce::String err;
                slot->plugin = formatManager.createPluginInstance (desc, 48000.0, 512, err);
                break;
            }
        }

        rack.slots.add (slot.release());
    }
}

juce::XmlElement PluginRack::toXml() const
{
    juce::XmlElement xml ("rack");
    for (auto* s : slots)
    {
        auto* slotXml = xml.createNewChildElement ("slot");
        slotXml->setAttribute ("bypass", s->bypassed);
        slotXml->setAttribute ("gain", (double) s->gain);
        if (s->plugin != nullptr)
        {
            slotXml->setAttribute ("uid", s->plugin->getPluginDescription().uniqueId);
            slotXml->setAttribute ("name", s->plugin->getName());
        }
    }
    return xml;
}

void PluginRack::prepare (double sampleRate, int maxBlockSize)
{
    for (auto* s : slots)
        if (s->plugin != nullptr && ! s->plugin->isSuspended())
            s->plugin->prepareToPlay (sampleRate, maxBlockSize);
}

void PluginRack::releaseResources()
{
    for (auto* s : slots)
        if (s->plugin != nullptr)
            s->plugin->releaseResources();
}

void PluginRack::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    for (auto* s : slots)
    {
        if (s->plugin == nullptr || s->bypassed)
            continue;

        s->plugin->processBlock (buffer, midiMessages);

        if (s->gain != 1.0f)
            buffer.applyGain (s->gain);
    }
}
