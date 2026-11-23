#include "PluginProcessor.h"
#include "PluginEditor.h"

JazzBoxProcessor::JazzBoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "JazzBox", createLayout())
{
    pWarmth  = apvts.getRawParameterValue ("warmth");
    pWood    = apvts.getRawParameterValue ("wood");
    pDark    = apvts.getRawParameterValue ("dark");
    pSqueeze = apvts.getRawParameterValue ("squeeze");
    pRoom    = apvts.getRawParameterValue ("room");
    pOutput  = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout JazzBoxProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<P> (ID { "warmth",  1 }, "Warmth",  juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    layout.add (std::make_unique<P> (ID { "wood",    1 }, "Wood",    juce::NormalisableRange<float> (0.0f, 1.0f), 0.50f, pct));
    layout.add (std::make_unique<P> (ID { "dark",    1 }, "Dark",    juce::NormalisableRange<float> (0.0f, 1.0f), 0.50f, pct));
    layout.add (std::make_unique<P> (ID { "squeeze", 1 }, "Squeeze", juce::NormalisableRange<float> (0.0f, 1.0f), 0.40f, pct));
    layout.add (std::make_unique<P> (ID { "room",    1 }, "Room",    juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    layout.add (std::make_unique<P> (ID { "output",  1 }, "Output",  juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

JazzBoxParams JazzBoxProcessor::readParams() const
{
    JazzBoxParams p;
    p.warmth   = pWarmth->load();
    p.wood     = pWood->load();
    p.dark     = pDark->load();
    p.squeeze  = pSqueeze->load();
    p.room     = pRoom->load();
    p.outputDb = pOutput->load();
    return p;
}

void JazzBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.setParams (readParams());
    dsp.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (dsp.getLatencySamples());
}

bool JazzBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void JazzBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    if (numIn == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples()); // mono guitar -> both sides
    for (int ch = juce::jmax (numIn, 2); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    dsp.setParams (readParams());
    dsp.process (buffer);

    const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
    const float prev = outputLevel.load();
    outputLevel.store (juce::jmax (peak, prev * 0.9f));
}

void JazzBoxProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram.store (index);
    applyParams (presets[(size_t) index].params);

    // A factory preset replaces any user preset selection.
    auto clearUser = [safe = juce::WeakReference<JazzBoxProcessor> (this), this]
    {
        if (safe != nullptr)
            userPresets.clearCurrent();
    };
    if (juce::MessageManager::existsAndIsCurrentThread())
        clearUser();
    else
        juce::MessageManager::callAsync (clearUser);
}

void JazzBoxProcessor::applyParams (const JazzBoxParams& p)
{
    const std::pair<const char*, float> values[] = {
        { "warmth", p.warmth }, { "wood", p.wood }, { "dark", p.dark },
        { "squeeze", p.squeeze }, { "room", p.room }, { "output", p.outputDb } };

    for (const auto& [id, v] : values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

bool JazzBoxProcessor::saveUserPreset (const juce::String& name)
{
    if (! userPresets.save (name))
        return false;
    currentProgram.store (-1);
    return true;
}

bool JazzBoxProcessor::loadUserPreset (const juce::File& file)
{
    if (! userPresets.load (file))
        return false;
    currentProgram.store (-1);
    return true;
}

const juce::String JazzBoxProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void JazzBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("program", currentProgram.load(), nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void JazzBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (getUserPresetName().isNotEmpty())
                currentProgram.store (-1);
            else
                currentProgram.store ((int) apvts.state.getProperty ("program", 0));
        }
}

juce::AudioProcessorEditor* JazzBoxProcessor::createEditor() { return new JazzBoxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JazzBoxProcessor(); }
