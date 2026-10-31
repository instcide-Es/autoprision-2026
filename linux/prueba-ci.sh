#!/bin/sh
# Prueba de GitHub Actions dentro de Xvfb: graba un clic y una tecla, los reproduce y saca una captura.
set -e
cd "$(dirname "$0")"
D="$HOME/AutoPrision 2026"
mkdir -p "$D"
rm -f "$D/secuencias.txt" "$D/autoprision.log"
printf 'solo_en_juego=0\nparar_si_mueves_raton=0\nreproduccion=200\n' > "$D/autoprision.ini"
pulsa() { xdotool keydown "$1"; sleep 0.15; xdotool keyup "$1"; sleep 0.4; }
./autoprision2026 &
APP=$!
sleep 3
import -window root captura-inicio.png     # sin barra: no está haciendo nada
pulsa F7                                  # empieza a grabar en la secuencia 1
xdotool mousemove 40 40 click 1            # clic fuera de la ventana
sleep 0.4
pulsa a                                    # una tecla
pulsa Tab                                  # termina la grabación
sleep 1
echo "--- secuencias.txt"
cat "$D/secuencias.txt"
grep -q "Secuencia 1=ML,40,40;.*K,38;" "$D/secuencias.txt"
pulsa F8                                   # reproduce la secuencia 1 una vez
sleep 4
import -window root captura-reposo.png     # sin barra otra vez, pasado el aviso de 2,5 s
pulsa Tab                                  # abre el menú del tabulador para la captura
import -window root captura.png
echo "--- autoprision.log"
cat "$D/autoprision.log"
grep -q "Reproducción terminada (sec. 1)" "$D/autoprision.log"
kill $APP
echo "PRUEBA OK"
