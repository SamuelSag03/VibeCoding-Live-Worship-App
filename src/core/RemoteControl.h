#pragma once

#include <juce_osc/juce_osc.h>
#include <juce_audio_devices/juce_audio_devices.h>

class PatchManager;

/**
 * RemoteControl — control remoto de la app (OSC UDP + mapeo MIDI personalizable).
 *
 *  OSC (puerto 8000 por defecto):
 *    /patch/select <int>  /patch/next  /patch/prev  /layer/bypass <layer> <0|1>
 *
 *  MIDI: tabla MidiMapping { key = (status & 0xF0) | numero, action, param }
 *    action: 0 = siguiente patch, 1 = anterior, 2 = seleccionar patch (param),
 *            3 = bypass de layer (param = índice de layer)
 */
class RemoteControl final : private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    RemoteControl();
    ~RemoteControl() override;

    void start (PatchManager* pm, int oscPort = 8000);
    void stop();

    void handleMidiRemote (const juce::MidiMessage& m);

    void selectPatch (int index);
    void nextPatch();
    void prevPatch();
    void setLayerBypass (int layer, bool bypassed);

    struct MidiMapping
    {
        int key;    // (status & 0xF0) | numero de CC/PC/Note
        int action; // 0 = siguiente, 1 = anterior, 2 = patch N, 3 = bypass layer
        int param;
    };

    void setMidiMappings (const juce::Array<MidiMapping>& m) { midiMappings = m; }
    const juce::Array<MidiMapping>& getMidiMappings() const { return midiMappings; }

    juce::XmlElement toXml() const;
    void fromXml (const juce::XmlElement& xml);

    bool isOscConnected() const { return connected.load(); }
    int getOscPort() const { return port; }

private:
    void oscMessageReceived (const juce::OSCMessage& message) override;
    void oscBundleReceived (const juce::OSCBundle& bundle) override;

    PatchManager* patchManager = nullptr;
    juce::OSCReceiver oscReceiver;
    int port = 8000;
    std::atomic<bool> connected { false };
    juce::Array<MidiMapping> midiMappings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RemoteControl)
};
