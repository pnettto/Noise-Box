# Noise Box: agent playbook

One-stop-shop guitar "boxes": each box is a small AU/VST3 plugin (JUCE 9.0.6, C++17) with a few big
knobs, factory presets, user presets and a hand-drawn UI, built for one style of tone. The user records in
**Ableton Live 12 Suite** on macOS (Apple Silicon) and asks for boxes by style ("make a Fuzz Box").

**This repo is for humans too.** Every box ships with a friendly `boxes/<Box>/README.md` for musicians,
not just code. Write it in plain language: what it sounds like, what each knob does, presets, playing tips.
Use [boxes/JazzBox/README.md](boxes/JazzBox/README.md) as the model.

You cannot hear. **Measure everything** with `tools/measure.py` and say what you measured, not how it
"sounds". The user is the ears: ask them to listen and describe, then translate words into numbers.

## Repo
- `github.com/pnettto/Noise-Box` (**private**, because of JUCE's AGPL), branch `main`. On the user's Mac the
  checkout is `~/Development/pibox/plugins/`; personal test recordings sit one level up (never commit them).
- Commit or push only when the user asks. Permission to commit does not include pushing.
- `gh` often can't authenticate from the sandbox (the token lives in the Keychain). If a push fails, give
  the user the exact commands instead of retrying.

## Layout
```
boxkit/                 shared, header-only: BoxLookAndFeel.h (Theme, knob, wood texture),
                        UserPresetStore.h (save/load/delete user presets). Changes affect EVERY box.
boxes/<Box>/            one plugin per style
  Source/<Box>DSP.h     the signal chain + <Box>Params. Shared by plugin and render tool.
  Source/Presets.h      factory presets (loudness-matched)
  Source/PluginProcessor.*   parameters (APVTS), programs = factory presets, state, user presets
  Source/PluginEditor.*      UI: title plate, preset bar (< menu > SAVE DEL), knobs, output meter
  Tools/Render.cpp      offline renderer: <Box>Render in.wav out.wav preset:N | --presets
  baseline.json         measured preset fingerprints for regression checks
tools/                  setup.sh, new-box.sh, measure.py, requirements.txt
BOXES.md                registry of boxes, plugin codes and sound goals. Keep it current.
CMakeLists.txt          root build; one add_subdirectory per box
install.sh              build + install to ~/Library/Audio/Plug-Ins (all boxes or one)
```
Not in git: `JUCE/`, `.tools/`, `build/` (recreate with `tools/setup.sh`).

## Commands
```bash
tools/setup.sh                                   # fresh clone: venv (cmake, ninja, numpy...) + JUCE
.tools/bin/cmake --build build                   # build everything
.tools/bin/cmake --build build --target FuzzBox_VST3 FuzzBoxRender   # one box
.tools/bin/python tools/measure.py box JazzBox --check   # render all presets, clip/loudness/baseline checks
./install.sh JazzBox                             # install one box (writes outside repo: needs approval)
tools/new-box.sh FuzzBox Fzbx "Fuzz Box"         # scaffold a new box from Jazz Box
```
Use `.tools/bin/...` (or `export PATH="$PWD/.tools/bin:$PATH"`). Homebrew is not usable from the sandbox.

## Adding a box (checklist)
1. **Agree on the sound first.** Ask the user for the style, reference artists or tracks, their guitar
   and amp, and which 4–6 knobs they want. Research real amps and plugins for that style if useful
   (cite sources). Write the sound goals into BOXES.md.
2. `tools/new-box.sh <Target> <Code> "<Display>"`. Code: 4 chars, exactly one upper-case letter, not in BOXES.md.
3. **DSP** (`<Box>DSP.h`): rewrite the chain and `<Box>Params`. Keep `prepare/reset/setParams/process/
   getLatencySamples`. Rules: smooth gain changes (SmoothedValue), only recompute filter coefficients
   when a parameter changes, no allocation in `process`, oversample nonlinear stages (see Warmth),
   keep the input level roughly unchanged at "neutral" settings.
4. **Parameters** (`PluginProcessor.cpp` `createLayout`, `readParams`, `applyParams`, and the ID
   list passed to `UserPresetStore` in the header): IDs are lowercase words and are **permanent once shipped**.
5. **Presets** (`Presets.h`): 8–10 named presets covering the style's classic tones. Then loudness-match:
   run `measure.py box <Box>`, set each preset's output dB so its LUFS matches preset 0 (within 0.5 LU).
6. **UI** (`PluginEditor.cpp`): labels, subtitle text, `boxkit::Theme` colours and background texture.
   Give each box its own look (wood, tolex, brushed metal, painted pedal...). Keep the preset bar and
   meter. If you add a reusable widget or texture, put it in boxkit.
7. **Verify**: build with **zero warnings**, then `measure.py box <Box>` must say OK. Then
   `measure.py box <Box> --save-baseline` and commit baseline.json.
8. **Install** with `./install.sh <Box>` (ask the user before writing to ~/Library). `auval` must pass.
   The user must **restart Live**, or rescan for a brand-new plugin.
9. **Load it in Live** (see below), set a preset, ask the user to play and react, iterate.
10. **Write `boxes/<Box>/README.md`** for humans, following the Jazz Box guide: a one-paragraph pitch, a knob
    table (what it does / try it), always-on features, a preset table with one-line sound descriptions,
    user presets, playing tips, and a short "Under the hood" section. Keep it in sync whenever knobs or presets change.
11. Update BOXES.md (status), add the box to the table in the root README, then summarise. Commit or push only when asked.

## Changing shared code (boxkit)
Before: `measure.py box <Box> --check` for every box. After the change, run it again. It must still pass.
If a sound change is intended, re-save the baselines and say so in the commit message. Visual changes
can't be measured: describe them to the user.

## Ableton Live integration (MCP "ableton" tools)
- Plugins appear in the browser at `plugins/Pibox/<Display Name>`; the URI looks like
  `query:Plugins#VST3:Pibox:Jazz%20Box`. Load with `load_instrument_or_effect`.
- Plugin knobs show up in `get_device_parameters` (index 0 = Device On, then parameters in layout order).
  Values are **normalised 0..1** (e.g. Output ±12 dB: 0.5 = 0 dB). `set_device_parameter` works.
- Factory/user presets can't be selected through the bridge. Set the knob values instead (read them from
  Presets.h or the user's `.jbpreset` file).
- The bridge cannot: move devices, group racks, set track volume/pan/sends/input, or touch the Master.
  Ask the user for those, or work around it (a Utility device for pan/gain).
- Track indices change when the user adds or removes tracks. Always `get_session_info`/`get_track_info` first,
  never assume. Don't modify tracks you didn't create unless asked.
- After reinstalling a plugin, Live keeps running the old binary until it restarts.

## Gotchas learned the hard way
- `juce::dsp::Reverb` scales dry by **2x** and wet by **3x** internally. Divide your levels (see JazzBoxDSP).
  Ignoring it made Jazz Box clip at +8 dB.
- Guitar tracks are often mono. Buses accept mono or stereo input; with mono input `processBlock` copies
  channel 0 to channel 1 before the stereo DSP. Keep that in every box.
- User presets live in `~/Library/Application Support/Pibox/<Display Name>/Presets/*.jbpreset`
  (XML: tag `<Target>Preset`, one attribute per parameter ID). The folder name comes from the display name.
- Programs (factory presets) are fixed at load time. Live caches the list, so don't make it dynamic.
- Plugin code + manufacturer code identify the plugin. Never change `PLUGIN_CODE` of a shipped box.
- JUCE is AGPLv3 unless licensed: keep the GitHub repo **private** unless the user decides otherwise.
- The sandbox asks before writes outside the repo (installing plugins, gh config). If denied, don't
  retry without asking.

## The user's setup and taste
- Guitars: nylon-string classical (with voice), Fender Mustang electric. Mic: AKG C214. Interface:
  Focusrite Scarlett (use INST for the guitar input). Aim for peaks around -12 dBFS.
- Taste so far: intimate, warm, woody, dark, lush, "profesh". Not harsh or airy. They like loudness-matched,
  well-named presets and pretty UIs.
- Reference analysis is welcome: if they provide a reference track, use `measure.py compare`.
- Test recordings (`sample.wav`, `reference.wav`) live outside this repo in `~/Development/pibox/`.
  They are personal: never commit them. Set `NOISEBOX_TEST_WAV` to use one for `measure.py box`.
