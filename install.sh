#!/bin/bash
# Builds and installs Noise Box plugins (AU + VST3) into your user plug-in folders.
#   ./install.sh            build + install every box
#   ./install.sh JazzBox    build + install one box
set -euo pipefail
cd "$(dirname "$0")"
export PATH="$PWD/.tools/bin:$PATH"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null

if [[ $# -gt 0 ]]; then
    BOXES=("$@")
else
    BOXES=()
    for d in boxes/*/; do BOXES+=("$(basename "$d")"); done
fi

AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU_DIR" "$VST3_DIR"

for box in "${BOXES[@]}"; do
    cmake --build build --target "${box}_AU" "${box}_VST3"
    A="build/boxes/${box}/${box}_artefacts/Release"
    AU=$(ls -d "$A"/AU/*.component); VST3=$(ls -d "$A"/VST3/*.vst3)
    rm -rf "$AU_DIR/$(basename "$AU")" "$VST3_DIR/$(basename "$VST3")"
    cp -R "$AU" "$AU_DIR/"
    cp -R "$VST3" "$VST3_DIR/"
    echo "Installed $(basename "$AU" .component)"
done

# Make macOS pick up new AUs, then validate them
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -a 2>/dev/null | grep -i Pibx || true

echo "Done. In Live: Settings > Plug-Ins > Rescan (or restart Live)."
