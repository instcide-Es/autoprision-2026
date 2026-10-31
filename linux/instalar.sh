#!/bin/sh
# Copia AutoPrisión 2026 a ~/.local/bin y lo añade al menú de aplicaciones (sin sudo).
set -e
cd "$(dirname "$0")"
mkdir -p "$HOME/.local/bin" "$HOME/.local/share/applications"
cp autoprision2026 "$HOME/.local/bin/autoprision2026"
chmod +x "$HOME/.local/bin/autoprision2026"
cat > "$HOME/.local/share/applications/autoprision2026.desktop" <<FIN
[Desktop Entry]
Type=Application
Name=AutoPrisión 2026
Comment=Bot de clics y teclas para Prison Legacy
Exec=$HOME/.local/bin/autoprision2026
Icon=input-mouse
Terminal=false
Categories=Game;Utility;
FIN
echo "Instalado. Búscalo en el menú como «AutoPrisión 2026» o ejecuta: ~/.local/bin/autoprision2026"
