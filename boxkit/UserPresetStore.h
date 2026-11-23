#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Generic user-preset storage shared by every Noise Box plugin.
// Presets are XML files: <TAG name="..." version="1" paramId="value" .../>
// stored in ~/Library/Application Support/Pibox/<Box Name>/Presets/*.jbpreset
// Call from the message thread only.
namespace boxkit
{
    class UserPresetStore
    {
    public:
        UserPresetStore (juce::AudioProcessorValueTreeState& stateToUse, juce::String boxName,
                         juce::String xmlTag, juce::StringArray parameterIds)
            : state (stateToUse), box (std::move (boxName)), tag (std::move (xmlTag)), ids (std::move (parameterIds)) {}

        juce::File getDirectory() const
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("Application Support/Pibox").getChildFile (box).getChildFile ("Presets");
        }

        juce::Array<juce::File> getFiles() const
        {
            auto files = getDirectory().findChildFiles (juce::File::findFiles, false, "*.jbpreset");
            files.sort();
            return files;
        }

        juce::String getCurrentName() const { return state.state.getProperty ("userPreset").toString(); }
        void clearCurrent() { state.state.setProperty ("userPreset", "", nullptr); }

        bool save (const juce::String& rawName)
        {
            const auto name = rawName.trim();
            if (name.isEmpty())
                return false;

            juce::XmlElement xml (tag);
            xml.setAttribute ("name", name);
            xml.setAttribute ("version", 1);
            for (const auto& id : ids)
                if (auto* v = state.getRawParameterValue (id))
                    xml.setAttribute (id, v->load());

            auto dir = getDirectory();
            if (! dir.createDirectory())
                return false;
            if (! xml.writeTo (dir.getChildFile (juce::File::createLegalFileName (name) + ".jbpreset")))
                return false;

            state.state.setProperty ("userPreset", name, nullptr);
            return true;
        }

        bool load (const juce::File& file)
        {
            const auto xml = juce::XmlDocument::parse (file);
            if (xml == nullptr || ! xml->hasTagName (tag))
                return false;

            for (const auto& id : ids)
                if (xml->hasAttribute (id))
                    if (auto* p = state.getParameter (id))
                        p->setValueNotifyingHost (p->convertTo0to1 ((float) xml->getDoubleAttribute (id)));

            state.state.setProperty ("userPreset", xml->getStringAttribute ("name", file.getFileNameWithoutExtension()), nullptr);
            return true;
        }

        bool remove (const juce::String& name)
        {
            for (const auto& f : getFiles())
            {
                const auto xml = juce::XmlDocument::parse (f);
                const auto n = xml != nullptr ? xml->getStringAttribute ("name", f.getFileNameWithoutExtension())
                                              : f.getFileNameWithoutExtension();
                if (n == name)
                {
                    if (getCurrentName() == name)
                        clearCurrent();
                    return f.deleteFile();
                }
            }
            return false;
        }

    private:
        juce::AudioProcessorValueTreeState& state;
        juce::String box, tag;
        juce::StringArray ids;
    };
}
