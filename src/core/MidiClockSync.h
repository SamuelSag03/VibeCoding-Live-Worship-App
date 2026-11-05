#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

/**
 * MidiClockSync
 * -------------
 * Sincronía de tiempo con Ableton Live u otro DAW por MIDI Clock (24 pulsos por negra),
 * Start/Stop/Continue y Song Position Pointer (SPP). Mantiene el tempo estimado (BPM)
 * y el contador de compás para mostrarlo en UI y reenviarlo a plugins que sigan
 * el transporte del host.
 */
class MidiClockSync
{
public:
    void handleTransport (const juce::MidiMessage& m);

    bool isPlaying() const { return playing.load(); }
    double getCurrentBpm() const { return bpm.load(); }
    int64 getTickCount() const { return ticks.load(); }

    juce::String getStatusText() const;

private:
    std::atomic<bool> playing { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int64> ticks { 0 };

    // Para estimar BPM: medimos muestras entre relojes
    int clocksSinceLastBeat = 0;
    double lastBeatTimeSeconds = 0.0;
    juce::Time lastBeatTime;
};
