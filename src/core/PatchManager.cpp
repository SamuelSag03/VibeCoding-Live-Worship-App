#include "PatchManager.h"

PatchManager::PatchManager() = default;

// ---------------------------------------------------------------- concert
int PatchManager::addPatch (const juce::String& name)
{
    Patch p;
    p.name = name;
    patches.push_back (std::move (p));
    return (int) patches.size() - 1;
}

bool PatchManager::removePatch (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPatches()))
        return false;
    patches.erase (patches.begin() + index);
    return true;
}

PatchManager::Patch* PatchManager::getPatch (int index)
{
    return juce::isPositiveAndBelow (index, getNumPatches()) ? &patches[(size_t) index] : nullptr;
}

const PatchManager::Patch* PatchManager::getPatch (int index) const
{
    return juce::isPositiveAndBelow (index, getNumPatches()) ? &patches[(size_t) index] : nullptr;
}

juce::StringArray PatchManager::getPatchNames() const
{
    juce::StringArray names;
    for (auto& p : patches) names.add (p.name);
    return names;
}

// ---------------------------------------------------------------- layers
int PatchManager::addLayer (int patchIndex, const juce::String& name)
{
    auto* p = getPatch (patchIndex);
    if (p == nullptr) return -1;
    auto* layer = new Layer();
    layer->name = name;
    layer->rack = std::make_unique<PluginRack>();
    const int idx = p->layers.size();
    p->layers.add (layer);
    return idx;
}

bool PatchManager::addPluginToLayer (int patchIndex, int layerIndex,
                                     std::unique_ptr<juce::AudioPluginInstance> plugin)
{
    auto* p = getPatch (patchIndex);
    if (p == nullptr || plugin == nullptr || ! juce::isPositiveAndBelow (layerIndex, p->layers.size()))
        return false;
    // Se encola: el hilo de audio aplica el cambio en el límite de buffer
    const juce::ScopedLock lock (pendingLock);
    PendingOp op;
    op.patchIndex = patchIndex; op.layerIndex = layerIndex; op.slotIndex = -1;
    op.isAdd = true; op.plugin = std::move (plugin);
    pendingOps.push_back (std::move (op));
    return true;
}

bool PatchManager::removeSlotFromLayer (int patchIndex, int layerIndex, int slotIndex)
{
    auto* p = getPatch (patchIndex);
    if (p == nullptr || ! juce::isPositiveAndBelow (layerIndex, p->layers.size()))
        return false;
    const juce::ScopedLock lock (pendingLock);
    PendingOp op;
    op.patchIndex = patchIndex; op.layerIndex = layerIndex; op.slotIndex = slotIndex;
    op.isAdd = false;
    pendingOps.push_back (std::move (op));
    return true;
}

void PatchManager::applyPendingOps()
{
    std::vector<PendingOp> ops;
    {
        const juce::ScopedLock lock (pendingLock);
        ops.swap (pendingOps);
    }
    for (auto& op : ops)
    {
        auto* p = getPatch (op.patchIndex);
        if (p == nullptr || ! juce::isPositiveAndBelow (op.layerIndex, p->layers.size()))
            continue;
        auto& rack = *p->layers[(size_t) op.layerIndex]->rack;
        if (op.isAdd)
        {
            rack.addSlot (std::move (op.plugin));
            rack.prepare (lastSampleRate, lastMaxBlockSize); // el runtime activo comparte este rack
        }
        else
        {
            rack.removeSlot (op.slotIndex);
        }
    }
}

// ---------------------------------------------------------------- variations
int PatchManager::addVariation (int patchIndex, const juce::String& name)
{
    auto* p = getPatch (patchIndex);
    if (p == nullptr) return -1;
    Variation v;
    v.name = name;
    p->variations.add (v);
    return p->variations.size() - 1;
}

void PatchManager::selectVariation (int patchIndex, int variationIndex)
{
    auto* p = getPatch (patchIndex);
    if (p == nullptr) return;
    p->activeVariation = variationIndex;

    if (patchIndex == activeIndex.load() && runtimeA != nullptr)
    {
        for (int i = 0; i < runtimeA->layerDefs.size(); ++i)
            runtimeA->layerDefs.getReference (i).gain = p->layers[(size_t) i]->gain;

        if (juce::isPositiveAndBelow (variationIndex, p->variations.size()))
        {
            for (auto& ov : p->variations.getReference ((size_t) variationIndex).overrides)
                if (juce::isPositiveAndBelow (ov.layerIndex, runtimeA->layerDefs.size()))
                {
                    runtimeA->layerDefs.getReference ((size_t) ov.layerIndex).gain = ov.gain;
                    for (int s = 0; s < runtimeA->layerRacks[(size_t) ov.layerIndex]->getNumSlots(); ++s)
                        runtimeA->layerRacks[(size_t) ov.layerIndex]->setSlotBypassed (s, ov.bypass);
                }
        }
    }
}

int PatchManager::getActiveVariation (int patchIndex) const
{
    auto* p = getPatch (patchIndex);
    return p != nullptr ? p->activeVariation : -1;
}

void PatchManager::setActiveLayerBypassed (int layerIndex, bool bypassed)
{
    if (runtimeA != nullptr && juce::isPositiveAndBelow (layerIndex, runtimeA->layerRacks.size()))
        for (int s = 0; s < runtimeA->layerRacks[(size_t) layerIndex]->getNumSlots(); ++s)
            runtimeA->layerRacks[(size_t) layerIndex]->setSlotBypassed (s, bypassed);
}

// ---------------------------------------------------------------- switching
void PatchManager::selectPatch (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPatches()))
    {
        auto* p = getPatch (index);
        for (auto* layer : p->layers)
            layer->rack->prepare (lastSampleRate, lastMaxBlockSize);

        pendingIndex.store (index);
        swapPending.store (true);
    }
}

void PatchManager::selectNextPatch()
{
    const int cur = activeIndex.load();
    selectPatch (juce::isPositiveAndBelow (cur, getNumPatches() - 1) ? cur + 1 : 0);
}

void PatchManager::selectPreviousPatch()
{
    const int cur = activeIndex.load();
    selectPatch (cur > 0 ? cur - 1 : juce::jmax (0, getNumPatches() - 1));
}

std::unique_ptr<PatchManager::PatchRuntime> PatchManager::makeRuntime (const Patch& p)
{
    auto rt = std::make_unique<PatchRuntime>();
    for (auto* layer : p.layers)
    {
        rt->layerRacks.add (layer->rack.get());
        LayerDef def;
        def.keyLow = layer->keyLow; def.keyHigh = layer->keyHigh;
        def.channelFilter = layer->channelFilter; def.transpose = layer->transpose;
        def.gain = layer->gain; def.pan = layer->pan; def.outputBus = layer->outputBus;
        rt->layerDefs.add (def);
    }
    return rt;
}

void PatchManager::performSwapIfNeeded()
{
    if (! swapPending.load())
        return;

    const int next = pendingIndex.load();
    if (! juce::isPositiveAndBelow (next, getNumPatches()))
    {
        swapPending.store (false);
        return;
    }

    tail = std::move (runtimeA);          // patch persist
    runtimeA = std::move (runtimeB);
    runtimeB = makeRuntime (*getPatch (next));

    activeIndex.store (next);
    tailBuffersRemaining = tailBuffers;
    swapPending.store (false);

    if (onPatchChanged != nullptr)
        onPatchChanged (next);
}

// ---------------------------------------------------------------- audio
void PatchManager::prepareToPlay (double sampleRate, int maxBlockSize)
{
    lastSampleRate = sampleRate;
    lastMaxBlockSize = maxBlockSize;
    scratch.setSize (2, juce::jmax (maxBlockSize, crossfadeSamples));
    scratch.clear();

    for (auto& p : patches)
        for (auto* layer : p.layers)
            layer->rack->prepare (sampleRate, maxBlockSize);

    if (activeIndex.load() < 0 && getNumPatches() > 0)
        selectPatch (0);
}

void PatchManager::releaseResources()
{
    for (auto& p : patches)
        for (auto* layer : p.layers)
            layer->rack->releaseResources();
}

void PatchManager::processLayer (const LayerDef& def, PluginRack& rack,
                                 juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn,
                                 juce::AudioBuffer<float>& scratchBuf, int numSamples)
{
    juce::MidiBuffer layerMidi;
    for (auto meta : midiIn)
    {
        auto m = meta.getMessage();
        if (m.isNoteOnOrOff())
        {
            if (def.channelFilter > 0 && m.getChannel() != def.channelFilter)
                continue;
            const int n = m.getNoteNumber();
            if (n < def.keyLow || n > def.keyHigh)
                continue;
            if (def.transpose != 0)
                m.setNoteNumber (juce::jlimit (0, 127, n + def.transpose));
        }
        else if (def.channelFilter > 0 && m.getChannel() != def.channelFilter)
        {
            continue;
        }
        layerMidi.addEvent (m, meta.samplePosition);
    }

    scratchBuf.setSize (2, numSamples, false, false, true);
    scratchBuf.clear();
    rack.process (scratchBuf, layerMidi);

    const int chL = juce::jmin (def.outputBus * 2, out.getNumChannels() - 1);
    const int chR = juce::jmin (chL + 1, out.getNumChannels() - 1);
    const float gl = def.gain * (def.pan <= 0.0f ? 1.0f : 1.0f - def.pan);
    const float gr = def.gain * (def.pan >= 0.0f ? 1.0f : 1.0f + def.pan);
    for (int i = 0; i < numSamples; ++i)
    {
        out.addFrom (chL, i, scratchBuf, 0, i, 1, gl);
        out.addFrom (chR, i, scratchBuf, juce::jmin (1, scratchBuf.getNumChannels() - 1), i, 1, gr);
    }
}

void PatchManager::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    performSwapIfNeeded();
    applyPendingOps();

    const int numSamples = buffer.getNumSamples();

    if (runtimeA != nullptr)
        for (int i = 0; i < runtimeA->layerDefs.size(); ++i)
            processLayer (runtimeA->layerDefs.getReference ((size_t) i), *runtimeA->layerRacks[(size_t) i],
                          buffer, midiMessages, scratch, numSamples);

    // Cola del patch anterior (patch persist) con fade-out
    if (tail != nullptr && tailBuffersRemaining > 0)
    {
        juce::AudioBuffer<float> tailBuf (buffer.getNumChannels(), numSamples);
        tailBuf.clear();
        juce::MidiBuffer emptyMidi;
        for (int i = 0; i < tail->layerDefs.size(); ++i)
            processLayer (tail->layerDefs.getReference ((size_t) i), *tail->layerRacks[(size_t) i],
                          tailBuf, emptyMidi, scratch, numSamples);

        const int fadeLen = juce::jmin (crossfadeSamples, numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < fadeLen; ++i)
            {
                const float g = 1.0f - (float) i / (float) fadeLen;
                buffer.addFrom (ch, i, tailBuf, ch, i, 1, g);
            }

        if (--tailBuffersRemaining == 0)
            tail.reset();
    }
}

// ---------------------------------------------------------------- persistencia
juce::XmlElement PatchManager::toXml() const
{
    juce::XmlElement xml ("concert");
    xml.setAttribute ("activePatch", activeIndex.load());
    for (auto& p : patches)
    {
        auto* pXml = xml.createNewChildElement ("patch");
        pXml->setAttribute ("name", p.name);
        pXml->setAttribute ("activeVariation", p.activeVariation);
        for (auto* layer : p.layers)
        {
            auto* lXml = pXml->createNewChildElement ("layer");
            lXml->setAttribute ("name", layer->name);
            lXml->setAttribute ("keyLow", layer->keyLow);
            lXml->setAttribute ("keyHigh", layer->keyHigh);
            lXml->setAttribute ("channel", layer->channelFilter);
            lXml->setAttribute ("transpose", layer->transpose);
            lXml->setAttribute ("gain", (double) layer->gain);
            lXml->setAttribute ("pan", (double) layer->pan);
            lXml->setAttribute ("outputBus", layer->outputBus);
            if (layer->rack != nullptr)
                lXml->addChildElement (new juce::XmlElement (layer->rack->toXml()));
        }
        for (auto& v : p.variations)
        {
            auto* vXml = pXml->createNewChildElement ("variation");
            vXml->setAttribute ("name", v.name);
            for (auto& ov : v.overrides)
            {
                auto* oXml = vXml->createNewChildElement ("override");
                oXml->setAttribute ("layer", ov.layerIndex);
                oXml->setAttribute ("gain", (double) ov.gain);
                oXml->setAttribute ("bypass", ov.bypass);
            }
        }
    }
    return xml;
}

bool PatchManager::fromXml (const juce::XmlElement& concertXml)
{
    if (formatManager == nullptr)
        return false;

    patches.clear();
    for (auto* pXml : concertXml.getChildIterator())
    {
        if (! pXml->hasTagName ("patch"))
            continue;
        Patch p;
        p.name = pXml->getStringAttribute ("name");
        p.activeVariation = pXml->getIntAttribute ("activeVariation", -1);
        for (auto* lXml : pXml->getChildIterator())
        {
            if (lXml->hasTagName ("layer"))
            {
                auto* layer = new Layer();
                layer->name = lXml->getStringAttribute ("name");
                layer->keyLow = lXml->getIntAttribute ("keyLow", 0);
                layer->keyHigh = lXml->getIntAttribute ("keyHigh", 127);
                layer->channelFilter = lXml->getIntAttribute ("channel", 0);
                layer->transpose = lXml->getIntAttribute ("transpose", 0);
                layer->gain = (float) lXml->getDoubleAttribute ("gain", 1.0);
                layer->pan = (float) lXml->getDoubleAttribute ("pan", 0.0);
                layer->outputBus = lXml->getIntAttribute ("outputBus", 0);
                layer->rack = std::make_unique<PluginRack>();
                if (auto* rackXml = lXml->getChildByName ("rack"))
                    PluginRack::fromXml (*layer->rack, *rackXml, *formatManager, knownTypes);
                p.layers.add (layer);
            }
            else if (lXml->hasTagName ("variation"))
            {
                Variation v;
                v.name = lXml->getStringAttribute ("name");
                for (auto* oXml : lXml->getChildIterator())
                {
                    if (! oXml->hasTagName ("override")) continue;
                    Variation::Override ov;
                    ov.layerIndex = oXml->getIntAttribute ("layer");
                    ov.gain = (float) oXml->getDoubleAttribute ("gain", 1.0);
                    ov.bypass = oXml->getBoolAttribute ("bypass", false);
                    v.overrides.add (ov);
                }
                p.variations.add (v);
            }
        }
        patches.push_back (std::move (p));
    }
    return true;
}
