#!/bin/sh
# Compila «AutoPrision 2026.app» (Apple Silicon e Intel). Hace falta un Mac con las
# herramientas de línea de comandos de Xcode (xcode-select --install) o GitHub Actions.
set -e
cd "$(dirname "$0")"
APP="AutoPrision 2026.app"
rm -rf "$APP" build
mkdir -p "$APP/Contents/MacOS" build
for ARCH in arm64 x86_64; do
  swiftc -O -swift-version 5 -target "$ARCH-apple-macos14.0" -o "build/AutoPrision2026-$ARCH" main.swift \
    -framework AppKit -framework ScreenCaptureKit
done
lipo -create build/AutoPrision2026-arm64 build/AutoPrision2026-x86_64 -output "$APP/Contents/MacOS/AutoPrision2026"
cp Info.plist "$APP/Contents/Info.plist"
codesign --force --deep --sign - "$APP"
echo "Hecho: $APP"
