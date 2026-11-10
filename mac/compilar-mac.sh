#!/bin/sh
# Compila «AutoPrision 2026.app» (Apple Silicon e Intel). Hace falta un Mac con las
# herramientas de línea de comandos de Xcode (xcode-select --install) o GitHub Actions.
set -e
cd "$(dirname "$0")"
APP="AutoPrision 2026.app"
rm -rf "$APP" build
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" build
# Icono de las espadas (el de AutoPrisión DX) a partir de icono.png (1024x1024)
mkdir -p build/AppIcon.iconset
for T in 16 32 128 256 512; do
  sips -z $T $T icono.png --out "build/AppIcon.iconset/icon_${T}x${T}.png" >/dev/null
  sips -z $((T*2)) $((T*2)) icono.png --out "build/AppIcon.iconset/icon_${T}x${T}@2x.png" >/dev/null
done
iconutil -c icns build/AppIcon.iconset -o "$APP/Contents/Resources/AppIcon.icns"
for ARCH in arm64 x86_64; do
  swiftc -O -swift-version 5 -target "$ARCH-apple-macos14.0" -o "build/AutoPrision2026-$ARCH" main.swift \
    -framework AppKit -framework ScreenCaptureKit
done
lipo -create build/AutoPrision2026-arm64 build/AutoPrision2026-x86_64 -output "$APP/Contents/MacOS/AutoPrision2026"
cp Info.plist "$APP/Contents/Info.plist"
codesign --force --deep --sign - "$APP"
echo "Hecho: $APP"
