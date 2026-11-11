#!/usr/bin/env bash
set -euo pipefail

echo "=========================================================="
echo "  XV Bass Pedal - Compilación macOS Universal (VST3 + AU)"
echo "=========================================================="

cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"

cmake --build build --config Release --parallel

ART_DIR="build/XVBassPedal_artefacts/Release"

# Firma ad-hoc para que Logic Pro / Ableton / macOS carguen AU y VST3 sin bloqueo
if [ -d "$ART_DIR/VST3/XV Bass Pedal.vst3" ]; then
  codesign --force --deep -s - "$ART_DIR/VST3/XV Bass Pedal.vst3"
fi
if [ -d "$ART_DIR/AU/XV Bass Pedal.component" ]; then
  codesign --force --deep -s - "$ART_DIR/AU/XV Bass Pedal.component"
fi

echo ""
echo "[OK] Compilación finalizada con éxito!"
echo "  VST3 : $ART_DIR/VST3/XV Bass Pedal.vst3      -> copiar a ~/Library/Audio/Plug-Ins/VST3/"
echo "  AU   : $ART_DIR/AU/XV Bass Pedal.component   -> copiar a ~/Library/Audio/Plug-Ins/Components/"
