#pragma once

#include <juce_core/juce_core.h>

class PatchManager;
class RemoteControl;

/** Guarda / carga el "concert" (patches + mapeo remoto) como XML en disco. */
namespace Persistence
{
    bool saveConcert (const PatchManager& pm, const RemoteControl& rc, const juce::File& file);
    bool loadConcert (PatchManager& pm, RemoteControl& rc, const juce::File& file);
}
