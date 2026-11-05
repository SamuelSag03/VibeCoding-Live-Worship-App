#include "MidiRouter.h"
#include "MidiClockSync.h"
#include "RemoteControl.h"

MidiRouter::MidiRouter() { refreshDevices(); }

MidiRouter::~MidiRouter()
{
    for (auto& in : openInputs)
    {
        in->stop();
    }
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
    // Transporte: va al reloj de sync (Ableton Live → MIDI Clock)
    if (message.isMidiClock() || message.isStart() || message.isStop() ||
        message.isContinue() || message.isSongPositionPointer())
    {
        if (clockSync != nullptr)
            clockSync->handleTransport (message);
        return;
    }

    // Filtro por canal del patch activo
    if (channelFilter > 0 && message.getChannel() != channelFilter)
        return;

    juce::MidiMessage out = message;

    // Split por rango de notas
    if (splitLow >= 0 && out.isNoteOnOrOff())
    {
        const int n = out.getNoteNumber();
        if (n < splitLow || n > splitHigh)
            return;
    }

    // Transposición
    if (transpose != 0 && out.isNoteOnOrOff())
        out.setNoteNumber (juce::jlimit (0, 127, out.getNoteNumber() + transpose));

    const juce::ScopedLock lock (pendingLock);
    pending.addEvent (out, 0);
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

void MidiRouter::setChannelFilter (int channel) { channelFilter = channel; }
void MidiRouter::setTranspose (int semitones)   { transpose = semitones; }
void MidiRouter::setSplit (int lowNote, int highNote) { splitLow = lowNote; splitHigh = highNote; }
