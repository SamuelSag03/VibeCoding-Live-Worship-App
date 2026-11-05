#pragma once

#include <juce_osc/juce_osc.h>
#include <juce_audio_devices/juce_audio_devices.h>

class PatchManager;

/**
 * RemoteControl
 * -------------
 * Control remoto de la app, clave para un worship leader que dispara patches desde
 * otro dispositivo (tablet, Stream Deck, pedalera MIDI, etc.):
 *
 *  OSC (UDP, puerto por defecto 8000) — mensajes personalizables:
 *    /patch/select <int>     -> selecciona patch por índice
 *    /patch/next  /patch/prev
 *    /patch/name             -> responde con el nombre del patch activo
 *    /rack/bypass <slot> <0|1>
 *
 *  MIDI — mapeo personalizable (CC/PC/Note → acción):
 *    CC/PC/Note -> patch siguiente/anterior, seleccionar patch N, bypass de slot.
 */
class RemoteControl final : private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    RemoteControl();
    ~RemoteControl() override;

    void start (PatchManager* pm, int oscPort = 8000);
    void stop();

    /** Mapeo MIDI: (statusByte & 0xF0, data1) -> acción. Llamar desde el router MIDI. */
    void handleMidiRemote (const juce::MidiMessage& m);

    // --- Mapeo personalizable (persistente en el concert) ---
    struct MidiMapping
    {
        int key;    // (status & 0xF0) | numero (CC/PC/Note)
        int action; // 0 = siguiente, 1 = anterior, 2 = seleccionar patch (param = índice), 3 = bypass slot (param = slot)
        int param;
    };

    void setMidiMappings (const juce::Array<MidiMapping>& m) { midiMappings = m; }
    const juce::Array<MidiMapping>& getMidiMappings() const { return midiMappings; }

    juce::XmlElement toXml() const;
    void fromXml (const juce::XmlElement& xml);

    // Acciones públicas (también invocadas por OSC/MIDI)
    void selectPatch (int index);
    void nextPatch();
    void prevPatch();
    void setSlotBypass (int slot, bool bypassed);

    bool isOscConnected() const { return connected.load(); }
    int getOscPort() const { return port; }

private:
    void oscMessageReceived (const juce::OSCMessage& message) override;
    void oscBundleReceived (const juce::OSCBundle& bundle) override;

    PatchManager* patchManager = nullptr;
    juce::OSCReceiver oscReceiver;
    int port = 8000;
    std::atomic<bool> connected { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RemoteControl)
};
