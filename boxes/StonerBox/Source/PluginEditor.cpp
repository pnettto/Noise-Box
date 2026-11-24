#include "PluginEditor.h"

namespace
{
    const juce::Colour bone   { 0xffd9d1c1 };
    const juce::Colour rust   { 0xffd9661f };
    const juce::Colour rustDk { 0xff5a2410 };
    const juce::Colour panel  { 0xff0d0c0b };

    boxkit::Theme fuzzTheme()
    {
        boxkit::Theme t;
        t.text = bone; t.accent = juce::Colour (0xff3a3632); t.accentDk = juce::Colour (0xff121110);
        t.panel = bone; t.arc = rust;
        return t;
    }

    constexpr int perRow = 4; // RIG FUZZ TONE MIDS / DOOM WALL SPACE OUTPUT
}

StonerBoxEditor::StonerBoxEditor (StonerBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), lnf (fuzzTheme())
{
    const char* ids[]   = { "rig", "fuzz", "tone", "mids", "doom", "wall", "space", "output" };
    const char* names[] = { "RIG", "FUZZ", "TONE", "MIDS", "DOOM", "WALL", "SPACE", "OUTPUT" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k.slider.setColour (juce::Slider::textBoxTextColourId, bone.withAlpha (0.85f));
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.35f));
        k.slider.setLookAndFeel (&lnf);
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions ("Courier New", 15.0f, juce::Font::bold));
        k.label.setColour (juce::Label::textColourId, bone);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
    }

    presetBox.setColour (juce::ComboBox::backgroundColourId, panel.withAlpha (0.9f));
    presetBox.setColour (juce::ComboBox::textColourId, bone);
    presetBox.setColour (juce::ComboBox::outlineColourId, rust.withAlpha (0.7f));
    presetBox.setColour (juce::ComboBox::arrowColourId, rust);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setTextWhenNothingSelected ("- custom -");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= userIdBase)
        {
            const int u = id - userIdBase;
            if (juce::isPositiveAndBelow (u, userPresetFiles.size()))
                proc.loadUserPreset (userPresetFiles[u]);
        }
        else if (id > 0)
        {
            proc.setCurrentProgram (id - 1);
        }
        syncPresetSelection();
    };
    addAndMakeVisible (presetBox);
    rebuildPresetMenu();

    saveButton.onClick   = [this] { showSaveDialog(); };
    deleteButton.onClick = [this] { confirmDelete(); };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton })
    {
        b->setColour (juce::TextButton::buttonColourId, panel.withAlpha (0.9f));
        b->setColour (juce::TextButton::textColourOffId, rust.brighter (0.2f));
        addAndMakeVisible (*b);
    }
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };

    setSize (820, 440);
    woodTexture = boxkit::makeGritTexture (820, 440, 420, juce::Colour (0xff17121a));
    startTimerHz (30);
}

StonerBoxEditor::~StonerBoxEditor()
{
    for (auto& k : knobs)
        k.slider.setLookAndFeel (nullptr);
}

void StonerBoxEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (woodTexture, 0, 0);

    // title: stencil-ish, no plate
    g.setColour (bone);
    g.setFont (juce::FontOptions ("Impact", 34.0f, juce::Font::plain));
    g.drawText ("STONER BOX", 26, 12, 260, 44, juce::Justification::centredLeft);
    g.setColour (rust);
    g.setFont (juce::FontOptions ("Courier New", 11.0f, juce::Font::bold));
    g.drawText ("FUZZ - AMP - CAB - WALL OF DOOM", 28, 50, 260, 14, juce::Justification::centredLeft);

    // thin rust rules framing the two rows
    g.setColour (rust.withAlpha (0.55f));
    g.fillRect (24, 74, getWidth() - 48, 2);
    g.setColour (rustDk.withAlpha (0.8f));
    g.fillRect (24, 258, getWidth() - 48, 1);

    // output meter
    auto m = meterArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRect (m);
    const float db = juce::Decibels::gainToDecibels (meterLevel, -60.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = m.reduced (2.0f);
    fill = fill.removeFromLeft (fill.getWidth() * norm);
    g.setGradientFill (juce::ColourGradient (rustDk, m.getX(), 0, juce::Colour (0xffe0603a), m.getRight(), 0, false));
    g.fillRect (fill);
    g.setColour (bone.withAlpha (0.7f));
    g.setFont (juce::FontOptions ("Courier New", 11.0f, juce::Font::bold));
    g.drawText ("OUT", meterArea.withX (meterArea.getX() - 34).withWidth (30), juce::Justification::centredRight);
}

void StonerBoxEditor::resized()
{
    meterArea = juce::Rectangle<int> (getWidth() - 130, 26, 106, 12);
    prevButton.setBounds (300, 22, 26, 26);
    presetBox.setBounds (330, 22, 170, 26);
    nextButton.setBounds (504, 22, 26, 26);
    saveButton.setBounds (538, 22, 48, 26);
    deleteButton.setBounds (590, 22, 42, 26);

    for (int row = 0; row < 2; ++row)
    {
        auto area = juce::Rectangle<int> (60, 82 + row * 178, getWidth() - 120, 170);
        const int w = area.getWidth() / perRow;
        for (int i = 0; i < perRow; ++i)
        {
            auto& k = knobs[(size_t) (row * perRow + i)];
            auto col = area.removeFromLeft (w);
            k.label.setBounds (col.removeFromTop (22));
            k.slider.setBounds (col.reduced (14, 0));
        }
    }
}

void StonerBoxEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);

    presetBox.addSectionHeading ("Factory");
    const auto& presets = getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i)
        presetBox.addItem (presets[i].name, (int) i + 1);

    userPresetFiles = proc.getUserPresetFiles();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("My Presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }

    shownProgram = -2; // force a resync
    syncPresetSelection();
}

void StonerBoxEditor::syncPresetSelection()
{
    const int program = proc.getCurrentProgram();
    const auto user = proc.getUserPresetName();
    if (program == shownProgram && user == shownUserPreset)
        return;

    shownProgram = program;
    shownUserPreset = user;

    int id = 0;
    if (program >= 0)
        id = program + 1;
    else
        for (int i = 0; i < userPresetFiles.size(); ++i)
            if (userPresetFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (user))
                id = userIdBase + i;

    presetBox.setSelectedId (id, juce::dontSendNotification);
    deleteButton.setEnabled (id >= userIdBase);
}

void StonerBoxEditor::stepPreset (int delta)
{
    juce::Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        ids.add (presetBox.getItemId (i));
    if (ids.isEmpty())
        return;

    const int current = juce::jmax (0, ids.indexOf (presetBox.getSelectedId()));
    const int n = ids.size();
    presetBox.setSelectedId (ids[((current + delta) % n + n) % n], juce::sendNotificationSync);
}

void StonerBoxEditor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name your sound:", juce::MessageBoxIconType::NoIcon, this);
    const auto suggestion = proc.getUserPresetName().isNotEmpty() ? proc.getUserPresetName() : juce::String ("My Riff Tone");
    saveDialog->addTextEditor ("name", suggestion);
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        const auto name = saveDialog != nullptr ? saveDialog->getTextEditorContents ("name") : juce::String();
        saveDialog.reset();
        if (result == 1 && proc.saveUserPreset (name))
            rebuildPresetMenu();
    }), false);
}

void StonerBoxEditor::confirmDelete()
{
    const auto name = proc.getUserPresetName();
    if (name.isEmpty())
        return;

    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::WarningIcon)
                       .withTitle ("Delete Preset")
                       .withMessage ("Delete \"" + name + "\"?")
                       .withButton ("Delete", 1)
                       .withButton ("Cancel", 0)
                       .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (options, [safe = juce::Component::SafePointer<StonerBoxEditor> (this), name] (int result)
    {
        if (safe != nullptr && result == 1 && safe->proc.deleteUserPreset (name))
            safe->rebuildPresetMenu();
    });
}

void StonerBoxEditor::timerCallback()
{
    // follow preset changes made by the host (e.g. Live's preset menu) or state restore
    syncPresetSelection();

    const float level = proc.outputLevel.exchange (0.0f);
    meterLevel = juce::jmax (level, meterLevel * 0.85f);
    repaint (meterArea.expanded (40, 4));
}
