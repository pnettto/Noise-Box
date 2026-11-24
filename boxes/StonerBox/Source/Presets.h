#pragma once

#include "StonerBoxDSP.h"

// Factory presets. Fuzz: 0..50% = overdrive only (chug), 50..100% = fuzz layered on top. Values: rig (0 Doom, 1 Desert, 2 Wizard), fuzz, tone, mids, doom, wall, space (0..1),
// output dB. Output values are loudness-matched to preset 0 on a real DI take.
struct StonerBoxPreset
{
    const char* name;
    StonerBoxParams params;
};

inline const std::array<StonerBoxPreset, 10>& getFactoryPresets()
{
    static const std::array<StonerBoxPreset, 10> presets {{
        //                      rig  fuzz   tone   mids   doom   wall   space  out
        { "Dopesmoker",       { 0, 0.80f, 0.50f, 0.55f, 0.60f, 0.60f, 0.15f, 0.0f } },
        { "Holy Mountain",    { 0, 0.92f, 0.45f, 0.50f, 0.75f, 0.70f, 0.20f, -0.4f } },
        { "Sky Valley",       { 1, 0.70f, 0.55f, 0.65f, 0.55f, 0.55f, 0.10f, 0.5f } },
        { "Desert Crawl",     { 1, 0.88f, 0.45f, 0.55f, 0.80f, 0.65f, 0.15f, -0.3f } },
        { "Wizard's Wall",    { 2, 0.85f, 0.50f, 0.55f, 0.65f, 0.80f, 0.25f, -0.3f } },
        { "Funeral Fog",      { 2, 1.00f, 0.38f, 0.50f, 0.80f, 0.60f, 0.40f, -0.5f } },
        { "Sub Ritual",       { 0, 0.85f, 0.42f, 0.50f, 1.00f, 0.50f, 0.15f, -0.9f } },
        { "Muff Scoop",       { 0, 0.90f, 0.55f, 0.15f, 0.65f, 0.60f, 0.15f, 0.1f } },
        { "Riff Lord Chug",   { 1, 0.45f, 0.55f, 0.60f, 0.60f, 0.45f, 0.10f, 0.5f } },
        { "Smoky Edge",       { 2, 0.30f, 0.50f, 0.55f, 0.45f, 0.35f, 0.25f, 0.8f } },
    }};
    return presets;
}
