#pragma once

#include "JazzBoxDSP.h"

// Factory presets. Values: warmth, wood, dark, squeeze, room (0..1), output dB.
// Output values are loudness-matched to Init (about -14.5 LUFS on the test take),
// so switching presets compares tone, not volume.
struct JazzBoxPreset
{
    const char* name;
    JazzBoxParams params;
};

inline const std::array<JazzBoxPreset, 10>& getFactoryPresets()
{
    static const std::array<JazzBoxPreset, 10> presets {{
        { "Init",                { 0.35f, 0.50f, 0.50f, 0.40f, 0.25f,  0.0f } },
        { "Polytone Club",       { 0.25f, 0.60f, 0.50f, 0.45f, 0.25f, -0.7f } },
        { "Thumb Octaves",       { 0.35f, 0.70f, 0.65f, 0.50f, 0.20f, -0.9f } },
        { "Whisper Archtop",     { 0.15f, 0.55f, 0.70f, 0.30f, 0.12f, -0.9f } },
        { "Quiet Storm",         { 0.30f, 0.45f, 0.35f, 0.55f, 0.40f,  1.0f } },
        { "Lush Room",           { 0.30f, 0.50f, 0.45f, 0.40f, 0.70f,  0.8f } },
        { "Bossa Muted",         { 0.20f, 0.40f, 0.60f, 0.60f, 0.15f,  0.9f } },
        { "Late Night Mahogany", { 0.55f, 0.85f, 0.75f, 0.50f, 0.35f, -1.1f } },
        { "Bright Bebop",        { 0.35f, 0.35f, 0.20f, 0.40f, 0.20f,  0.8f } },
        { "Warm Tube Push",      { 0.80f, 0.60f, 0.55f, 0.35f, 0.25f,  0.1f } },
    }};
    return presets;
}
