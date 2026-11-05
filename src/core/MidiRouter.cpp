#include "MidiRouter.h"
#include "MidiClockSync.h"
#include "RemoteControl.h"

MidiRouter::MidiRouter() { refreshDevices(); }

MidiRouter::~MidiRouter()
{
    for (auto& in : openInputs)
        in->stop();
}

void MidiRouter::refreshDevices()
{
    auto available = juce::MidiInput::getAvailableDevices();
    openInputs.clear();
    for (auto& info : available)
    {
        if (auto in = juce::MidiInput::openDevice (info.identifier, this))
        {
            in->start();
            openInputs.push_back (std::move (in));
        }
    }
}

void MidiRouter::handleIncomingMidiMessage (juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    // Transporte: va al reloj de sync (Ableton Live -> MIDI Clock)
    if (message.isMidiClock() || message.isMidiStart() || message.isMidiStop() ||
        message.isMidiContinue() || message.isSongPositionPointer())
    {
        if (clockSync != nullptr)
            clockSync->handleTransport (message);
        return;
    }

    // Control remoto por MIDI (mapeo personalizable)
    if (remoteControl != nullptr)
        remoteControl->handleMidiRemote (message);

    const juce::ScopedLock lock (pendingLock);
    pending.addEvent (message, 0);
}

void MidiRouter::drainInto (juce::MidiBuffer& destination)
{
    if (pendingLock.tryEnter())
    {
        destination.addEvents (pending, 0, pending.getNumEvents(), 0);
        pending.clear();
        pendingLock.exit();
    }
}

juce::StringArray MidiRouter::getOpenDeviceNames() const
{
    juce::StringArray names;
    for (auto& in : openInputs)
        names.add (in->getName());
    return names;
}
