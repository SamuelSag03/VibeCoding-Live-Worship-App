#include "RemoteControl.h"
#include "PatchManager.h"

RemoteControl::RemoteControl() = default;
RemoteControl::~RemoteControl() { stop(); }

void RemoteControl::start (PatchManager* pm, int oscPort)
{
    patchManager = pm;
    port = oscPort;
    if (oscReceiver.connect (port))
    {
        oscReceiver.addListener (this);
        connected.store (true);
    }
}

void RemoteControl::stop()
{
    oscReceiver.removeListener (this);
    oscReceiver.disconnect();
    connected.store (false);
}

void RemoteControl::selectPatch (int index) { if (patchManager) patchManager->selectPatch (index); }
void RemoteControl::nextPatch()             { if (patchManager) patchManager->selectNextPatch(); }
void RemoteControl::prevPatch()             { if (patchManager) patchManager->selectPreviousPatch(); }
void RemoteControl::setLayerBypass (int layer, bool bypassed)
{
    if (patchManager != nullptr)
        patchManager->setActiveLayerBypassed (layer, bypassed);
}

void RemoteControl::oscMessageReceived (const juce::OSCMessage& message)
{
    const auto addr = message.getAddressPattern().toString();

    if (addr == "/patch/select" && message.size() >= 1)
        selectPatch (message[0].getInt32());
    else if (addr == "/patch/next")
        nextPatch();
    else if (addr == "/patch/prev")
        prevPatch();
    else if (addr == "/layer/bypass" && message.size() >= 2)
        setLayerBypass (message[0].getInt32(), message[1].getInt32() != 0);
}

void RemoteControl::oscBundleReceived (const juce::OSCBundle& bundle)
{
    for (auto& m : bundle)
        oscMessageReceived (m);
}

void RemoteControl::handleMidiRemote (const juce::MidiMessage& m)
{
    // 1) Mapeo personalizable (cargado del concert)
    if (m.getRawDataSize() >= 2)
    {
        const int key = (m.getRawData()[0] & 0xF0) | m.getRawData()[1];
        for (auto& map : midiMappings)
        {
            if (map.key == key)
            {
                switch (map.action)
                {
                    case 0: nextPatch(); break;
                    case 1: prevPatch(); break;
                    case 2: selectPatch (map.param); break;
                    case 3: setLayerBypass (map.param, true); break;
                    default: break;
                }
                return;
            }
        }
    }

    // 2) Mapeo por defecto si no hay mapeo configurado
    if (midiMappings.isEmpty())
    {
        if (m.isProgramChange())
            selectPatch (m.getProgramChangeNumber());
        else if (m.isController() && m.getControllerNumber() == 115)
            nextPatch();
        else if (m.isController() && m.getControllerNumber() == 116)
            prevPatch();
    }
}

juce::XmlElement RemoteControl::toXml() const
{
    juce::XmlElement xml ("remote");
    xml.setAttribute ("oscPort", port);
    for (auto& map : midiMappings)
    {
        auto* mXml = xml.createNewChildElement ("midiMap");
        mXml->setAttribute ("key", map.key);
        mXml->setAttribute ("action", map.action);
        mXml->setAttribute ("param", map.param);
    }
    return xml;
}

void RemoteControl::fromXml (const juce::XmlElement& xml)
{
    midiMappings.clear();
    port = xml.getIntAttribute ("oscPort", 8000);
    for (auto* mXml : xml.getChildIterator())
    {
        if (! mXml->hasTagName ("midiMap"))
            continue;
        MidiMapping map;
        map.key    = mXml->getIntAttribute ("key");
        map.action = mXml->getIntAttribute ("action");
        map.param  = mXml->getIntAttribute ("param");
        midiMappings.add (map);
    }
}
