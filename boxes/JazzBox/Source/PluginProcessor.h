#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "JazzBoxDSP.h"
#include "Presets.h"
#include <boxkit/UserPresetStore.h>

class JazzBoxProcessor final : public juce::AudioProcessor
{
public:
    JazzBoxProcessor();
    ~JazzBoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return (int) getFactoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // User presets (message thread only)
    juce::Array<juce::File> getUserPresetFiles() const { return userPresets.getFiles(); }
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    bool deleteUserPreset (const juce::String& name) { return userPresets.remove (name); }
    juce::String getUserPresetName() const { return userPresets.getCurrentName(); }
    std::atomic<float> outputLevel { 0.0f }; // peak, linear, read by the editor meter

private:
    JazzBoxParams readParams() const;
    void applyParams (const JazzBoxParams&);

    boxkit::UserPresetStore userPresets { apvts, "Jazz Box", "JazzBoxPreset",
                                          { "warmth", "wood", "dark", "squeeze", "room", "output" } };
    JazzBoxDSP dsp;
    std::atomic<int> currentProgram { 0 };
    std::atomic<float>* pWarmth  = nullptr;
    std::atomic<float>* pWood    = nullptr;
    std::atomic<float>* pDark    = nullptr;
    std::atomic<float>* pSqueeze = nullptr;
    std::atomic<float>* pRoom    = nullptr;
    std::atomic<float>* pOutput  = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE (JazzBoxProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JazzBoxProcessor)
};
