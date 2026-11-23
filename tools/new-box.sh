#!/bin/bash
# Scaffold a new box from the Jazz Box template.
#   tools/new-box.sh <TargetName> <CODE> "<Display Name>"
#   tools/new-box.sh FuzzBox Fzbx "Fuzz Box"
#
# TargetName : CamelCase identifier, used for folders, classes and CMake targets (e.g. FuzzBox)
# CODE       : 4-character plugin code, exactly ONE upper-case letter, unique in BOXES.md. Never change it later.
# Display    : name shown in Live's browser and on the plugin (e.g. "Fuzz Box")
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ $# -ne 3 ]]; then
    sed -n '2,9p' "$0"; exit 2
fi
NAME="$1"; CODE="$2"; DISPLAY="$3"
TEMPLATE="JazzBox"; TEMPLATE_CODE="Jzbx"; TEMPLATE_DISPLAY="Jazz Box"

[[ "$NAME" =~ ^[A-Z][A-Za-z0-9]+$ ]] || { echo "TargetName must be CamelCase alphanumeric"; exit 1; }
[[ "$CODE" =~ ^[A-Za-z0-9]{4}$ ]] || { echo "CODE must be 4 alphanumeric characters"; exit 1; }
UPPER=$(echo -n "$CODE" | tr -cd 'A-Z' | wc -c | tr -d ' ')
[[ "$UPPER" == "1" ]] || { echo "CODE must contain exactly one upper-case letter (JUCE/AU rule)"; exit 1; }
[[ -e "boxes/$NAME" ]] && { echo "boxes/$NAME already exists"; exit 1; }
grep -q "| \`$CODE\` |" BOXES.md && { echo "CODE $CODE is already taken (see BOXES.md)"; exit 1; }
grep -q "| $DISPLAY |" BOXES.md && { echo "Display name '$DISPLAY' is already taken"; exit 1; }

cp -R "boxes/$TEMPLATE" "boxes/$NAME"
rm -f "boxes/$NAME/baseline.json"
mv "boxes/$NAME/Source/${TEMPLATE}DSP.h" "boxes/$NAME/Source/${NAME}DSP.h"

find "boxes/$NAME" -type f \( -name '*.h' -o -name '*.cpp' -o -name 'CMakeLists.txt' \) -print0 |
    xargs -0 sed -i '' -e "s/${TEMPLATE}/${NAME}/g" -e "s/${TEMPLATE_DISPLAY}/${DISPLAY}/g" -e "s/${TEMPLATE_CODE}/${CODE}/g"

echo "add_subdirectory(boxes/$NAME)" >> CMakeLists.txt
echo "| $DISPLAY | \`$NAME\` | \`$CODE\` | in progress | TODO: describe the sound goals |" >> BOXES.md

export PATH="$PWD/.tools/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null

cat <<EOF
Created boxes/$NAME (copy of $TEMPLATE, renamed, registered in CMakeLists.txt and BOXES.md).

Still jazz-flavoured, now make it yours (see AGENTS.md > "Adding a box"):
  - boxes/$NAME/Source/${NAME}DSP.h    signal chain + ${NAME}Params
  - boxes/$NAME/Source/Presets.h       factory presets
  - boxes/$NAME/Source/PluginProcessor.cpp  parameter layout (createLayout)
  - boxes/$NAME/Source/PluginEditor.cpp     labels, subtitle, theme/texture
  - BOXES.md                                sound goals row
Leftover jazz references:
EOF
grep -rni "jazz\|wood\|archtop" "boxes/$NAME" | head -20 || true
