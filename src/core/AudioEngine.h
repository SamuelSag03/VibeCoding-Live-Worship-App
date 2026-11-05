#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * AudioEngine
 * -----------
 * Envuelve el AudioDeviceManager de JUCE con foco en JACK (el estándar en Linux/Arch
 * vía PipeWire-jack). Responsabilidades:
 *   - Abrir JACK primero; si no está, caer a ALSA (útil en casa con la M-TRACK DUO).
 *   - Hot-swap de dispositivos: reacciona a audioDeviceIOCallbackEndedWithError /
 *     cambios de dispositivo y reengancha sin cerrar la app (cambiar de YPG-235+M-Track
 *     a YC88/MX88 en la iglesia es solo reconectar USB; JACK lo refleja y nosotros
 *     re-enganchamos el callback).
 *   - Callback de audio en tiempo real: NUNCA asigna memoria aquí; delega en el
 *     PatchManager activo.
 */
class PatchManager;
class MidiRouter;

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void setPatchManager (PatchManager* pm);
    void setMidiRouter (MidiRouter* mr);

    /** Intenta abrir JACK con el tamaño de buffer pedido; si falla, ALSA. */
    juce::String initialise (int numInputs, int numOutputs, int sampleRate, int bufferSize);

    /** Re-engancha el dispositivo actual tras un hot-swap (cambio de interfaz/teclado). */
    void rescanAndReopen();

    // AudioIODeviceCallback (modo sin lista explícita de dispositivos)
    void audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numSamples) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // AudioIODeviceCallbackWithDevices
    void audioDeviceIOCallbackWithDevices (const float* const* inputData, int numInputChannels,
                                           float* const* outputData, int numOutputChannels,
                                           int numSamples, int deviceIndex) override;

    juce::String getLastError() const { return lastError; }
    juce::String getCurrentDeviceName() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    double getMeasuredLatencyMs() const;
    float getCpuUsage() const { return deviceManager.getCpuUsage(); }

private:
    juce::AudioDeviceManager deviceManager;
    PatchManager* patchManager = nullptr;
    juce::String lastError;
    double measuredLatencyMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
