#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

/**
 * MidiClockSync — sincronía con Ableton Live u otro DAW por MIDI Clock (24 ppqn),
 * Start/Stop/Continue y Song Position Pointer. Mantiene tempo estimado (BPM) y
 * contador de ticks para la UI y para reenviar transporte a los plugins.
 */
class MidiClockSync
{
public:
    void handleTransport (const juce::MidiMessage& m);

    bool isPlaying() const { return playing.load(); }
    double getCurrentBpm() const { return bpm.load(); }
    juce::int64 getTickCount() const { return ticks.load(); }

    juce::String getStatusText() const;

private:
    std::atomic<bool> playing { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<juce::int64> ticks { 0 };

    int clocksSinceLastBeat = 0;
    juce::Time lastBeatTime;
};
