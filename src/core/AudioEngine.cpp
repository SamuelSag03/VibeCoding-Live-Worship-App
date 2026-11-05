#include "AudioEngine.h"
#include "PatchManager.h"
#include "MidiRouter.h"

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

void AudioEngine::setPatchManager (PatchManager* pm) { patchManager = pm; }
void AudioEngine::setMidiRouter (MidiRouter* mr)     { midiRouter = mr; }

juce::String AudioEngine::initialise (int numInputs, int numOutputs)
{
    const auto& types = deviceManager.getAvailableDeviceTypes();
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
        return "No se encontro ningun dispositivo de audio (instala jack2/pipewire-jack o alsa).";

    lastError = deviceManager.initialise (numInputs, numOutputs, nullptr, true, chosenType, nullptr);
    if (lastError.isNotEmpty())
        return lastError;

    deviceManager.addAudioCallback (this);
    return {};
}

void AudioEngine::rescanAndReopen()
{
    deviceManager.removeAudioCallback (this);
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

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                                    float* const* out, int numOut, int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    juce::AudioBuffer<float> buffer (numOut, numSamples);
    buffer.clear();

    // Entradas (audio de teclados con interfaz propia) a las primeras canales
    for (int ch = 0; ch < juce::jmin (numIn, numOut); ++ch)
        juce::FloatVectorOperations::copy (buffer.getWritePointer (ch), in[ch], numSamples);

    juce::MidiBuffer midi;
    if (midiRouter != nullptr)
        midiRouter->drainInto (midi);

    if (patchManager != nullptr)
        patchManager->processBlock (buffer, midi);

    for (int ch = 0; ch < numOut; ++ch)
        juce::FloatVectorOperations::copy (out[ch], buffer.getReadPointer (ch), numSamples);
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
