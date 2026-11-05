#include "MidiClockSync.h"

void MidiClockSync::handleTransport (const juce::MidiMessage& m)
{
    if (m.isMidiStart() || m.isMidiContinue())
    {
        playing.store (true);
        ticks.store (0);
        clocksSinceLastBeat = 0;
        lastBeatTime = juce::Time::getCurrentTime();
    }
    else if (m.isMidiStop())
    {
        playing.store (false);
    }
    else if (m.isMidiClock())
    {
        ticks.fetch_add (1);
        if (++clocksSinceLastBeat >= 24) // una negra completa
        {
            clocksSinceLastBeat = 0;
            const auto now = juce::Time::getCurrentTime();
            const double dt = (now - lastBeatTime).inSeconds();
            if (dt > 0.0)
                bpm.store (60.0 / dt);
            lastBeatTime = now;
        }
    }
    else if (m.isSongPositionPointer())
    {
        // SPP: valor de 14 bits, cada unidad = 6 pulsos de reloj
        ticks.store ((juce::int64) m.getSongPositionPointerMidiValue() * 6);
    }
}

juce::String MidiClockSync::getStatusText() const
{
    return (playing.load() ? "▶ SYNC" : "■ STOP") + juce::String ("  ") +
           juce::String (bpm.load(), 1) + " BPM  tick " + juce::String (ticks.load());
}
