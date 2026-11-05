#include "Persistence.h"
#include "PatchManager.h"
#include "RemoteControl.h"

namespace Persistence
{
    bool saveConcert (const PatchManager& pm, const RemoteControl& rc, const juce::File& file)
    {
        juce::XmlElement root ("livekeys");
        root.addChildElement (pm.toXml().createCopy());
        root.addChildElement (rc.toXml().createCopy());
        return root.writeTo (file);
    }

    bool loadConcert (PatchManager& pm, RemoteControl& rc, const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("livekeys"))
            return false;
        bool ok = true;
        if (auto* concert = xml->getChildByName ("concert"))
            ok = pm.fromXml (*concert);
        if (auto* remote = xml->getChildByName ("remote"))
            rc.fromXml (*remote);
        return ok;
    }
}
