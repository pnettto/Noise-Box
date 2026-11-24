#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <boxkit/BoxLookAndFeel.h>

class StonerBoxEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StonerBoxEditor (StonerBoxProcessor&);
    ~StonerBoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    StonerBoxProcessor& proc;
    boxkit::KnobLookAndFeel lnf;
    std::array<Knob, 8> knobs;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TextButton saveButton { "SAVE" }, deleteButton { "DEL" };
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::Array<juce::File> userPresetFiles;
    int shownProgram = -2;
    juce::String shownUserPreset;

    static constexpr int userIdBase = 1000;
    void rebuildPresetMenu();
    void syncPresetSelection();
    void stepPreset (int delta);
    void showSaveDialog();
    void confirmDelete();
    juce::Image woodTexture;
    float meterLevel = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StonerBoxEditor)
};
