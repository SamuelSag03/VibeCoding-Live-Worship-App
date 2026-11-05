#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

/**
 * MidiRouter
 * ----------
 * Recibe MIDI de todos los dispositivos abiertos (YPG-235 en casa, YC88/MX88 en la iglesia)
 * y lo dirige al patch activo aplicando:
 *   - filtro por dispositivo y canal MIDI
 *   - transposición (útil para splits/teclas diferentes)
 *   - zonas de split (rangos de nota por patch)
 * Además reenvía MIDI Clock/Start/Stop al MidiClockSync para sincronía con Ableton Live.
 */
class MidiClockSync;

class MidiRouter final : public juce::MidiInputCallback
{
public:
    MidiRouter();
    ~MidiRouter() override;

    void setClockSync (MidiClockSync* s) { clockSync = s; }

    /** Abre todos los dispositivos MIDI de entrada disponibles (hot-swap friendly). */
    void refreshDevices();

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    /** Vuelca los mensajes pendientes al buffer indicado y lo vacía.
     *  Llamar SOLO desde el hilo de audio (usa tryLock para no bloquear RT). */
    void drainInto (juce::MidiBuffer& destination);

    juce::StringArray getOpenDeviceNames() const;

    // Configuración del patch activo
    void setChannelFilter (int channel);   // 0 = todos
    void setTranspose (int semitones);
    void setSplit (int lowNote, int highNote); // -1 = sin split

private:
    std::vector<std::unique_ptr<juce::MidiInput>> openInputs;
    MidiClockSync* clockSync = nullptr;
    RemoteControl* remoteControl = nullptr;
    juce::MidiBuffer pending;      // mensajes del ciclo actual (hilo MIDI)
    juce::CriticalSection pendingLock;
    int channelFilter = 0;
    int transpose = 0;
    int splitLow = -1, splitHigh = -1;
    std::atomic<bool> deviceListChanged { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRouter)
};
