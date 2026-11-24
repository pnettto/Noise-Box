# Stoner Box 🌋

**A whole stoner / doom rig in one plugin: fuzz pedal, cranked amp, big cab and a wall of doubled guitars.**
Plug in your Fender Mustang (drop D or E standard), pick a rig and riff. It is voiced mid-forward like
the records (Sleep, Electric Wizard, Kyuss, Belzebong, Red Fang), not scooped and fizzy, and the low end
is built to punch you in the chest instead of turning into mud.

## The knobs
| Knob | What it does | Try it |
|---|---|---|
| **Rig** | **Doom**: Muff-style fuzz into an Orange/Matamp-style head and a closed 4x12. **Desert**: fuzz into an Ampeg-style bass rig (8x10): huge low mids, darker top. **Wizard**: raw germanium fuzz into a cranked Sunn/Laney-style amp, sputters and blooms. | Doom for riffs, Desert for grooves, Wizard for filth |
| **Fuzz** | One knob from chug to drone. **0–50%**: an overdrive into the amp: tight, keeps your pick attack, palm mutes chug. **50–100%**: the fuzz layers in on top for sustain and drone. The amp sags when you dig in and blooms back as notes ring. | 40–50% chug, 80–95% drone |
| **Tone** | Dark ↔ bright tilt; also opens the cab's top end. | 35–50% |
| **Mids** | The only mid control. Left: classic Muff scoop (-8 dB). Noon: flat. Right: push (+5 dB) to cut through a band. | 50–60% |
| **Doom** | The weight. Pushes the lows *into* the power amp so they saturate and pump with the riff, raises the cab thump around 90 Hz, slows the amp's bloom, and above 70% adds a smooth sub-octave. | 50–80%, 100% for ritual sub |
| **Wall** | Mono → a second, slightly different rig on the right, played a hair later like a double-tracked take. Only the mids and highs spread; the low end stays identical and centred, so turning it up never thins the sound. | 50–80% |
| **Space** | Dark room around the wall; the low end stays dry. | 10–25% |
| **Output** | Final volume, ±12 dB. A soft limiter keeps peaks under 0 dBFS. | Match bypass |

**Always on:** an automatic noise gate (keyed from your clean signal, so riffs stop dead without chopping
tails) and a glue compressor.

## Presets
All factory presets are loudness-matched on a real DI take.

| Preset | Sound |
|---|---|
| **Dopesmoker** | The default: thick Muff + Orange wall, mid-forward, wide |
| **Holy Mountain** | More fuzz, more weight, wider |
| **Sky Valley** | Desert rig, punchy bass-amp mids, half overdrive half fuzz |
| **Desert Crawl** | Desert rig, heavy and low |
| **Wizard's Wall** | Germanium + cranked amp, very wide |
| **Funeral Fog** | Max germanium, dark, roomy |
| **Sub Ritual** | Doom at 100%: sub-octave under the riff |
| **Muff Scoop** | The classic scooped Muff sound, for contrast (thinner on purpose) |
| **Riff Lord Chug** | Overdrive only: tight, chugging desert riffs |
| **Smoky Edge** | Light overdrive edge, Wizard rig |

User presets: **SAVE / DEL / ◀ ▶**, stored in `~/Library/Application Support/Pibox/Stoner Box/Presets/`.

## Playing tips
- Scarlett **INST** on, peaks around **-12 dBFS**. Your test take peaked at 0 dBFS, which is too hot.
- Bridge pickup for riffs, neck for fat leads. Let open low D ring: Doom and the amp sag reward sustain.
- For a real quad-track, record the riff twice and put a Stoner Box with Wall on each take.

## Under the hood
Per channel, 8x oversampled: tighten → overdrive (pre-filtered, soft asymmetric clip) → fuzz layered on
top (bias shift on Wizard, Muff-style tone blend) → triode preamp (no low cut, so your 70–80 Hz notes
survive) → power amp with sag and the Doom low boost inside the clipping. Then auto gate, rig voicing
(low-mid dip), Mids, Tone, cab model (gentle 45 Hz low cut, ~90 Hz thump, cone presence, steep top),
Doom low shelf, sub-octave, Wall (rig B above a 160 Hz crossover, 12.5 ms drifting delay aligned to the
crossover; lows from rig A on both sides), Space (wet-only, 280 Hz–3.5 kHz), glue comp, Output, soft limiter.

Measured on a real drop-D DI take against a reference of stoner records: 63–127 Hz and 320–1280 Hz within
about 1 dB of the reference on average, Wall at 100% loses under 1 dB in mono. Plugin code `Stbx`,
AU + VST3, macOS (Apple Silicon).
