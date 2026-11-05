#include "AudioEngine.h"
#include "PatchManager.h"
#include "MidiRouter.h"

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    deviceManager.closeAudioDevice();
    deviceManager.removeAudioCallback (this);
}

void AudioEngine::setPatchManager (PatchManager* pm) { patchManager = pm; }

juce::String AudioEngine::initialise (int numInputs, int numOutputs, int sampleRate, int bufferSize)
{
    // Preferimos JACK (mejor latencia y hot-swap en Linux); si no está, ALSA.
    auto types = deviceManager.getAvailableDeviceTypes();
    juce::String chosenType;
    for (auto* t : types)
        if (t->getTypeName().containsIgnoreCase ("jack"))
            { chosenType = t->getTypeName(); break; }
    if (chosenType.isEmpty())
        for (auto* t : types)
            if (t->getTypeName().containsIgnoreCase ("alsa"))
                { chosenType = t->getTypeName(); break; }
    if (chosenType.isEmpty() && ! types.isEmpty())
        chosenType = types.getFirst()->getTypeName();

    if (chosenType.isEmpty())
        return "No se encontró ningún dispositivo de audio (instala jack2/pipewire-jack o alsa).";

    lastError = deviceManager.initialise (numInputs, numOutputs, nullptr, true, chosenType, nullptr);
    if (lastError.isNotEmpty())
        return lastError;

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        dev->setSampleRateIfSupported ((double) sampleRate);
        dev->setBufferSizeIfSupported (bufferSize);
    }

    deviceManager.addAudioCallback (this);
    return {};
}

void AudioEngine::rescanAndReopen()
{
    // Tras un hot-swap (cambio de interfaz/teclado USB): reescanea y reabre sin parar.
    deviceManager.closeAudioDevice();
    lastError = deviceManager.initialise (2, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback (this);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    measuredLatencyMs = 1000.0 * device->getCurrentBufferSizeSamples() / device->getCurrentSampleRate()
                        + device->getOutputLatencyInSamples() * 1000.0 / device->getCurrentSampleRate();
    if (patchManager != nullptr)
        patchManager->prepareToPlay (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    if (patchManager != nullptr)
        patchManager->releaseResources();
}

void AudioEngine::audioDeviceIOCallback (const float** in, int numIn,
                                         float** out, int numOut, int numSamples)
{
    juce::AudioBuffer<float> inBuffer (numIn, numSamples);
    inBuffer.copyFromArray (0, in, numIn, numSamples);
    juce::AudioBuffer<float> buffer (juce::jmax (numIn, numOut), numSamples);
    buffer.copyFrom (0, 0, inBuffer, 0, 0, juce::jmin (numIn, buffer.getNumChannels()));

    juce::MidiBuffer midi;
    // (los mensajes MIDI llegan por MidiRouter; aquí sólo procesamos audio)

    if (patchManager != nullptr)
        patchManager->processBlock (buffer, midi);

    for (int ch = 0; ch < numOut; ++ch)
        juce::FloatVectorOperations::copy (out[ch], buffer.getReadPointer (juce::jmin (ch, buffer.getNumChannels() - 1)), numSamples);
}

void AudioEngine::audioDeviceIOCallbackWithDevices (const float* const* in, int numIn,
                                                    float* const* out, int numOut,
                                                    int numSamples, int /*deviceIndex*/)
{
    audioDeviceIOCallback (in, numIn, out, numOut, numSamples);
}

juce::String AudioEngine::getCurrentDeviceName() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr
               ? deviceManager.getCurrentAudioDevice()->getName()
               : juce::String();
}

double AudioEngine::getCurrentSampleRate() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr
               ? deviceManager.getCurrentAudioDevice()->getCurrentSampleRate() : 0.0;
}

int AudioEngine::getCurrentBufferSize() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr
               ? deviceManager.getCurrentAudioDevice()->getCurrentBufferSizeSamples() : 0;
}

double AudioEngine::getMeasuredLatencyMs() const { return measuredLatencyMs; }
