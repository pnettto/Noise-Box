#!/bin/bash
# One-time setup for a fresh clone. Everything stays inside the repo (no Homebrew, no sudo):
#   .tools/  Python venv with cmake, ninja and the measurement libraries
#   JUCE/    JUCE framework, pinned version
set -euo pipefail
cd "$(dirname "$0")/.."

JUCE_VERSION="9.0.6"

if [[ ! -x .tools/bin/python ]]; then
    echo "Creating .tools venv..."
    python3 -m venv .tools
fi
.tools/bin/pip install -q --disable-pip-version-check -r tools/requirements.txt

if [[ ! -d JUCE/modules ]]; then
    echo "Downloading JUCE $JUCE_VERSION..."
    rm -rf JUCE
    git clone -q --depth 1 --branch "$JUCE_VERSION" https://github.com/juce-framework/JUCE.git JUCE
fi

export PATH="$PWD/.tools/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
echo "Setup complete. Build everything with: .tools/bin/cmake --build build"
