# Noise Box

One-stop-shop guitar "box" plugins (AU/VST3, JUCE) for Ableton Live: a few big knobs, factory and user
presets, and a hand-drawn UI per style.

| Box | Status |
|---|---|
| Jazz Box | shipped |

See [BOXES.md](BOXES.md) for the registry and sound goals.

## Quick start
```bash
git clone https://github.com/pnettto/Noise-Box.git && cd Noise-Box
tools/setup.sh                                          # venv (cmake, ninja, analysis libs) + JUCE 9.0.6
.tools/bin/cmake --build build                          # build all boxes
.tools/bin/python tools/measure.py box JazzBox --check  # render presets, check clipping/loudness/baseline
./install.sh JazzBox                                    # install to ~/Library/Audio/Plug-Ins
tools/new-box.sh FuzzBox Fzbx "Fuzz Box"                # start a new box
```

**Agents:** read [AGENTS.md](AGENTS.md) first. It has the full workflow, rules and gotchas.

## Layout
- `boxkit/`: shared code (look & feel, user preset store)
- `boxes/<Box>/`: one plugin per style (DSP, presets, processor, editor, render tool, baseline)
- `tools/`: setup, scaffolding and measurement
