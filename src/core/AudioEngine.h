#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

class PatchManager;
class MidiRouter;

/**
 * AudioEngine — envuelve AudioDeviceManager con foco en JACK (Linux/Arch vía
 * PipeWire-jack), fallback a ALSA. Hot-swap de dispositivos y callback RT sin
 * allocations; delega el procesamiento en el PatchManager activo.
 */
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void setPatchManager (PatchManager* pm);
    void setMidiRouter (MidiRouter* mr);

    /** Abre JACK primero; si no está, ALSA. */
    juce::String initialise (int numInputs, int numOutputs);

    /** Re-engancha el dispositivo tras un hot-swap (cambio de interfaz/teclado). */
    void rescanAndReopen();

    void audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numSamples) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::String getLastError() const { return lastError; }
    juce::String getCurrentDeviceName() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    double getMeasuredLatencyMs() const;
    float getCpuUsage() const { return deviceManager.getCpuUsage(); }

private:
    juce::AudioDeviceManager deviceManager;
    PatchManager* patchManager = nullptr;
    MidiRouter* midiRouter = nullptr;
    juce::String lastError;
    double measuredLatencyMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
