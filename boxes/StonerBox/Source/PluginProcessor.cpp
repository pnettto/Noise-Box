#include "PluginProcessor.h"
#include "PluginEditor.h"

StonerBoxProcessor::StonerBoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StonerBox", createLayout())
{
    pRig    = apvts.getRawParameterValue ("rig");
    pFuzz   = apvts.getRawParameterValue ("fuzz");
    pTone   = apvts.getRawParameterValue ("tone");
    pMids   = apvts.getRawParameterValue ("mids");
    pDoom   = apvts.getRawParameterValue ("doom");
    pWall   = apvts.getRawParameterValue ("wall");
    pSpace  = apvts.getRawParameterValue ("space");
    pOutput = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout StonerBoxProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const StonerBoxParams d;
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "rig", 1 }, "Rig",
                                                              juce::StringArray { "Doom", "Desert", "Wizard" }, d.rig));
    const std::tuple<const char*, const char*, float> pcts[] = {
        { "fuzz", "Fuzz", d.fuzz }, { "tone", "Tone", d.tone }, { "mids", "Mids", d.mids },
        { "doom", "Doom", d.doom }, { "wall", "Wall", d.wall }, { "space", "Space", d.space } };
    for (const auto& [id, name, def] : pcts)
        layout.add (std::make_unique<P> (ID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def, pct));
    layout.add (std::make_unique<P> (ID { "output", 1 }, "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

StonerBoxParams StonerBoxProcessor::readParams() const
{
    StonerBoxParams p;
    p.rig      = juce::roundToInt (pRig->load());
    p.fuzz     = pFuzz->load();
    p.tone     = pTone->load();
    p.mids     = pMids->load();
    p.doom     = pDoom->load();
    p.wall     = pWall->load();
    p.space    = pSpace->load();
    p.outputDb = pOutput->load();
    return p;
}

void StonerBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.setParams (readParams());
    dsp.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (dsp.getLatencySamples());
}

bool StonerBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void StonerBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

void StonerBoxProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram.store (index);
    applyParams (presets[(size_t) index].params);

    // A factory preset replaces any user preset selection.
    auto clearUser = [safe = juce::WeakReference<StonerBoxProcessor> (this), this]
    {
        if (safe != nullptr)
            userPresets.clearCurrent();
    };
    if (juce::MessageManager::existsAndIsCurrentThread())
        clearUser();
    else
        juce::MessageManager::callAsync (clearUser);
}

void StonerBoxProcessor::applyParams (const StonerBoxParams& p)
{
    const std::pair<const char*, float> values[] = {
        { "rig", (float) p.rig }, { "fuzz", p.fuzz }, { "tone", p.tone }, { "mids", p.mids },
        { "doom", p.doom }, { "wall", p.wall }, { "space", p.space }, { "output", p.outputDb } };

    for (const auto& [id, v] : values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

bool StonerBoxProcessor::saveUserPreset (const juce::String& name)
{
    if (! userPresets.save (name))
        return false;
    currentProgram.store (-1);
    return true;
}

bool StonerBoxProcessor::loadUserPreset (const juce::File& file)
{
    if (! userPresets.load (file))
        return false;
    currentProgram.store (-1);
    return true;
}

const juce::String StonerBoxProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void StonerBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("program", currentProgram.load(), nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StonerBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::AudioProcessorEditor* StonerBoxProcessor::createEditor() { return new StonerBoxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StonerBoxProcessor(); }
