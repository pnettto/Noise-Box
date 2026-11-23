#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared visual building blocks for every Noise Box plugin.
namespace boxkit
{
    struct Theme
    {
        juce::Colour text     { 0xfff3e6c8 }; // cream labels
        juce::Colour accent   { 0xffc9a24a }; // brass
        juce::Colour accentDk { 0xff7a5a1c };
        juce::Colour panel    { 0xff1d1410 };
        juce::Colour arc      { 0xffffc86b }; // value arc
    };

    // Procedural wood grain (deterministic for a given seed).
    inline juce::Image makeWoodTexture (int w, int h, int seed = 1955,
                                        juce::Colour light = juce::Colour (0xff5a2e14),
                                        juce::Colour dark  = juce::Colour (0xff3a1c0c))
    {
        juce::Image img (juce::Image::RGB, w, h, true);
        juce::Graphics g (img);
        juce::Random rng (seed);

        g.setGradientFill (juce::ColourGradient (light, 0, 0, dark, (float) w, (float) h, false));
        g.fillAll();

        for (int i = 0; i < 140; ++i)
        {
            const float y0    = rng.nextFloat() * (float) h;
            const float amp   = 2.0f + rng.nextFloat() * 6.0f;
            const float freq  = 0.004f + rng.nextFloat() * 0.01f;
            const float phase = rng.nextFloat() * 6.28f;
            juce::Path p;
            p.startNewSubPath (0, y0);
            for (int x = 0; x <= w; x += 6)
                p.lineTo ((float) x, y0 + amp * std::sin (phase + (float) x * freq));
            const bool isLight = rng.nextBool();
            g.setColour ((isLight ? light.brighter (0.5f) : dark.darker (0.6f))
                             .withAlpha (0.10f + rng.nextFloat() * 0.25f));
            g.strokePath (p, juce::PathStrokeType (0.6f + rng.nextFloat() * 1.6f));
        }
        for (int i = 0; i < 3; ++i)
        {
            const float cx = rng.nextFloat() * (float) w, cy = rng.nextFloat() * (float) h;
            for (int r = 18; r > 2; r -= 3)
            {
                g.setColour (dark.darker (0.4f).withAlpha (0.15f));
                g.drawEllipse (cx - (float) r * 1.8f, cy - (float) r * 0.6f, (float) r * 3.6f, (float) r * 1.2f, 1.0f);
            }
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, 0,
                                                 juce::Colours::transparentWhite, 0, (float) h * 0.6f, false));
        g.fillAll();
        return img;
    }

    // Metal knob with knurled edge, value arc and pointer. Colours come from the Theme.
    class KnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit KnobLookAndFeel (Theme t = {}) : theme (t) {}

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override
        {
            const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);

            juce::Path track, value;
            track.addCentredArc (centre.x, centre.y, radius, radius, 0, startAngle, endAngle, true);
            value.addCentredArc (centre.x, centre.y, radius, radius, 0, startAngle, angle, true);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (theme.arc);
            g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float kr = radius * 0.78f;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (centre.x - kr + 2, centre.y - kr + 4, kr * 2, kr * 2);
            g.setGradientFill (juce::ColourGradient (theme.accent.brighter (0.6f), centre.x - kr * 0.6f, centre.y - kr * 0.8f,
                                                     theme.accentDk, centre.x + kr * 0.6f, centre.y + kr, true));
            g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2, kr * 2);

            g.setColour (theme.accentDk.withAlpha (0.6f));
            for (int i = 0; i < 36; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::twoPi / 36.0f;
                g.drawLine (centre.x + std::cos (a) * kr * 0.88f, centre.y + std::sin (a) * kr * 0.88f,
                            centre.x + std::cos (a) * kr,          centre.y + std::sin (a) * kr, 1.0f);
            }

            const float cr = kr * 0.62f;
            g.setGradientFill (juce::ColourGradient (theme.accent.brighter (0.4f), centre.x, centre.y - cr,
                                                     theme.accent.darker (0.3f), centre.x, centre.y + cr, false));
            g.fillEllipse (centre.x - cr, centre.y - cr, cr * 2, cr * 2);

            juce::Path pointer;
            pointer.addRoundedRectangle (-1.8f, -kr * 0.95f, 3.6f, kr * 0.45f, 1.5f);
            g.setColour (theme.panel);
            g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
        }

        Theme theme;
    };
}
