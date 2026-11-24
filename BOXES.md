# Box registry

Every box is its own plugin. **Plugin codes and parameter IDs are permanent**: changing either breaks
saved Live projects and user presets. Check this table before picking a code; `tools/new-box.sh` does too.
Manufacturer code for all boxes: `Pibx` (company "Pibox").

| Display name | Target | Code | Status | Sound goals |
|---|---|---|---|---|
| Jazz Box | `JazzBox` | `Jzbx` | shipped | Clean jazz amp for a Fender Mustang (neck pickup, tone rolled back). Warm, woody, dark, rounded; never distorts unless Warmth is pushed. Inspired by Sonatura Solid Soul and Polytone-style amps. Knobs: Warmth, Wood, Dark, Squeeze, Room, Output. 10 loudness-matched factory presets. |
| Stoner Box | `StonerBox` | `Stbx` | in progress | Complete stoner/doom rig for a Fender Mustang in drop D: overdrive-to-fuzz knob (chug -> drone) into a sagging amp and cab, Doom (lows saturated in the power amp + cab thump + sub-octave), Wall (rig B mids/highs as a second take, lows identical), auto gate. Mid-forward (target ref: flat 600 Hz-3 kHz). Rigs: Doom, Desert, Wizard. Knobs: Rig, Fuzz, Tone, Mids, Doom, Wall, Space, Output. Replaces Fuzz Box (`Fzbx`, never shipped). |
