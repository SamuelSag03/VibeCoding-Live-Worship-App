#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

class MidiClockSync;
class RemoteControl;

/**
 * MidiRouter — recibe MIDI de todos los teclados abiertos (YPG-235, YC88, MX88),
 * reenvía el transporte (Clock/Start/Stop/SPP) al MidiClockSync, consulta el
 * control remoto (mapeo MIDI) y acumula el resto para el hilo de audio.
 * El filtrado por zona (split/layer) lo hace el PatchManager en processBlock.
 */
class MidiRouter final : public juce::MidiInputCallback
{
public:
    MidiRouter();
    ~MidiRouter() override;

    void setClockSync (MidiClockSync* s) { clockSync = s; }
    void setRemoteControl (RemoteControl* r) { remoteControl = r; }

    /** Abre todos los dispositivos MIDI de entrada disponibles (hot-swap friendly). */
    void refreshDevices();

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    /** Vuelca los mensajes pendientes al buffer indicado. SOLO desde el hilo de audio. */
    void drainInto (juce::MidiBuffer& destination);

    juce::StringArray getOpenDeviceNames() const;

private:
    std::vector<std::unique_ptr<juce::MidiInput>> openInputs;
    MidiClockSync* clockSync = nullptr;
    RemoteControl* remoteControl = nullptr;
    juce::MidiBuffer pending;
    juce::CriticalSection pendingLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRouter)
};
