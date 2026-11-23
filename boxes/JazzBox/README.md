# Jazz Box 🎷

**A warm, woody, clean jazz amp in a box.** Plug in an electric guitar (it was voiced on a Fender
Mustang, neck pickup, tone knob around 4) and get the round, dark, late-night tone of a classic jazz
club amp: no fiddling with amp, cabinet, EQ and reverb separately.

Inspired by clean jazz amp simulators like Sonatura's *Solid Soul* and the Polytone-style approach
loved by jazz players: amplify the guitar as cleanly as possible, then shape it with the cabinet, the mic and the room.

## The knobs

| Knob | What it does | Try it |
|---|---|---|
| **Warmth** | Clean tube-style saturation. Low settings just round off your peaks; high settings add soft, even-toned grit. Quiet playing stays about the same volume. | 20–40% for clean jazz, 70%+ for a pushed-tube feel |
| **Wood** | A broad boost at 200 Hz (0 to +7 dB): the hollow, woody body of an archtop. | Turn up for thin guitars, down if it gets boomy |
| **Dark** | Like rolling back your tone knob or using a ribbon mic: cuts the highs from 9 kHz down to 2.5 kHz. | 50–70% is classic jazz; lower it for bebop clarity |
| **Squeeze** | Gentle, soft compression with automatic gain to make up for it. Evens out your picking and adds sustain. | 40–60% for smooth lines; lower it to keep dynamics |
| **Room** | A small studio room behind your dry signal. More = a bigger room. | 15% close and intimate, 50%+ for ballads |
| **Output** | Final volume, ±12 dB. | Match the bypassed level |

**Always working quietly in the background:** a low cut at 80 Hz removes rumble, a gentle lift at 700 Hz
helps notes speak, and a small dip at 2.5 kHz softens pick click. A mono guitar comes out on both sides.

## Presets

All factory presets play at the same loudness, so you compare tone, not volume.

| Preset | Sound |
|---|---|
| **Init** | Balanced starting point |
| **Polytone Club** | The classic clean jazz club amp: woody and even |
| **Thumb Octaves** | Warm and round, for thumb-picked octave lines |
| **Whisper Archtop** | Very clean and quiet, dark, barely compressed, almost no room |
| **Quiet Storm** | Smooth jazz: brighter, silkier, more compressed, airy room |
| **Lush Room** | Big, spacious reverb for ballads |
| **Bossa Muted** | Tight, compressed and dry, for muted bossa comping |
| **Late Night Mahogany** | The darkest and woodiest, with saturation |
| **Bright Bebop** | Open and articulate, for fast lines |
| **Warm Tube Push** | Heavier saturation, like a tube amp pushed a little |

**Your own presets:** dial in a sound, click **SAVE** and name it. Your presets appear under *My Presets*
in the menu; **DEL** removes the selected one, and ◀ ▶ steps through everything. The selected preset is saved
with your Live project. Files live in `~/Library/Application Support/Pibox/Jazz Box/Presets/` (one
`.jbpreset` per sound), so you can back them up or share them.

## Playing tips
- Neck pickup, tone knob rolled back: this matters more than any knob here.
- Mute lightly with the edge of your picking hand and use your thumb for that Wes-style softness.
- On a Scarlett interface, press **INST** for the guitar input and aim for peaks around -12 dBFS.
- Flatwound strings are the single biggest upgrade for this sound.

## Under the hood
Signal flow: **Squeeze → Warmth → cab/mic EQ (80 Hz cut, Wood, 700 Hz, 2.5 kHz dip, Dark) → Room → Output.**
Plugin code `Jzbx`, AU + VST3, macOS (Apple Silicon). Details for developers are in
[`Source/JazzBoxDSP.h`](Source/JazzBoxDSP.h) and the repo's [AGENTS.md](../../AGENTS.md).
