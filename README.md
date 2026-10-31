# AutoPrisión 2026

Bot de clics y teclas para **Prison Legacy**, para Windows, Mac y Linux. Es una versión reescrita desde cero del clásico *AutoPrisión DX Profesional* (2003-2007, MoleSoft) y ya no pierde el teclado: TAB lo para todo siempre.

## Descargar

**👉 [Página de descargas](https://instcide-es.github.io/autoprision-2026/)**

- Windows: [AutoPrision2026-Windows.zip](https://github.com/instcide-Es/autoprision-2026/releases/latest/download/AutoPrision2026-Windows.zip)
- Mac (Apple Silicon e Intel, macOS 14 o posterior): [AutoPrision2026-Mac.zip](https://github.com/instcide-Es/autoprision-2026/releases/latest/download/AutoPrision2026-Mac.zip)
- Linux (64 bits, X11 o XWayland): [AutoPrision2026-Linux.tar.gz](https://github.com/instcide-Es/autoprision-2026/releases/latest/download/AutoPrision2026-Linux.tar.gz)

Cada descarga trae un LEEME con la instalación y los permisos.

## Teclas

| Teclas | Qué hace |
|---|---|
| TAB+1 | Autoclick |
| TAB+2+nº | Grabar la secuencia nº (TAB termina) |
| TAB+3+nº | Reproducir la secuencia nº en bucle hasta TAB |
| TAB+4 | Cambiar la velocidad |
| TAB+5+nº | Reproducir la secuencia nº una vez |
| TAB+6 | Autopelea (Control+R en cada turno) |
| TAB+7 / TAB+8 | Guardar / borrar la señal de turno (el color bajo el ratón) |
| 0 | Quitar el menú |
| **TAB** | **Parar todo** (de reserva: F11 o Pausa en Windows y Linux; F11 o Cmd+punto en Mac) |

## Compilar uno mismo

- Windows: `windows\compilar.cmd` (usa el compilador de C# que ya trae Windows).
- Mac: `sh mac/compilar-mac.sh` (hacen falta las herramientas de Xcode).
- Linux: `sh linux/compilar-linux.sh` (hacen falta gcc, libgtk-3-dev y libxtst-dev).
- GitHub Actions compila los tres en cada cambio (en Linux, además, prueba a grabar y reproducir en una pantalla virtual), y al subir una etiqueta `v*` publica una versión nueva con las tres descargas.

## Aviso

Úsalo bajo tu responsabilidad: las normas del juego pueden no permitir automatizar acciones y se arriesga la cuenta. Este proyecto no tiene relación oficial con Prison Legacy.

Basado en AutoPrisión DX de Jagoba Los Arcos González (MoleSoft, moledj.com). Versión 2026: Carlos Sanz, con ayuda de Claude (Anthropic).
