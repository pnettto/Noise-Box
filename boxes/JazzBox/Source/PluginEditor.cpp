#include "PluginEditor.h"

namespace
{
    const juce::Colour cream   { 0xfff3e6c8 };
    const juce::Colour brass   { 0xffc9a24a };
    const juce::Colour brassDk { 0xff7a5a1c };
    const juce::Colour panel   { 0xff1d1410 };

}

JazzBoxEditor::JazzBoxEditor (JazzBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    const char* ids[]   = { "warmth", "wood", "dark", "squeeze", "room", "output" };
    const char* names[] = { "WARMTH", "WOOD", "DARK", "SQUEEZE", "ROOM", "OUTPUT" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        k.slider.setColour (juce::Slider::textBoxTextColourId, cream);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.25f));
        k.slider.setLookAndFeel (&lnf);
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        k.label.setColour (juce::Label::textColourId, cream);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
    }

    // Preset browser
    presetBox.setColour (juce::ComboBox::backgroundColourId, panel.withAlpha (0.85f));
    presetBox.setColour (juce::ComboBox::textColourId, cream);
    presetBox.setColour (juce::ComboBox::outlineColourId, brass.withAlpha (0.8f));
    presetBox.setColour (juce::ComboBox::arrowColourId, brass);
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
        b->setColour (juce::TextButton::buttonColourId, panel.withAlpha (0.85f));
        b->setColour (juce::TextButton::textColourOffId, brass);
        addAndMakeVisible (*b);
    }
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };

    setSize (780, 330);
    woodTexture = boxkit::makeWoodTexture (780, 330);
    startTimerHz (30);
}

JazzBoxEditor::~JazzBoxEditor()
{
    for (auto& k : knobs)
        k.slider.setLookAndFeel (nullptr);
}

void JazzBoxEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (woodTexture, 0, 0);

    // inset control panel
    auto inner = getLocalBounds().toFloat().reduced (18.0f).withTrimmedTop (64.0f);
    g.setColour (panel.withAlpha (0.82f));
    g.fillRoundedRectangle (inner, 10.0f);
    g.setColour (brass.withAlpha (0.7f));
    g.drawRoundedRectangle (inner, 10.0f, 1.5f);

    // title plate
    auto plate = juce::Rectangle<float> (18.0f, 14.0f, 248.0f, 42.0f);
    g.setGradientFill (juce::ColourGradient (brass.brighter (0.3f), plate.getX(), plate.getY(),
                                             brassDk, plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 5.0f);
    g.setColour (panel);
    g.setFont (juce::FontOptions ("Georgia", 28.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("Jazz Box", plate.reduced (14.0f, 0).toNearestInt(), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("CLEAN JAZZ AMP", plate.reduced (12.0f, 0).toNearestInt(), juce::Justification::centredRight);

    // screws
    for (auto pt : { juce::Point<float> (10, 10), juce::Point<float> ((float) getWidth() - 10, 10),
                     juce::Point<float> (10, (float) getHeight() - 10), juce::Point<float> ((float) getWidth() - 10, (float) getHeight() - 10) })
    {
        g.setColour (brass.darker (0.2f));
        g.fillEllipse (pt.x - 4, pt.y - 4, 8, 8);
        g.setColour (brassDk);
        g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.2f);
    }

    // output meter
    auto m = meterArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (m, 4.0f);
    const float db = juce::Decibels::gainToDecibels (meterLevel, -60.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = m.reduced (3.0f);
    fill = fill.removeFromLeft (fill.getWidth() * norm);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6fae5a), m.getX(), 0,
                                             juce::Colour (0xffe0503a), m.getRight(), 0, false));
    g.fillRoundedRectangle (fill, 3.0f);
    g.setColour (cream.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("OUT", meterArea.withX (meterArea.getX() - 34).withWidth (30), juce::Justification::centredRight);
}

void JazzBoxEditor::resized()
{
    auto area = getLocalBounds().reduced (30).withTrimmedTop (70);
    meterArea = juce::Rectangle<int> (getWidth() - 130, 26, 106, 14);
    prevButton.setBounds (280, 20, 26, 26);
    presetBox.setBounds (310, 20, 160, 26);
    nextButton.setBounds (474, 20, 26, 26);
    saveButton.setBounds (508, 20, 48, 26);
    deleteButton.setBounds (560, 20, 42, 26);
    const int w = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (w);
        k.label.setBounds (col.removeFromTop (22));
        k.slider.setBounds (col.reduced (4));
    }
}

void JazzBoxEditor::rebuildPresetMenu()
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

void JazzBoxEditor::syncPresetSelection()
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

void JazzBoxEditor::stepPreset (int delta)
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

void JazzBoxEditor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name your sound:", juce::MessageBoxIconType::NoIcon, this);
    const auto suggestion = proc.getUserPresetName().isNotEmpty() ? proc.getUserPresetName() : juce::String ("My Jazz Tone");
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

void JazzBoxEditor::confirmDelete()
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

    juce::AlertWindow::showAsync (options, [safe = juce::Component::SafePointer<JazzBoxEditor> (this), name] (int result)
    {
        if (safe != nullptr && result == 1 && safe->proc.deleteUserPreset (name))
            safe->rebuildPresetMenu();
    });
}

void JazzBoxEditor::timerCallback()
{
    // follow preset changes made by the host (e.g. Live's preset menu) or state restore
    syncPresetSelection();

    const float level = proc.outputLevel.exchange (0.0f);
    meterLevel = juce::jmax (level, meterLevel * 0.85f);
    repaint (meterArea.expanded (40, 4));
}
