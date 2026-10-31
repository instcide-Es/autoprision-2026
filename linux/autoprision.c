// AutoPrisión 2026 para Linux — versión Linux (X11) de AutoPrisión 2026 (Windows: windows/AutoPrision.cs,
// Mac: mac/main.swift).
// Original: Jagoba Los Arcos González · MoleSoft (2003-2007) · www.moledj.com
// Versión 2026: Carlos Sanz, con ayuda de Claude (Anthropic).
// Mismo formato de secuencias.txt y autoprision.ini que en Windows y Mac, guardados en ~/AutoPrision 2026.
//
// Cómo funciona en Linux:
//  · Ventana y panel con GTK 3 (siempre por X11; en Wayland va por XWayland, igual que el juego, que usa SDL2).
//  · Teclas: un hilo mira el teclado cada 10 ms con XQueryKeymap (no depende del foco).
//  · Ratón y teclas: extensión XTest. Grabación: extensión RECORD. Colores: XGetImage.
//  · Las teclas de K,código son códigos de tecla de X11 (A=38, R=27, 1=10…), no de Windows ni de Mac.
//    Modificadores: C+ Control · S+ Mayúsculas · A+ Alt · M+ Super (tecla Windows).
//  · Parada de reserva: F11 o Pausa.
//
// Compilar: sh compilar-linux.sh  (hace falta: gcc, libgtk-3-dev, libxtst-dev)

#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/record.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "1.0"

static char carpeta[PATH_MAX];

static void ruta_en_carpeta(char *out, size_t n, const char *nombre) { snprintf(out, n, "%s/%s", carpeta, nombre); }

static double ahora(void) { return (double)g_get_monotonic_time() / 1e6; }

static int error_x(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; } // ningún error de X cierra el programa

// ---------------------------------------------------------------- teclas

static int K_TAB = 23, K_F7 = 73, K_F8 = 74, K_F9 = 75, K_F10 = 76, K_F11 = 95, K_PAUSA = 127;
static int K_SHIFT_L = 50, K_SHIFT_R = 62, K_CTRL = 37, K_ALT = 64, K_SUPER = 133;
static int FILA[10] = {19, 10, 11, 12, 13, 14, 15, 16, 17, 18};    // 0..9 de la fila de números
static int NUMERICO[10] = {90, 87, 88, 89, 83, 84, 85, 79, 80, 81}; // 0..9 del teclado numérico

static int codigo(Display *d, KeySym ks, int def) {
    int k = d ? XKeysymToKeycode(d, ks) : 0;
    return k > 0 ? k : def;
}

static void preparar_teclas(Display *d) {
    K_TAB = codigo(d, XK_Tab, K_TAB);
    K_F7 = codigo(d, XK_F7, K_F7); K_F8 = codigo(d, XK_F8, K_F8); K_F9 = codigo(d, XK_F9, K_F9);
    K_F10 = codigo(d, XK_F10, K_F10); K_F11 = codigo(d, XK_F11, K_F11); K_PAUSA = codigo(d, XK_Pause, K_PAUSA);
    K_SHIFT_L = codigo(d, XK_Shift_L, K_SHIFT_L); K_SHIFT_R = codigo(d, XK_Shift_R, K_SHIFT_R);
    K_CTRL = codigo(d, XK_Control_L, K_CTRL); K_ALT = codigo(d, XK_Alt_L, K_ALT); K_SUPER = codigo(d, XK_Super_L, K_SUPER);
    for (int i = 0; i <= 9; i++) {
        FILA[i] = codigo(d, XK_0 + i, FILA[i]);
        NUMERICO[i] = codigo(d, XK_KP_0 + i, NUMERICO[i]);
    }
}

static bool en_mapa(const char m[32], int k) { return k > 0 && k < 256 && (m[k >> 3] & (1 << (k & 7))); }

static bool pulsada(Display *d, int k) {
    char m[32];
    XQueryKeymap(d, m);
    return en_mapa(m, k);
}

static bool es_modificador(int k) {
    return k == 37 || k == 105 || k == 50 || k == 62 || k == 64 || k == 108 || k == 133 || k == 134 || k == 66 ||
        k == K_CTRL || k == K_SHIFT_L || k == K_SHIFT_R || k == K_ALT || k == K_SUPER;
}

// Nombres de los códigos de X11 más usados (posiciones de un teclado normal).
static const char *nombre_tecla(int k) {
    static const char *const letras[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"}; // 24..33
    static const char *const letras2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};     // 38..46
    static const char *const letras3[] = {"Z", "X", "C", "V", "B", "N", "M"};              // 52..58
    static const char *const cifras[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};      // 10..18
    if (k >= 24 && k <= 33) return letras[k - 24];
    if (k >= 38 && k <= 46) return letras2[k - 38];
    if (k >= 52 && k <= 58) return letras3[k - 52];
    if (k >= 10 && k <= 18) return cifras[k - 10];
    switch (k) {
    case 19: return "0";
    case 9: return "Esc";
    case 22: return "Borrar";
    case 23: return "Tab";
    case 36: return "Intro";
    case 65: return "Espacio";
    case 67: return "F1"; case 68: return "F2"; case 69: return "F3"; case 70: return "F4"; case 71: return "F5";
    case 72: return "F6"; case 73: return "F7"; case 74: return "F8"; case 75: return "F9"; case 76: return "F10";
    case 95: return "F11"; case 96: return "F12";
    case 111: return "↑"; case 113: return "←"; case 114: return "→"; case 116: return "↓";
    case 127: return "Pausa";
    default: return NULL;
    }
}

// ---------------------------------------------------------------- ratón, teclado y pantalla (cada hilo con su Display)

static void raton_pos(Display *d, int *x, int *y) {
    Window r, h; int wx, wy; unsigned m;
    *x = 0; *y = 0;
    XQueryPointer(d, DefaultRootWindow(d), &r, &h, x, y, &wx, &wy, &m);
}

static void raton_mover(Display *d, int x, int y) {
    XTestFakeMotionEvent(d, -1, x, y, CurrentTime);
    XFlush(d);
}

static void raton_clic(Display *d, bool izquierdo) {
    unsigned b = izquierdo ? 1 : 3;
    XTestFakeButtonEvent(d, b, True, CurrentTime);
    XFlush(d);
    g_usleep(30000);
    XTestFakeButtonEvent(d, b, False, CurrentTime);
    XFlush(d);
}

static void tecla_enviar(Display *d, int k, bool abajo) {
    XTestFakeKeyEvent(d, (unsigned)k, abajo ? True : False, CurrentTime);
    XFlush(d);
}

// Color de un punto de la pantalla como 0xRRGGBB.
static int componente(unsigned long px, unsigned long mascara) {
    if (!mascara) return 0;
    int sh = 0;
    while (!(mascara & 1)) { mascara >>= 1; sh++; }
    return (int)(((px >> sh) & mascara) * 255 / mascara);
}

static bool color_en(Display *d, int x, int y, int *rgb) {
    Screen *s = DefaultScreenOfDisplay(d);
    if (x < 0 || y < 0 || x >= WidthOfScreen(s) || y >= HeightOfScreen(s)) return false;
    XImage *im = XGetImage(d, DefaultRootWindow(d), x, y, 1, 1, AllPlanes, ZPixmap);
    if (!im) return false;
    unsigned long px = XGetPixel(im, 0, 0);
    Visual *v = DefaultVisual(d, DefaultScreen(d));
    *rgb = (componente(px, v->red_mask) << 16) | (componente(px, v->green_mask) << 8) | componente(px, v->blue_mask);
    XDestroyImage(im);
    return true;
}

static bool parecido(int a, int b, int tol) {
    return abs(((a >> 16) & 0xFF) - ((b >> 16) & 0xFF)) <= tol &&
        abs(((a >> 8) & 0xFF) - ((b >> 8) & 0xFF)) <= tol &&
        abs((a & 0xFF) - (b & 0xFF)) <= tol;
}

// ---------------------------------------------------------------- qué programa está delante

static unsigned long propiedad_larga(Display *d, Window w, const char *nombre, Atom tipo_esperado, bool *hay) {
    Atom a = XInternAtom(d, nombre, False), tipo;
    int fmt; unsigned long n = 0, resto; unsigned char *datos = NULL; unsigned long r = 0;
    *hay = false;
    if (XGetWindowProperty(d, w, a, 0, 1, False, tipo_esperado, &tipo, &fmt, &n, &resto, &datos) == Success && datos) {
        if (n > 0 && fmt == 32) { r = *(unsigned long *)datos; *hay = true; }
        XFree(datos);
    }
    return r;
}

static Window ventana_activa(Display *d) {
    bool hay;
    Window w = (Window)propiedad_larga(d, DefaultRootWindow(d), "_NET_ACTIVE_WINDOW", XA_WINDOW, &hay);
    if (!hay || !w) {
        int rev;
        XGetInputFocus(d, &w, &rev);
        if (w == PointerRoot || w == None) w = 0;
    }
    return w;
}

static bool tiene_wm_state(Display *d, Window w) {
    Atom a = XInternAtom(d, "WM_STATE", True), tipo = None;
    if (a == None) return false;
    int fmt; unsigned long n, resto; unsigned char *datos = NULL;
    if (XGetWindowProperty(d, w, a, 0, 0, False, AnyPropertyType, &tipo, &fmt, &n, &resto, &datos) == Success && datos) XFree(datos);
    return tipo != None;
}

// Baja desde el marco del gestor de ventanas hasta la ventana del programa.
static Window buscar_cliente(Display *d, Window w, int prof) {
    if (tiene_wm_state(d, w)) return w;
    if (prof > 4) return 0;
    Window r, p, *hijos = NULL; unsigned nh = 0, i;
    Window encontrada = 0;
    if (XQueryTree(d, w, &r, &p, &hijos, &nh)) {
        for (i = nh; i-- > 0 && !encontrada;) encontrada = buscar_cliente(d, hijos[i], prof + 1);
        if (hijos) XFree(hijos);
    }
    return encontrada;
}

static Window ventana_bajo_raton(Display *d) {
    Window r, h = 0; int rx, ry, wx, wy; unsigned m;
    if (!XQueryPointer(d, DefaultRootWindow(d), &r, &h, &rx, &ry, &wx, &wy, &m) || !h) return 0;
    Window c = buscar_cliente(d, h, 0);
    return c ? c : h;
}

static void anadir_nombre(char *out, size_t n, const char *s) {
    if (!s || !*s) return;
    size_t l = strlen(out);
    snprintf(out + l, n - l, "%s%s", l ? "|" : "", s);
}

// Devuelve el PID del programa de la ventana y en «out» sus nombres en minúsculas (clase|nombre|proceso|ejecutable).
static int nombres_ventana(Display *d, Window w, char *out, size_t n) {
    out[0] = 0;
    if (!w) return 0;
    Window v = w;
    for (int i = 0; i < 8 && v; i++) {
        XClassHint ch = {0};
        if (XGetClassHint(d, v, &ch)) {
            anadir_nombre(out, n, ch.res_class);
            anadir_nombre(out, n, ch.res_name);
            if (ch.res_name) XFree(ch.res_name);
            if (ch.res_class) XFree(ch.res_class);
            break;
        }
        Window r, p, *hijos = NULL; unsigned nh;
        if (!XQueryTree(d, v, &r, &p, &hijos, &nh)) break;
        if (hijos) XFree(hijos);
        if (p == r || p == 0) break;
        v = p;
    }
    bool hay;
    int pid = (int)propiedad_larga(d, v, "_NET_WM_PID", XA_CARDINAL, &hay);
    if (!hay) pid = 0;
    if (pid > 0) {
        char ruta[64], buf[PATH_MAX];
        snprintf(ruta, sizeof ruta, "/proc/%d/comm", pid);
        FILE *f = fopen(ruta, "r");
        if (f) {
            if (fgets(buf, sizeof buf, f)) { buf[strcspn(buf, "\n")] = 0; anadir_nombre(out, n, buf); }
            fclose(f);
        }
        snprintf(ruta, sizeof ruta, "/proc/%d/exe", pid);
        ssize_t l = readlink(ruta, buf, sizeof buf - 1);
        if (l > 0) { buf[l] = 0; char *b = strrchr(buf, '/'); anadir_nombre(out, n, b ? b + 1 : buf); }
    }
    for (char *c = out; *c; c++) *c = (char)tolower((unsigned char)*c);
    return pid;
}

// «proceso» admite varios nombres separados por comas; basta con que algún nombre del programa los contenga.
static bool es_juego(const char *nombres, const char *proceso) {
    char copia[256];
    g_strlcpy(copia, proceso, sizeof copia);
    char *guarda = NULL;
    for (char *t = strtok_r(copia, ",", &guarda); t; t = strtok_r(NULL, ",", &guarda)) {
        char *s = g_strstrip(t);
        for (char *c = s; *c; c++) *c = (char)tolower((unsigned char)*c);
        if (*s && strstr(nombres, s)) return true;
    }
    return false;
}

static void primer_nombre(const char *nombres, char *out, size_t n) {
    if (!*nombres) { g_strlcpy(out, "?", n); return; }
    g_strlcpy(out, nombres, n);
    char *b = strchr(out, '|');
    if (b) *b = 0;
}

typedef struct { Window w; int pid; char nombres[512]; } Delante;

static void mirar_delante(Display *d, Delante *c) {
    Window w = ventana_activa(d);
    if (w != c->w || !w) { c->w = w; c->pid = nombres_ventana(d, w, c->nombres, sizeof c->nombres); }
}

// ---------------------------------------------------------------- acciones

typedef enum { CLIC_IZQ, CLIC_DER, TECLA, ESPERA, COLOR } Tipo;

typedef struct {
    Tipo t;
    int x, y, sc, ms, rgb, tol, timeout;
    bool ctrl, shift, alt, super;
} Accion;

typedef struct { Accion *v; int n, cap; } Lista;

static void lista_anadir(Lista *l, Accion a) {
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 16; l->v = g_realloc_n(l->v, (gsize)l->cap, sizeof(Accion)); }
    l->v[l->n++] = a;
}
static void lista_vaciar(Lista *l) { g_free(l->v); l->v = NULL; l->n = l->cap = 0; }
static Lista lista_copia(const Lista *l) {
    Lista c = {0};
    for (int i = 0; i < l->n; i++) lista_anadir(&c, l->v[i]);
    return c;
}

static bool entero(const char *s, int *v) {
    char *e; errno = 0;
    long r = strtol(s, &e, 10);
    if (!*s || *e || errno || r < INT_MIN || r > INT_MAX) return false;
    *v = (int)r;
    return true;
}

#define FALLA(...) do { snprintf(err, en, __VA_ARGS__); return false; } while (0)
#define NUM(i, dst) do { if (!entero(p[i], &(dst))) FALLA("«%s» no es un número", p[i]); } while (0)

// ML,x,y · MR,x,y · K,[C+][S+][A+][M+]código · E,ms · P,x,y,RRGGBB[,tolerancia[,maxms]]
static bool accion_leer(const char *src, Accion *out, char *err, size_t en) {
    char buf[512];
    g_strlcpy(buf, src, sizeof buf);
    char *p[16]; int np = 0;
    char *s = buf;
    for (;;) {
        char *c = strchr(s, ',');
        if (c) *c = 0;
        if (np < 16) p[np++] = g_strstrip(s);
        if (!c) break;
        s = c + 1;
    }
    Accion a = {0};
    a.t = ESPERA; a.tol = 25;
    if (!g_ascii_strcasecmp(p[0], "ML") || !g_ascii_strcasecmp(p[0], "MR")) {
        if (np != 3) FALLA("se esperaba ML,x,y");
        a.t = toupper((unsigned char)p[0][1]) == 'L' ? CLIC_IZQ : CLIC_DER;
        NUM(1, a.x); NUM(2, a.y);
    } else if (!g_ascii_strcasecmp(p[0], "K")) {
        if (np != 2) FALLA("se esperaba K,código");
        a.t = TECLA; a.sc = -1;
        char *g = NULL;
        for (char *t = strtok_r(p[1], "+", &g); t; t = strtok_r(NULL, "+", &g)) {
            char *u = g_strstrip(t);
            if (!g_ascii_strcasecmp(u, "C")) a.ctrl = true;
            else if (!g_ascii_strcasecmp(u, "S")) a.shift = true;
            else if (!g_ascii_strcasecmp(u, "A")) a.alt = true;
            else if (!g_ascii_strcasecmp(u, "M")) a.super = true;
            else if (!entero(u, &a.sc)) FALLA("código de tecla no válido");
        }
        if (a.sc < 8 || a.sc > 255) FALLA("código de tecla no válido (8-255)");
    } else if (!g_ascii_strcasecmp(p[0], "E")) {
        if (np != 2) FALLA("se esperaba E,milisegundos");
        a.t = ESPERA; NUM(1, a.ms);
        if (a.ms < 0) a.ms = 0;
    } else if (!g_ascii_strcasecmp(p[0], "P")) {
        if (np < 4) FALLA("se esperaba P,x,y,RRGGBB");
        a.t = COLOR; NUM(1, a.x); NUM(2, a.y);
        const char *h = p[3][0] == '#' ? p[3] + 1 : p[3];
        char *e; errno = 0;
        long c = strtol(h, &e, 16);
        if (!*h || *e || errno || c < 0 || c > 0xFFFFFF) FALLA("color no válido");
        a.rgb = (int)c;
        if (np > 4) NUM(4, a.tol);
        if (np > 5) NUM(5, a.timeout);
    } else {
        FALLA("acción desconocida «%s»", p[0]);
    }
    *out = a;
    return true;
}

static void accion_texto(const Accion *a, char *b, size_t n) {
    switch (a->t) {
    case CLIC_IZQ: snprintf(b, n, "ML,%d,%d", a->x, a->y); break;
    case CLIC_DER: snprintf(b, n, "MR,%d,%d", a->x, a->y); break;
    case TECLA:
        snprintf(b, n, "K,%s%s%s%s%d", a->ctrl ? "C+" : "", a->shift ? "S+" : "", a->alt ? "A+" : "", a->super ? "M+" : "", a->sc);
        break;
    case ESPERA: snprintf(b, n, "E,%d", a->ms); break;
    case COLOR:
        if (a->timeout > 0) snprintf(b, n, "P,%d,%d,%06X,%d,%d", a->x, a->y, a->rgb, a->tol, a->timeout);
        else snprintf(b, n, "P,%d,%d,%06X,%d", a->x, a->y, a->rgb, a->tol);
        break;
    }
}

static void accion_describir(const Accion *a, char *b, size_t n) {
    switch (a->t) {
    case CLIC_IZQ: snprintf(b, n, "clic izquierdo en (%d, %d)", a->x, a->y); break;
    case CLIC_DER: snprintf(b, n, "clic derecho en (%d, %d)", a->x, a->y); break;
    case TECLA: {
        const char *nom = nombre_tecla(a->sc);
        char cod[32];
        if (!nom) { snprintf(cod, sizeof cod, "código %d", a->sc); nom = cod; }
        snprintf(b, n, "tecla %s%s%s%s%s", a->ctrl ? "Control+" : "", a->shift ? "Mayús+" : "", a->alt ? "Alt+" : "",
                 a->super ? "Super+" : "", nom);
        break;
    }
    case ESPERA: snprintf(b, n, "esperar %.1f s", a->ms / 1000.0); break;
    case COLOR:
        if (a->timeout > 0) snprintf(b, n, "esperar color #%06X en (%d, %d), máx. %.1f s", a->rgb, a->x, a->y, a->timeout / 1000.0);
        else snprintf(b, n, "esperar color #%06X en (%d, %d)", a->rgb, a->x, a->y);
        break;
    }
}

// Pulsar y soltar van siempre emparejados para no dejar nunca una tecla «pegada».
static void tecla_pulsar(Display *d, const Accion *a) {
    int mods[4], nm = 0;
    if (a->ctrl) mods[nm++] = K_CTRL;
    if (a->shift) mods[nm++] = K_SHIFT_L;
    if (a->alt) mods[nm++] = K_ALT;
    if (a->super) mods[nm++] = K_SUPER;
    for (int i = 0; i < nm; i++) tecla_enviar(d, mods[i], true);
    tecla_enviar(d, a->sc, true);
    g_usleep(40000);
    tecla_enviar(d, a->sc, false);
    for (int i = nm; i-- > 0;) { tecla_enviar(d, mods[i], false); g_usleep(5000); }
}

// ---------------------------------------------------------------- secuencias

static Lista secs[10]; // 1..9

static void secuencias_leer(const char *ruta, Lista s[10], GPtrArray *errores) {
    gchar *datos = NULL; gsize largo = 0;
    if (!g_file_get_contents(ruta, &datos, &largo, NULL)) return;
    gchar *texto = datos;
    if (!g_utf8_validate(datos, (gssize)largo, NULL)) {
        texto = g_convert(datos, (gssize)largo, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        g_free(datos);
        if (!texto) return;
    }
    // El original a veces deja basura delante («10Secuencia 3=...»): se busca el patrón en la línea.
    GRegex *re = g_regex_new("Secuencia\\s*([1-9])\\s*=(.*)$", 0, 0, NULL);
    gchar **lineas = g_strsplit(texto, "\n", -1);
    for (int i = 0; lineas[i]; i++) {
        g_strchomp(lineas[i]);
        GMatchInfo *mi = NULL;
        if (g_regex_match(re, lineas[i], 0, &mi)) {
            gchar *num = g_match_info_fetch(mi, 1), *resto = g_match_info_fetch(mi, 2);
            int n = atoi(num);
            lista_vaciar(&s[n]);
            gchar **trozos = g_strsplit(resto, ";", -1);
            for (int j = 0; trozos[j]; j++) {
                char *t = g_strstrip(trozos[j]);
                if (!*t) continue;
                Accion a; char err[200];
                if (accion_leer(t, &a, err, sizeof err)) lista_anadir(&s[n], a);
                else if (errores) g_ptr_array_add(errores, g_strdup_printf("Secuencia %d: «%s» ignorada (%s)", n, t, err));
            }
            g_strfreev(trozos);
            g_free(num); g_free(resto);
        }
        g_match_info_free(mi);
    }
    g_strfreev(lineas);
    g_regex_unref(re);
    g_free(texto);
}

static bool secuencias_escribir(const char *ruta, Lista s[10], GError **e) {
    GString *r = g_string_new(NULL);
    char b[128];
    for (int i = 1; i <= 9; i++) {
        g_string_append_printf(r, "Secuencia %d=", i);
        for (int j = 0; j < s[i].n; j++) { accion_texto(&s[i].v[j], b, sizeof b); g_string_append(r, b); g_string_append_c(r, ';'); }
        g_string_append(r, "\r\n");
    }
    bool ok = g_file_set_contents(ruta, r->str, (gssize)r->len, e);
    g_string_free(r, TRUE);
    return ok;
}

// ---------------------------------------------------------------- configuración

static struct {
    int velocidad, autoclick_ms, tolerancia, slot, color, pelea_ms;
    bool autolock, solo_en_juego, parar_si_mueves, grabar_pausas, panel;
    char proceso[256], pelea_senal[64], pelea_tecla[32];
} cfg = {200, 1000, 40, 1, 0, 1500, false, true, true, true, true, "Prison", "", "C+27"};

static bool leer_senal(const char *s, int *x, int *y, int *rgb) {
    gchar **p = g_strsplit(s, ",", -1);
    bool ok = g_strv_length(p) == 3;
    if (ok) {
        for (int i = 0; i < 3; i++) g_strstrip(p[i]);
        char *e;
        ok = entero(p[0], x) && entero(p[1], y);
        long c = strtol(p[2], &e, 16);
        ok = ok && *p[2] && !*e;
        *rgb = (int)c;
    }
    g_strfreev(p);
    return ok;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int ini_int(GHashTable *d, const char *k, int def) {
    const char *v = g_hash_table_lookup(d, k);
    int r;
    return (v && entero(v, &r)) ? r : def;
}

static void cfg_leer(void) {
    char ruta[PATH_MAX]; ruta_en_carpeta(ruta, sizeof ruta, "autoprision.ini");
    gchar *texto = NULL;
    if (!g_file_get_contents(ruta, &texto, NULL, NULL)) return;
    GHashTable *d = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    gchar **l = g_strsplit(texto, "\n", -1);
    for (int i = 0; l[i]; i++) {
        char *ig = strchr(l[i], '=');
        if (!ig || ig == l[i]) continue;
        *ig = 0;
        gchar *k = g_ascii_strdown(g_strstrip(l[i]), -1);
        g_hash_table_replace(d, k, g_strdup(g_strstrip(ig + 1)));
    }
    g_strfreev(l);
    g_free(texto);
    cfg.color = ini_int(d, "color", cfg.color);
    cfg.velocidad = MAX(20, ini_int(d, "reproduccion", cfg.velocidad));
    cfg.autolock = ini_int(d, "autolock", 0) != 0;
    cfg.autoclick_ms = MAX(20, ini_int(d, "autoclick_ms", cfg.autoclick_ms));
    cfg.tolerancia = MAX(5, ini_int(d, "tolerancia_raton", cfg.tolerancia));
    cfg.slot = clamp(ini_int(d, "secuencia_activa", cfg.slot), 1, 9);
    cfg.solo_en_juego = ini_int(d, "solo_en_juego", 1) != 0;
    cfg.parar_si_mueves = ini_int(d, "parar_si_mueves_raton", 1) != 0;
    cfg.grabar_pausas = ini_int(d, "grabar_pausas", 1) != 0;
    cfg.panel = ini_int(d, "panel", 1) != 0;
    cfg.pelea_ms = MAX(300, ini_int(d, "autopelea_ms", cfg.pelea_ms));
    const char *v;
    if ((v = g_hash_table_lookup(d, "proceso")) && *v) g_strlcpy(cfg.proceso, v, sizeof cfg.proceso);
    if ((v = g_hash_table_lookup(d, "autopelea_senal"))) g_strlcpy(cfg.pelea_senal, v, sizeof cfg.pelea_senal);
    if ((v = g_hash_table_lookup(d, "autopelea_tecla")) && *v) g_strlcpy(cfg.pelea_tecla, v, sizeof cfg.pelea_tecla);
    g_hash_table_destroy(d);
}

static void cfg_guardar(void) {
    char ruta[PATH_MAX]; ruta_en_carpeta(ruta, sizeof ruta, "autoprision.ini");
    gchar *t = g_strdup_printf(
        "[configuracion]\ncolor=%d\nauto=1\nreproduccion=%d\nautolock=%d\nautoclick_ms=%d\nsecuencia_activa=%d\n"
        "solo_en_juego=%d\nproceso=%s\nparar_si_mueves_raton=%d\ntolerancia_raton=%d\ngrabar_pausas=%d\npanel=%d\n"
        "autopelea_ms=%d\nautopelea_senal=%s\nautopelea_tecla=%s\n",
        cfg.color, cfg.velocidad, cfg.autolock, cfg.autoclick_ms, cfg.slot, cfg.solo_en_juego, cfg.proceso,
        cfg.parar_si_mueves, cfg.tolerancia, cfg.grabar_pausas, cfg.panel, cfg.pelea_ms, cfg.pelea_senal, cfg.pelea_tecla);
    g_file_set_contents(ruta, t, -1, NULL);
    g_free(t);
}

// ---------------------------------------------------------------- sonidos (si el escritorio los tiene)

static void sonar(const char *id) {
    gchar *argv[] = {"canberra-gtk-play", "-i", (gchar *)id, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
}
static void sonido_inicio(void) { sonar("message"); }
static void sonido_fin(void) { sonar("complete"); }
static void sonido_grabar(void) { sonar("dialog-information"); }
static void sonido_anotar(void) { sonar("button-pressed"); }

// ---------------------------------------------------------------- motor (hilo propio)

typedef enum { REPRODUCIR, AUTOCLICK, AUTOPELEA } TipoTrabajo;

typedef struct {
    TipoTrabajo tipo;
    int slot, retraso;
    bool bucle;
    Lista lista;
    char proceso[256];
    bool hay_senal;
    int sx, sy, srgb;
    Accion tecla;
} Trabajo;

static struct {
    gint ocupado;
    GMutex m;
    bool parar;
    char motivo[256];
    double fin_tick;
} motor;

typedef struct {
    Display *d;
    Trabajo *t;
    Delante delante;
    bool vigilar_raton;
    int ex, ey;
    char fin[512];
} Ctx;

static gboolean ui_estado(gpointer p);
static gboolean ui_terminado(gpointer p);

static void avisar(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void avisar(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    gchar *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    g_idle_add(ui_estado, s);
}

static bool debe_parar(char *motivo, size_t n) {
    g_mutex_lock(&motor.m);
    bool p = motor.parar;
    if (p && motivo) g_strlcpy(motivo, motor.motivo[0] ? motor.motivo : "Parado", n);
    g_mutex_unlock(&motor.m);
    return p;
}

static void motor_parar(const char *motivo) {
    g_mutex_lock(&motor.m);
    g_strlcpy(motor.motivo, motivo, sizeof motor.motivo);
    motor.parar = true;
    g_mutex_unlock(&motor.m);
}

static bool motor_ocupado(void) { return g_atomic_int_get(&motor.ocupado) != 0; }

// Se llama constantemente: cualquiera de estas condiciones para el trabajo en menos de 15 ms.
static bool comprobar(Ctx *c) {
    if (debe_parar(c->fin, sizeof c->fin)) return false;
    char m[32];
    XQueryKeymap(c->d, m);
    if (en_mapa(m, K_TAB)) { g_strlcpy(c->fin, "Parado con el tabulador", sizeof c->fin); return false; }
    if (en_mapa(m, K_F11) || en_mapa(m, K_PAUSA)) { g_strlcpy(c->fin, "Parado con la tecla de parada", sizeof c->fin); return false; }
    if (cfg.solo_en_juego) {
        mirar_delante(c->d, &c->delante);
        if (!es_juego(c->delante.nombres, c->t->proceso)) {
            char n[128]; primer_nombre(c->delante.nombres, n, sizeof n);
            snprintf(c->fin, sizeof c->fin, "Parado: el juego no está delante (delante: %s)", n);
            return false;
        }
    }
    if (cfg.parar_si_mueves && c->vigilar_raton) {
        int x, y; raton_pos(c->d, &x, &y);
        int dx = x - c->ex, dy = y - c->ey, tol = cfg.tolerancia;
        if (dx * dx + dy * dy > tol * tol) { g_strlcpy(c->fin, "Parado: has movido el ratón", sizeof c->fin); return false; }
    }
    return true;
}

static bool esperar(Ctx *c, double ms) {
    double t0 = ahora();
    for (;;) {
        if (!comprobar(c)) return false;
        double resta = ms / 1000.0 - (ahora() - t0);
        if (resta <= 0) return true;
        g_usleep((gulong)(MIN(0.015, resta) * 1e6));
    }
}

static void mover_a(Ctx *c, int x, int y) {
    raton_mover(c->d, x, y);
    c->ex = x; c->ey = y; c->vigilar_raton = true;
}

static bool color_parecido(Ctx *c, int x, int y, int rgb, int tol, bool *si) {
    int col;
    if (!color_en(c->d, x, y, &col)) {
        snprintf(c->fin, sizeof c->fin, "Parado: no se pudo leer el color de la pantalla en (%d, %d)", x, y);
        return false;
    }
    *si = parecido(col, rgb, tol);
    return true;
}

static bool ejecutar(Ctx *c, const Accion *a) {
    switch (a->t) {
    case CLIC_IZQ:
    case CLIC_DER:
        mover_a(c, a->x, a->y);
        if (!esperar(c, 30)) return false; // el juego necesita ver el ratón encima antes del clic
        raton_clic(c->d, a->t == CLIC_IZQ);
        return true;
    case TECLA:
        tecla_pulsar(c->d, a);
        return true;
    case ESPERA:
        return esperar(c, a->ms);
    case COLOR: {
        double t0 = ahora();
        for (;;) {
            bool si;
            if (!color_parecido(c, a->x, a->y, a->rgb, a->tol, &si)) return false;
            if (si) return true;
            if (a->timeout > 0 && (ahora() - t0) * 1000 > a->timeout) {
                snprintf(c->fin, sizeof c->fin, "Parado: no apareció el color #%06X en (%d, %d)", a->rgb, a->x, a->y);
                return false;
            }
            if (!esperar(c, 50)) return false;
        }
    }
    }
    return true;
}

static void trabajo_reproducir(Ctx *c) {
    Trabajo *t = c->t;
    if (t->lista.n == 0) { snprintf(c->fin, sizeof c->fin, "La secuencia %d está vacía", t->slot); return; }
    int vuelta = 0;
    do {
        vuelta++;
        for (int i = 0; i < t->lista.n; i++) {
            if (t->bucle) avisar("BUCLE sec. %d · vuelta %d · %d/%d · TAB para", t->slot, vuelta, i + 1, t->lista.n);
            else avisar("Reproduciendo sec. %d · %d/%d · TAB para", t->slot, i + 1, t->lista.n);
            if (!comprobar(c) || !ejecutar(c, &t->lista.v[i]) || !esperar(c, cfg.velocidad)) return;
        }
    } while (t->bucle);
    snprintf(c->fin, sizeof c->fin, "Reproducción terminada (sec. %d)", t->slot);
}

static void trabajo_autoclick(Ctx *c) {
    bool fijo = cfg.autolock;
    int px, py; raton_pos(c->d, &px, &py);
    for (int n = 1;; n++) {
        if (fijo) mover_a(c, px, py);
        if (!comprobar(c)) return;
        raton_clic(c->d, true);
        if (fijo) avisar("AUTOCLICK fijo en (%d, %d) · %d clics · TAB para", px, py, n);
        else avisar("AUTOCLICK donde está el ratón · %d clics · TAB para", n);
        if (!esperar(c, cfg.autoclick_ms)) return;
    }
}

// Autopelea (turno continuo): pulsa Control+R, que en Prison Legacy repite las acciones del turno anterior;
// el juego solo lo acepta en tu turno (también en un TURNO DOBLE). Con «señal de turno» (un punto que solo
// tiene ese color cuando es tu turno) pulsa justo al empezar tu turno; sin ella, pulsa cada autopelea_ms.
static void trabajo_autopelea(Ctx *c) {
    Trabajo *t = c->t;
    char desc[128]; accion_describir(&t->tecla, desc, sizeof desc);
    for (int n = 0;;) {
        bool si;
        if (t->hay_senal) {
            avisar("AUTOPELEA · esperando tu turno · %d turnos · TAB para", n);
            for (;;) {
                if (!color_parecido(c, t->sx, t->sy, t->srgb, 30, &si)) return;
                if (si) break;
                if (!esperar(c, 100)) return;
            }
        }
        if (!comprobar(c)) return;
        tecla_pulsar(c->d, &t->tecla);
        n++;
        if (t->hay_senal) {
            avisar("AUTOPELEA · %s (%d) · TAB para", desc, n);
            // Si la señal sigue ahí pasado el tiempo es un turno doble (o no entró): se repite.
            double t0 = ahora();
            for (;;) {
                if (!color_parecido(c, t->sx, t->sy, t->srgb, 30, &si)) return;
                if (!si || (ahora() - t0) * 1000 >= cfg.pelea_ms * 2) break;
                if (!esperar(c, 100)) return;
            }
        } else {
            avisar("AUTOPELEA · %s (%d) cada %d ms · TAB para", desc, n, cfg.pelea_ms);
            if (!esperar(c, cfg.pelea_ms)) return;
        }
    }
}

static gpointer motor_hilo(gpointer p) {
    Ctx c = {0};
    c.t = p;
    c.d = XOpenDisplay(NULL);
    if (!c.d) {
        g_strlcpy(c.fin, "Error: no se pudo abrir la pantalla X11", sizeof c.fin);
    } else {
        bool seguir = true;
        for (int r = c.t->retraso; r > 0; r -= 100) {
            if (debe_parar(c.fin, sizeof c.fin)) { seguir = false; break; }
            avisar("Empieza en %d s: cambia al juego", (r + 999) / 1000);
            g_usleep(100000);
        }
        if (seguir) {
            sonido_inicio();
            switch (c.t->tipo) {
            case REPRODUCIR: trabajo_reproducir(&c); break;
            case AUTOCLICK: trabajo_autoclick(&c); break;
            case AUTOPELEA: trabajo_autopelea(&c); break;
            }
        }
        XCloseDisplay(c.d);
    }
    if (!c.fin[0]) g_strlcpy(c.fin, "Parado", sizeof c.fin);
    g_mutex_lock(&motor.m);
    motor.fin_tick = ahora();
    g_mutex_unlock(&motor.m);
    g_atomic_int_set(&motor.ocupado, 0);
    sonido_fin();
    g_idle_add(ui_terminado, g_strdup(c.fin));
    lista_vaciar(&c.t->lista);
    g_free(c.t);
    return NULL;
}

static void motor_lanzar(Trabajo *t) {
    if (!g_atomic_int_compare_and_exchange(&motor.ocupado, 0, 1)) { lista_vaciar(&t->lista); g_free(t); return; }
    g_mutex_lock(&motor.m);
    motor.parar = false; motor.motivo[0] = 0;
    g_mutex_unlock(&motor.m);
    g_strlcpy(t->proceso, cfg.proceso, sizeof t->proceso);
    g_thread_unref(g_thread_new("motor", motor_hilo, t));
}

static double motor_fin_tick(void) {
    g_mutex_lock(&motor.m);
    double f = motor.fin_tick;
    g_mutex_unlock(&motor.m);
    return f;
}

// ---------------------------------------------------------------- grabación (extensión RECORD de X11)

static struct {
    bool activo;          // solo hilo principal
    Display *ctrl, *datos, *consulta;
    XRecordContext ctx;
    GThread *hilo;
    gint parar;
    char proceso[256];
    bool solo;
    bool abajo[256];      // solo hilo de grabación
    Lista acciones;       // solo hilo principal
    double ultimo;
    int slot;
} grab;

static gboolean ui_anotada(gpointer p);

static bool se_graba(int pid, const char *nombres) {
    if (pid == getpid()) return false; // clics y teclas en esta ventana, no
    return !grab.solo || es_juego(nombres, grab.proceso);
}

static void grab_evento(XPointer priv, XRecordInterceptData *r) {
    (void)priv;
    if (r->category == XRecordFromServer && r->data_len * 4 >= sizeof(xEvent)) {
        xEvent *ev = (xEvent *)r->data;
        int tipo = ev->u.u.type & 0x7F, det = ev->u.u.detail;
        unsigned st = ev->u.keyButtonPointer.state;
        char nombres[512];
        if (tipo == KeyRelease) {
            grab.abajo[det & 0xFF] = false;
        } else if (tipo == KeyPress) {
            bool repetida = grab.abajo[det & 0xFF];
            grab.abajo[det & 0xFF] = true;
            if (!repetida && !es_modificador(det) && det != K_TAB && det != K_F7 && det != K_F8 && det != K_F9 &&
                det != K_F10 && det != K_F11 && det != K_PAUSA) {
                int pid = nombres_ventana(grab.consulta, ventana_activa(grab.consulta), nombres, sizeof nombres);
                if (se_graba(pid, nombres)) {
                    Accion *a = g_new0(Accion, 1);
                    a->t = TECLA; a->sc = det; a->tol = 25;
                    a->ctrl = st & ControlMask; a->shift = st & ShiftMask; a->alt = st & Mod1Mask; a->super = st & Mod4Mask;
                    g_idle_add(ui_anotada, a);
                }
            }
        } else if (tipo == ButtonPress && (det == 1 || det == 3)) {
            int pid = nombres_ventana(grab.consulta, ventana_bajo_raton(grab.consulta), nombres, sizeof nombres);
            if (se_graba(pid, nombres)) {
                Accion *a = g_new0(Accion, 1);
                a->t = det == 1 ? CLIC_IZQ : CLIC_DER;
                a->x = ev->u.keyButtonPointer.rootX; a->y = ev->u.keyButtonPointer.rootY; a->tol = 25;
                g_idle_add(ui_anotada, a);
            }
        }
    }
    XRecordFreeData(r);
}

static gpointer grab_hilo(gpointer p) {
    (void)p;
    while (!g_atomic_int_get(&grab.parar)) {
        XRecordProcessReplies(grab.datos);
        g_usleep(5000);
    }
    return NULL;
}

static void grab_cerrar_displays(void) {
    if (grab.consulta) XCloseDisplay(grab.consulta);
    if (grab.datos) XCloseDisplay(grab.datos);
    if (grab.ctrl) XCloseDisplay(grab.ctrl);
    grab.consulta = grab.datos = grab.ctrl = NULL;
}

// Devuelve NULL si va bien, o el motivo del fallo.
static const char *grab_empezar(void) {
    grab.ctrl = XOpenDisplay(NULL);
    grab.datos = XOpenDisplay(NULL);
    grab.consulta = XOpenDisplay(NULL);
    if (!grab.ctrl || !grab.datos || !grab.consulta) { grab_cerrar_displays(); return "no se pudo abrir la pantalla X11"; }
    int may, men;
    if (!XRecordQueryVersion(grab.ctrl, &may, &men)) { grab_cerrar_displays(); return "falta la extensión RECORD de X11"; }
    XRecordRange *rango = XRecordAllocRange();
    if (!rango) { grab_cerrar_displays(); return "sin memoria"; }
    rango->device_events.first = KeyPress;
    rango->device_events.last = ButtonPress;
    XRecordClientSpec clientes = XRecordAllClients;
    grab.ctx = XRecordCreateContext(grab.ctrl, 0, &clientes, 1, &rango, 1);
    XFree(rango);
    if (!grab.ctx) { grab_cerrar_displays(); return "X11 no dejó crear la grabación"; }
    XSync(grab.ctrl, False);
    memset(grab.abajo, 0, sizeof grab.abajo);
    g_strlcpy(grab.proceso, cfg.proceso, sizeof grab.proceso);
    grab.solo = cfg.solo_en_juego;
    if (!XRecordEnableContextAsync(grab.datos, grab.ctx, grab_evento, NULL)) {
        XRecordFreeContext(grab.ctrl, grab.ctx);
        grab_cerrar_displays();
        return "X11 no dejó empezar la grabación";
    }
    XFlush(grab.datos);
    g_atomic_int_set(&grab.parar, 0);
    grab.hilo = g_thread_new("grabar", grab_hilo, NULL);
    lista_vaciar(&grab.acciones);
    grab.ultimo = -1;
    grab.activo = true;
    return NULL;
}

static void grab_terminar(void) {
    if (!grab.activo) return;
    g_atomic_int_set(&grab.parar, 1);
    g_thread_join(grab.hilo);
    grab.hilo = NULL;
    XRecordDisableContext(grab.ctrl, grab.ctx);
    XSync(grab.ctrl, False);
    XRecordFreeContext(grab.ctrl, grab.ctx);
    XSync(grab.ctrl, False);
    grab_cerrar_displays();
    grab.activo = false;
}

// ---------------------------------------------------------------- interfaz

static const char *GRIS = "#292929", *VERDE = "#146e29", *ROJO = "#ab1f1f", *AZUL = "#1f4596";

static Display *dpy_ui; // conexión propia del hilo principal (aparte de la de GTK)
static GtkWidget *ventana, *combo_slots, *txt_acciones, *txt_log;
static GtkWidget *num_vel, *num_auto, *num_tol, *txt_proceso;
static GtkWidget *chk_lock, *chk_juego, *chk_mover, *chk_pausas, *chk_panel;
static GtkWidget *panel_win, *panel_lbl;
static GtkCssProvider *panel_css;
static bool cargando = true;

typedef enum { M_CERRADO, M_PRINCIPAL, M_GRABAR, M_REPRODUCIR, M_BUCLE, M_VELOCIDAD } Modo;
static Modo menu = M_CERRADO;
static double menu_hasta = 0;

static void ui_log(const char *s) {
    GDateTime *d = g_date_time_new_now_local();
    gchar *hora = g_date_time_format(d, "%H:%M:%S"), *fecha = g_date_time_format(d, "%Y-%m-%d ");
    gchar *l = g_strdup_printf("%s  %s\n", hora, s);
    if (txt_log) {
        GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_log));
        GtkTextIter fin;
        gtk_text_buffer_get_end_iter(b, &fin);
        gtk_text_buffer_insert(b, &fin, l, -1);
        while (gtk_text_buffer_get_line_count(b) > 400) {
            GtkTextIter a, z;
            gtk_text_buffer_get_start_iter(b, &a);
            gtk_text_buffer_get_iter_at_line(b, &z, 1);
            gtk_text_buffer_delete(b, &a, &z);
        }
        gtk_text_buffer_get_end_iter(b, &fin);
        GtkTextMark *m = gtk_text_buffer_create_mark(b, NULL, &fin, FALSE);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(txt_log), m);
        gtk_text_buffer_delete_mark(b, m);
    }
    char ruta[PATH_MAX]; ruta_en_carpeta(ruta, sizeof ruta, "autoprision.log");
    FILE *f = fopen(ruta, "a");
    if (f) { fprintf(f, "%s%s", fecha, l); fclose(f); }
    g_free(l); g_free(hora); g_free(fecha);
    g_date_time_unref(d);
}

static void ui_logf(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void ui_logf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    gchar *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    ui_log(s);
    g_free(s);
}

static void panel_poner(const char *s, const char *fondo) {
    if (!panel_lbl) return;
    gtk_label_set_text(GTK_LABEL(panel_lbl), s);
    gchar *css = g_strdup_printf("* { background-color: %s; color: #ffffff; font-weight: bold; }", fondo);
    gtk_css_provider_load_from_data(panel_css, css, -1, NULL);
    g_free(css);
}

static void panel_mostrar(bool si) {
    if (si) gtk_widget_show_all(panel_win); else gtk_widget_hide(panel_win);
}

static void listo(const char *ultimo) {
    if (motor_ocupado() || grab.activo) return;
    gchar *s = g_strdup_printf("%s%sListo · sec. %d (%d acciones)", ultimo ? ultimo : "", ultimo ? " · " : "", cfg.slot, secs[cfg.slot].n);
    panel_poner(s, ultimo && g_str_has_prefix(ultimo, "Parado") ? AZUL : GRIS);
    g_free(s);
}

static void refrescar_slots(void) {
    bool antes = cargando;
    cargando = true;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo_slots));
    for (int i = 1; i <= 9; i++) {
        gchar *t = g_strdup_printf("Secuencia %d  (%d acciones)", i, secs[i].n);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_slots), t);
        g_free(t);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_slots), cfg.slot - 1);
    cargando = antes;
}

static void mostrar_texto(void) {
    GString *s = g_string_new(NULL);
    char t[128], d[160];
    for (int i = 0; i < secs[cfg.slot].n; i++) {
        accion_texto(&secs[cfg.slot].v[i], t, sizeof t);
        accion_describir(&secs[cfg.slot].v[i], d, sizeof d);
        g_string_append_printf(s, "%-24s# %s\n", t, d);
    }
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_acciones)), s->str, -1);
    g_string_free(s, TRUE);
}

static void guardar_secuencias(void) {
    char ruta[PATH_MAX]; ruta_en_carpeta(ruta, sizeof ruta, "secuencias.txt");
    GError *e = NULL;
    if (!secuencias_escribir(ruta, secs, &e)) { ui_logf("ERROR guardando secuencias.txt: %s", e ? e->message : "?"); g_clear_error(&e); }
    refrescar_slots();
}

static void aviso(const char *titulo, const char *texto) {
    GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(ventana), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", titulo);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(m), "%s", texto);
    gtk_dialog_run(GTK_DIALOG(m));
    gtk_widget_destroy(m);
}

// ---- acciones

static void reproducir(bool bucle, int retraso) {
    if (grab.activo || motor_ocupado()) return;
    ui_logf("%s de la secuencia %d (%d acciones, una cada %d ms)", bucle ? "Bucle" : "Reproducción", cfg.slot, secs[cfg.slot].n, cfg.velocidad);
    Trabajo *t = g_new0(Trabajo, 1);
    t->tipo = REPRODUCIR; t->slot = cfg.slot; t->bucle = bucle; t->retraso = retraso;
    t->lista = lista_copia(&secs[cfg.slot]);
    motor_lanzar(t);
}

static void auto_click(int retraso) {
    if (grab.activo || motor_ocupado()) return;
    ui_logf("Autoclick cada %d ms%s", cfg.autoclick_ms, cfg.autolock ? " en posición fija" : "");
    Trabajo *t = g_new0(Trabajo, 1);
    t->tipo = AUTOCLICK; t->retraso = retraso;
    motor_lanzar(t);
}

static void auto_pelea(int retraso) {
    if (grab.activo || motor_ocupado()) return;
    Trabajo *t = g_new0(Trabajo, 1);
    t->tipo = AUTOPELEA; t->retraso = retraso;
    char k[64], err[128];
    snprintf(k, sizeof k, "K,%s", cfg.pelea_tecla);
    if (!accion_leer(k, &t->tecla, err, sizeof err)) {
        ui_logf("autopelea_tecla=%s no vale (%s): se usa Control+R (C+27)", cfg.pelea_tecla, err);
        accion_leer("K,C+27", &t->tecla, err, sizeof err);
    }
    t->hay_senal = leer_senal(cfg.pelea_senal, &t->sx, &t->sy, &t->srgb);
    if (t->hay_senal) ui_logf("Autopelea: tecla al empezar cada turno tuyo (señal #%06X en %d, %d)", t->srgb, t->sx, t->sy);
    else ui_logf("Autopelea: tecla cada %d ms (sin señal de turno)", cfg.pelea_ms);
    motor_lanzar(t);
}

// Con el ratón sobre algo que solo se ve en tu turno (p. ej. el botón FIN TURNO).
static void guardar_senal(void) {
    int x, y, c;
    raton_pos(dpy_ui, &x, &y);
    if (!color_en(dpy_ui, x, y, &c)) { ui_log("No se pudo leer el color de la pantalla"); return; }
    snprintf(cfg.pelea_senal, sizeof cfg.pelea_senal, "%d,%d,%06X", x, y, c);
    cfg_guardar();
    sonido_grabar();
    ui_logf("Señal de turno guardada: #%06X en (%d, %d)", c, x, y);
    listo("Señal de turno guardada");
}

static void empezar_grabacion(void) {
    if (motor_ocupado() || grab.activo) return;
    grab.slot = cfg.slot;
    const char *e = grab_empezar();
    if (e) { ui_logf("ERROR: no se puede grabar: %s", e); return; }
    sonido_grabar();
    ui_logf("Grabando en la secuencia %d… (tabulador para terminar)", grab.slot);
    gchar *s = g_strdup_printf("GRABANDO sec. %d · 0 acciones · TAB termina", grab.slot);
    panel_poner(s, ROJO);
    g_free(s);
}

static void terminar_grabacion(void) {
    if (!grab.activo) return;
    grab_terminar();
    sonido_fin();
    if (grab.acciones.n == 0) {
        ui_logf("Grabación vacía: se conserva la secuencia %d anterior", grab.slot);
        listo(NULL);
        return;
    }
    lista_vaciar(&secs[grab.slot]);
    secs[grab.slot] = grab.acciones;
    grab.acciones = (Lista){0};
    guardar_secuencias();
    ui_logf("Grabación terminada: %d acciones en la secuencia %d", secs[grab.slot].n, grab.slot);
    if (cfg.slot == grab.slot) mostrar_texto();
    listo(NULL);
}

static void anotar(Accion a) {
    if (!grab.activo) return;
    double t = ahora();
    if (cfg.grabar_pausas && grab.ultimo >= 0) {
        double hueco = (t - grab.ultimo) * 1000 - cfg.velocidad;
        if (hueco > 800) {
            Accion e = {0};
            e.t = ESPERA; e.ms = (int)(hueco / 100 + 0.5) * 100;
            lista_anadir(&grab.acciones, e);
        }
    }
    grab.ultimo = t;
    lista_anadir(&grab.acciones, a);
    sonido_anotar();
    gchar *s = g_strdup_printf("GRABANDO sec. %d · %d acciones · TAB termina", grab.slot, grab.acciones.n);
    panel_poner(s, ROJO);
    g_free(s);
    char d[160]; accion_describir(&a, d, sizeof d);
    ui_logf("  + %s", d);
}

static void anotar_color(void) {
    int x, y, c;
    raton_pos(dpy_ui, &x, &y);
    if (!color_en(dpy_ui, x, y, &c)) { ui_log("No se pudo leer el color de la pantalla"); return; }
    if (grab.activo) {
        Accion a = {0};
        a.t = COLOR; a.x = x; a.y = y; a.rgb = c; a.tol = 25;
        anotar(a);
    } else {
        ui_logf("Color en (%d, %d): #%06X", x, y, c);
    }
}

static void alternar_grabacion(void) {
    if (motor_ocupado()) { motor_parar("Parado para grabar"); return; }
    if (grab.activo) { terminar_grabacion(); return; }
    empezar_grabacion();
}

static void parar_todo(const char *motivo) {
    if (grab.activo) terminar_grabacion();
    if (motor_ocupado()) motor_parar(motivo);
}

// ---- menú del tabulador (como el original)

static void cerrar_menu(void) {
    if (menu == M_CERRADO) return;
    menu = M_CERRADO;
    listo(NULL);
}

static void abrir_menu(Modo m) {
    menu = m;
    menu_hasta = ahora() + 8;
    sonido_anotar();
    GString *con = g_string_new(NULL);
    for (int i = 1; i <= 9; i++) if (secs[i].n) g_string_append_printf(con, "%s%d", con->len ? "," : "", i);
    gchar *datos = g_strdup_printf("   con datos: %s", con->len ? con->str : "ninguna");
    gchar *s = NULL;
    switch (m) {
    case M_PRINCIPAL: s = g_strdup("1 Autoclick · 2 Grabar · 3 Reproducir · 4 Velocidad · 5 Una vez · 6 Autopelea · 7/8 Señal turno · 0 Quitar"); break;
    case M_GRABAR: s = g_strconcat("GRABAR en la secuencia nº… (1-9)", datos, NULL); break;
    case M_REPRODUCIR: s = g_strconcat("REPRODUCIR UNA VEZ la secuencia nº… (1-9)", datos, NULL); break;
    case M_BUCLE: s = g_strconcat("REPRODUCIR (hasta TAB) la secuencia nº… (1-9)", datos, NULL); break;
    case M_VELOCIDAD:
        s = g_strdup_printf("Una acción cada %d ms (1 más rápido · 2 más lento) · autoclick %d ms (3 · 4)", cfg.velocidad, cfg.autoclick_ms);
        break;
    case M_CERRADO: break;
    }
    if (s) panel_poner(s, AZUL);
    g_free(s); g_free(datos);
    g_string_free(con, TRUE);
}

static void seleccionar(int n) {
    if (n >= 1 && n <= 9) gtk_combo_box_set_active(GTK_COMBO_BOX(combo_slots), n - 1); // dispara en_cambiar_slot
}

static void poner_numeros(void) {
    bool antes = cargando;
    cargando = true;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_vel), cfg.velocidad);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_auto), cfg.autoclick_ms);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_tol), cfg.tolerancia);
    cargando = antes;
}

static void tecla_menu(int n) {
    if (n < 0) { // tabulador
        if (motor_ocupado() || grab.activo) { cerrar_menu(); parar_todo("Parado con el tabulador"); }
        else if (ahora() - motor_fin_tick() < 0.7) {} // ese tabulador ya paró la reproducción
        else if (menu != M_CERRADO) cerrar_menu();
        else abrir_menu(M_PRINCIPAL);
        return;
    }
    if (menu == M_CERRADO) return;
    if (n == 0) { cerrar_menu(); return; }
    switch (menu) {
    case M_PRINCIPAL:
        switch (n) {
        case 1: cerrar_menu(); auto_click(0); break;
        case 2: abrir_menu(M_GRABAR); break;
        case 3: abrir_menu(M_BUCLE); break;
        case 4: abrir_menu(M_VELOCIDAD); break;
        case 5: abrir_menu(M_REPRODUCIR); break;
        case 6: cerrar_menu(); auto_pelea(0); break;
        case 7: cerrar_menu(); guardar_senal(); break;
        case 8:
            cerrar_menu(); cfg.pelea_senal[0] = 0; cfg_guardar();
            ui_logf("Señal de turno borrada: la autopelea pulsará la tecla cada %d ms", cfg.pelea_ms);
            listo("Señal de turno borrada");
            break;
        default: break;
        }
        break;
    case M_GRABAR: cerrar_menu(); seleccionar(n); empezar_grabacion(); break;
    case M_REPRODUCIR:
    case M_BUCLE: {
        bool bucle = menu == M_BUCLE;
        cerrar_menu(); seleccionar(n); reproducir(bucle, 0);
        break;
    }
    case M_VELOCIDAD:
        if (n == 1) cfg.velocidad = MAX(20, cfg.velocidad - 100);
        else if (n == 2) cfg.velocidad = MIN(10000, cfg.velocidad + 100);
        else if (n == 3) cfg.autoclick_ms = MAX(20, cfg.autoclick_ms - 100);
        else if (n == 4) cfg.autoclick_ms = MIN(60000, cfg.autoclick_ms + 100);
        poner_numeros();
        cfg_guardar();
        abrir_menu(M_VELOCIDAD);
        break;
    case M_CERRADO: break;
    }
}

// Códigos internos del vigía: 0-9 números, 100 TAB, 107-110 F7-F10, 111 parada, 117 Mayús+F7.
static gboolean ui_tecla(gpointer p) {
    int id = GPOINTER_TO_INT(p) - 1;
    switch (id) {
    case 100: tecla_menu(-1); break;
    case 107: alternar_grabacion(); break;
    case 108: if (motor_ocupado()) motor_parar("Parado con F8"); else reproducir(false, 0); break;
    case 109: if (motor_ocupado()) motor_parar("Parado con F9"); else auto_click(0); break;
    case 110: if (motor_ocupado()) motor_parar("Parado con F10"); else reproducir(true, 0); break;
    case 111: parar_todo("Parado con la tecla de parada"); break;
    case 117: anotar_color(); break;
    default: if (id >= 0 && id <= 9) tecla_menu(id); break;
    }
    return G_SOURCE_REMOVE;
}

// Un hilo propio mira el teclado cada 10 ms. No depende del foco: por eso no «pierde el teclado».
// El tabulador y los números siguen llegando al juego, igual que con el programa viejo.
static gpointer vigia_hilo(gpointer p) {
    (void)p;
    Display *d = XOpenDisplay(NULL);
    if (!d) return NULL;
    bool antes[128] = {0};
    for (;;) {
        char m[32];
        XQueryKeymap(d, m);
        int nuevos[16], nn = 0;
#define MIRA(id, si) do { bool s_ = (si); if (s_ && !antes[id] && nn < 16) nuevos[nn++] = (id); antes[id] = s_; } while (0)
        MIRA(100, en_mapa(m, K_TAB));
        for (int i = 0; i <= 9; i++) MIRA(i, en_mapa(m, FILA[i]) || en_mapa(m, NUMERICO[i]));
        bool mayus = en_mapa(m, K_SHIFT_L) || en_mapa(m, K_SHIFT_R);
        bool f7 = en_mapa(m, K_F7);
        MIRA(107, f7 && !mayus);
        MIRA(117, f7 && mayus);
        MIRA(108, en_mapa(m, K_F8));
        MIRA(109, en_mapa(m, K_F9));
        MIRA(110, en_mapa(m, K_F10));
        MIRA(111, en_mapa(m, K_F11) || en_mapa(m, K_PAUSA));
#undef MIRA
        for (int i = 0; i < nn; i++) g_idle_add(ui_tecla, GINT_TO_POINTER(nuevos[i] + 1));
        g_usleep(10000);
    }
    return NULL;
}

// ---- llamadas desde otros hilos

static gboolean ui_estado(gpointer p) {
    if (motor_ocupado()) panel_poner(p, VERDE);
    g_free(p);
    return G_SOURCE_REMOVE;
}

static gboolean ui_terminado(gpointer p) {
    ui_log(p);
    listo(p);
    g_free(p);
    return G_SOURCE_REMOVE;
}

static gboolean ui_anotada(gpointer p) {
    anotar(*(Accion *)p);
    g_free(p);
    return G_SOURCE_REMOVE;
}

static gboolean ui_reloj(gpointer p) {
    (void)p;
    if (menu != M_CERRADO && ahora() > menu_hasta) cerrar_menu();
    return G_SOURCE_CONTINUE;
}

// ---- botones y ajustes

static void en_cambio_ajustes(GtkWidget *w, gpointer p) {
    (void)w; (void)p;
    if (cargando) return;
    cfg.velocidad = clamp(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_vel)), 20, 10000);
    cfg.autoclick_ms = clamp(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_auto)), 20, 60000);
    cfg.tolerancia = clamp(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_tol)), 5, 500);
    cfg.autolock = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_lock));
    cfg.solo_en_juego = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_juego));
    cfg.parar_si_mueves = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_mover));
    cfg.grabar_pausas = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_pausas));
    gchar *pr = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_proceso))));
    if (*pr) g_strlcpy(cfg.proceso, pr, sizeof cfg.proceso);
    g_free(pr);
    bool panel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_panel));
    if (panel != cfg.panel) { cfg.panel = panel; panel_mostrar(panel); }
    cfg_guardar();
}

static void en_cambiar_slot(GtkComboBox *c, gpointer p) {
    (void)p;
    if (cargando) return;
    int n = gtk_combo_box_get_active(c) + 1;
    if (n < 1 || n > 9) return;
    if (cfg.slot != n) { cfg.slot = n; cfg_guardar(); }
    mostrar_texto();
    listo(NULL);
}

static void en_guardar_texto(GtkWidget *w, gpointer p) {
    (void)w; (void)p;
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_acciones));
    GtkTextIter a, z;
    gtk_text_buffer_get_bounds(b, &a, &z);
    gchar *texto = gtk_text_buffer_get_text(b, &a, &z, FALSE);
    gchar **lineas = g_strsplit(texto, "\n", -1);
    Lista nueva = {0};
    for (int i = 0; lineas[i]; i++) {
        char *almohadilla = strchr(lineas[i], '#');
        if (almohadilla) *almohadilla = 0;
        gchar **trozos = g_strsplit(lineas[i], ";", -1);
        for (int j = 0; trozos[j]; j++) {
            char *t = g_strstrip(trozos[j]);
            if (!*t) continue;
            Accion ac; char err[200];
            if (!accion_leer(t, &ac, err, sizeof err)) {
                gchar *m = g_strdup_printf("Línea %d: %s", i + 1, err);
                aviso("No se ha guardado", m);
                g_free(m);
                g_strfreev(trozos); g_strfreev(lineas); g_free(texto); lista_vaciar(&nueva);
                return;
            }
            lista_anadir(&nueva, ac);
        }
        g_strfreev(trozos);
    }
    g_strfreev(lineas);
    g_free(texto);
    lista_vaciar(&secs[cfg.slot]);
    secs[cfg.slot] = nueva;
    guardar_secuencias();
    mostrar_texto();
    ui_logf("Secuencia %d guardada (%d acciones)", cfg.slot, nueva.n);
}

static void en_abrir_carpeta(GtkWidget *w, gpointer p) {
    (void)w; (void)p;
    gchar *uri = g_filename_to_uri(carpeta, NULL, NULL);
    if (uri && !gtk_show_uri_on_window(GTK_WINDOW(ventana), uri, GDK_CURRENT_TIME, NULL)) ui_logf("Carpeta de datos: %s", carpeta);
    g_free(uri);
}

static void en_creditos(GtkWidget *w, gpointer p) {
    (void)w; (void)p;
    aviso("AutoPrisión 2026 para Linux · versión " VERSION " para Prison Legacy",
          "Basado en AutoPrisión DX Profesional (2003-2007)\ncreado por Jagoba Los Arcos González · MoleSoft\nwww.moledj.com\n\n"
          "Versión 2026 (reescrita desde cero): Carlos Sanz, con ayuda de Claude (Anthropic).\n\n"
          "Igual que avisaba el original: úsalo bajo tu responsabilidad. Las normas del juego pueden no permitir "
          "automatizar acciones y se arriesga la cuenta.\n\nSaludos a los colegas :)");
}

static void en_grabar(GtkWidget *w, gpointer p) { (void)w; (void)p; alternar_grabacion(); }
static void en_reproducir(GtkWidget *w, gpointer p) { (void)w; (void)p; reproducir(false, 3000); }
static void en_bucle(GtkWidget *w, gpointer p) { (void)w; (void)p; reproducir(true, 3000); }
static void en_autoclick(GtkWidget *w, gpointer p) { (void)w; (void)p; auto_click(3000); }
static void en_autopelea(GtkWidget *w, gpointer p) { (void)w; (void)p; auto_pelea(3000); }
static void en_parar(GtkWidget *w, gpointer p) { (void)w; (void)p; parar_todo("Parado desde la ventana"); }

static void en_cerrar(GtkWidget *w, gpointer p) {
    (void)w; (void)p;
    parar_todo("Cerrando");
    gtk_main_quit();
}

// ---- construcción de la ventana

static GtkWidget *boton(GtkWidget *caja, const char *t, GCallback cb) {
    GtkWidget *b = gtk_button_new_with_label(t);
    g_signal_connect(b, "clicked", cb, NULL);
    gtk_box_pack_start(GTK_BOX(caja), b, FALSE, FALSE, 0);
    return b;
}

static GtkWidget *casilla(const char *t) {
    GtkWidget *c = gtk_check_button_new_with_label(t);
    g_signal_connect(c, "toggled", G_CALLBACK(en_cambio_ajustes), NULL);
    return c;
}

static GtkWidget *numero(int min, int max) {
    GtkWidget *n = gtk_spin_button_new_with_range(min, max, 10);
    g_signal_connect(n, "value-changed", G_CALLBACK(en_cambio_ajustes), NULL);
    return n;
}

static GtkWidget *etiqueta(const char *t) {
    GtkWidget *l = gtk_label_new(t);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    return l;
}

static GtkWidget *texto_con_scroll(GtkWidget **tv, int alto, bool editable) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
    gtk_widget_set_size_request(sw, -1, alto);
    *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(*tv), editable);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(*tv), editable);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(*tv), editable);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(*tv), 4);
    gtk_container_add(GTK_CONTAINER(sw), *tv);
    return sw;
}

static void construir(void) {
    ventana = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ventana), "AutoPrisión 2026 por Carlos Sanz · basado en AutoPrisión DX de MoleSoft");
    gtk_window_set_default_size(GTK_WINDOW(ventana), 840, 700);
    gtk_window_set_position(GTK_WINDOW(ventana), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(ventana), "input-mouse");
    g_signal_connect(ventana, "destroy", G_CALLBACK(en_cerrar), NULL);

    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(v), 12);
    gtk_container_add(GTK_CONTAINER(ventana), v);

    GtkWidget *ayuda = etiqueta(
        "TAB+1 autoclick · TAB+2+nº grabar (TAB termina) · TAB+3+nº reproducir hasta TAB · TAB+4 velocidad · TAB+5+nº una vez\n"
        "TAB+6 autopelea (Control+R cada turno) · TAB+7 guardar señal de turno bajo el ratón · TAB+8 borrarla · 0 quita el menú\n"
        "PARAR: tabulador (también termina la grabación). De reserva: F11 o Pausa. También para si mueves el ratón o el juego no está delante.\n"
        "F7 grabar · F8 una vez · F9 autoclick · F10 bucle · Mayús+F7 apunta el color bajo el ratón.");
    gtk_label_set_line_wrap(GTK_LABEL(ayuda), TRUE);
    gtk_box_pack_start(GTK_BOX(v), ayuda, FALSE, FALSE, 0);

    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    combo_slots = gtk_combo_box_text_new();
    g_signal_connect(combo_slots, "changed", G_CALLBACK(en_cambiar_slot), NULL);
    gtk_box_pack_start(GTK_BOX(h), combo_slots, FALSE, FALSE, 0);
    GtkWidget *formato = etiqueta(
        "ML,x,y clic izq. · MR,x,y clic der. · K,C+27 Control+R (C Control, S Mayús, A Alt, M Super; códigos de X11) · "
        "E,1500 esperar ms · P,x,y,RRGGBB,tol,máx-ms esperar color · # comentario");
    gtk_label_set_line_wrap(GTK_LABEL(formato), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(formato), "dim-label");
    gtk_box_pack_start(GTK_BOX(h), formato, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(v), h, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(v), texto_con_scroll(&txt_acciones, 170, true), TRUE, TRUE, 0);

    h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    boton(h, "Guardar cambios de la secuencia", G_CALLBACK(en_guardar_texto));
    boton(h, "Abrir carpeta", G_CALLBACK(en_abrir_carpeta));
    boton(h, "Acerca de… / créditos", G_CALLBACK(en_creditos));
    gtk_box_pack_start(GTK_BOX(v), h, FALSE, FALSE, 0);

    h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    boton(h, "Grabar (F7)", G_CALLBACK(en_grabar));
    boton(h, "Reproducir (F8)", G_CALLBACK(en_reproducir));
    boton(h, "Bucle (F10)", G_CALLBACK(en_bucle));
    boton(h, "Autoclick (F9)", G_CALLBACK(en_autoclick));
    boton(h, "Autopelea", G_CALLBACK(en_autopelea));
    GtkWidget *parar = boton(h, "PARAR (TAB)", G_CALLBACK(en_parar));
    gtk_style_context_add_class(gtk_widget_get_style_context(parar), "destructive-action");
    gtk_box_pack_start(GTK_BOX(v), h, FALSE, FALSE, 0);

    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 6);
    gtk_grid_set_column_spacing(GTK_GRID(g), 10);
    num_vel = numero(20, 10000);
    num_auto = numero(20, 60000);
    num_tol = numero(5, 500);
    chk_lock = casilla("Autoclick en posición fija");
    chk_juego = casilla("Solo con el juego delante. Nombre:");
    txt_proceso = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(txt_proceso), 14);
    g_signal_connect(txt_proceso, "changed", G_CALLBACK(en_cambio_ajustes), NULL);
    chk_pausas = casilla("Grabar las pausas largas");
    chk_mover = casilla("Parar si muevo el ratón más de (px):");
    chk_panel = casilla("Panel de estado encima del juego");
    gtk_grid_attach(GTK_GRID(g), etiqueta("Una acción cada (ms):"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), num_vel, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), etiqueta("Autoclick cada (ms):"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), num_auto, 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), chk_lock, 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), chk_juego, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), txt_proceso, 1, 1, 3, 1);
    gtk_grid_attach(GTK_GRID(g), chk_pausas, 4, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), chk_mover, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(g), num_tol, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(g), chk_panel, 4, 2, 1, 1);
    gtk_box_pack_start(GTK_BOX(v), g, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(v), texto_con_scroll(&txt_log, 160, false), TRUE, TRUE, 0);

    // Panel de estado encima del juego: no coge el foco ni los clics.
    panel_win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_keep_above(GTK_WINDOW(panel_win), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(panel_win), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(panel_win), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
    gtk_widget_set_size_request(panel_win, 820, 26);
    panel_lbl = gtk_label_new("");
    gtk_label_set_ellipsize(GTK_LABEL(panel_lbl), PANGO_ELLIPSIZE_END);
    gtk_container_add(GTK_CONTAINER(panel_win), panel_lbl);
    panel_css = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(panel_win), GTK_STYLE_PROVIDER(panel_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_style_context_add_provider(gtk_widget_get_style_context(panel_lbl), GTK_STYLE_PROVIDER(panel_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    GdkMonitor *mon = gdk_display_get_primary_monitor(gdk_display_get_default());
    if (!mon) mon = gdk_display_get_monitor(gdk_display_get_default(), 0);
    GdkRectangle r = {0, 0, 1280, 800};
    if (mon) gdk_monitor_get_geometry(mon, &r);
    gtk_window_move(GTK_WINDOW(panel_win), r.x + (r.width - 820) / 2, r.y);
    gtk_widget_set_opacity(panel_win, 0.88);
    gtk_widget_realize(panel_win);
    cairo_region_t *vacia = cairo_region_create();
    gdk_window_input_shape_combine_region(gtk_widget_get_window(panel_win), vacia, 0, 0);
    cairo_region_destroy(vacia);
}

// ---------------------------------------------------------------- prueba automática (sin ventana)

static int autotest(void) {
    int fallos = 0;
    char b[128], err[200];
    const char *buenas[] = {"ML,10,20", "MR,5,6", "K,27", "K,C+27", "K,C+S+A+M+38", "E,1500", "P,1,2,FF00AA,25", "P,1,2,00FF00,10,3000", NULL};
    for (int i = 0; buenas[i]; i++) {
        Accion a;
        if (!accion_leer(buenas[i], &a, err, sizeof err)) { printf("FALLO leer %s: %s\n", buenas[i], err); fallos++; continue; }
        accion_texto(&a, b, sizeof b);
        if (strcmp(b, buenas[i])) { printf("FALLO ida y vuelta %s -> %s\n", buenas[i], b); fallos++; }
    }
    const char *malas[] = {"X,1", "ML,1", "ML,a,2", "K,999", "K,C+", "E", "P,1,2,ZZZ", NULL};
    for (int i = 0; malas[i]; i++) {
        Accion a;
        if (accion_leer(malas[i], &a, err, sizeof err)) { printf("FALLO: «%s» debería no valer\n", malas[i]); fallos++; }
    }
    Accion pp = {0};
    accion_leer("p, 3 , 4 ,#00ff00", &pp, err, sizeof err);
    if (pp.t != COLOR || pp.rgb != 0x00FF00 || pp.tol != 25) { printf("FALLO P con espacios\n"); fallos++; }

    gchar *tmp = g_build_filename(g_get_tmp_dir(), "ap-secuencias-prueba.txt", NULL);
    g_file_set_contents(tmp, "10Secuencia 3=ML,1,2;E,100;K,C+27;\r\nSecuencia 1=MR,5,5;XX,1;\r\n", -1, NULL);
    Lista s[10] = {{0}};
    GPtrArray *errs = g_ptr_array_new_with_free_func(g_free);
    secuencias_leer(tmp, s, errs);
    if (s[3].n != 3 || s[1].n != 1 || errs->len != 1) { printf("FALLO leer secuencias (%d, %d, %u)\n", s[3].n, s[1].n, errs->len); fallos++; }
    secuencias_escribir(tmp, s, NULL);
    Lista s2[10] = {{0}};
    secuencias_leer(tmp, s2, NULL);
    if (s2[3].n != 3 || s2[1].n != 1) { printf("FALLO escribir secuencias\n"); fallos++; }
    g_remove(tmp); g_free(tmp);
    g_ptr_array_free(errs, TRUE);

    if (!es_juego("prisonlegacy|prisonlegacy.x86_64", "Prison") || es_juego("firefox", "Prison") || !es_juego("x|wine", "steam, Wine")) {
        printf("FALLO es_juego\n"); fallos++;
    }

    Display *d = XOpenDisplay(NULL);
    if (!d) { printf("FALLO: sin pantalla X11\n"); return 1; }
    XSetErrorHandler(error_x);
    int ev, er, may, men;
    if (!XTestQueryExtension(d, &ev, &er, &may, &men)) { printf("FALLO: sin XTest\n"); fallos++; }
    if (!XRecordQueryVersion(d, &may, &men)) { printf("FALLO: sin RECORD\n"); fallos++; }
    preparar_teclas(d);
    raton_mover(d, 123, 77);
    g_usleep(50000);
    int x, y; raton_pos(d, &x, &y);
    if (x != 123 || y != 77) { printf("FALLO ratón: (%d, %d)\n", x, y); fallos++; }
    int c;
    if (!color_en(d, 5, 5, &c)) { printf("FALLO color\n"); fallos++; } else printf("color en (5,5): #%06X\n", c);
    if (color_en(d, -1, 5, &c)) { printf("FALLO color fuera de pantalla\n"); fallos++; }
    printf("TAB=%d F11=%d 1=%d KP1=%d pulsada(TAB)=%d\n", K_TAB, K_F11, FILA[1], NUMERICO[1], pulsada(d, K_TAB));
    XCloseDisplay(d);

    if (fallos) printf("AUTOTEST: %d FALLOS\n", fallos);
    else printf("AUTOTEST OK\n");
    return fallos ? 1 : 0;
}

// ---------------------------------------------------------------- arranque

int main(int argc, char **argv) {
    XInitThreads();
    snprintf(carpeta, sizeof carpeta, "%s/AutoPrision 2026", g_get_home_dir());
    g_mkdir_with_parents(carpeta, 0755);
    if (argc > 1 && !strcmp(argv[1], "--autotest")) return autotest();

    gdk_set_allowed_backends("x11"); // en Wayland va por XWayland, como el juego
    if (!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "AutoPrisión 2026 necesita un escritorio gráfico con X11 o XWayland.\n");
        return 1;
    }
    XSetErrorHandler(error_x);
    dpy_ui = XOpenDisplay(NULL);
    if (!dpy_ui) { fprintf(stderr, "No se pudo abrir la pantalla X11.\n"); return 1; }
    preparar_teclas(dpy_ui);
    g_mutex_init(&motor.m);

    cfg_leer();
    construir();

    char ruta[PATH_MAX]; ruta_en_carpeta(ruta, sizeof ruta, "secuencias.txt");
    GPtrArray *errs = g_ptr_array_new_with_free_func(g_free);
    secuencias_leer(ruta, secs, errs);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_vel), cfg.velocidad);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_auto), cfg.autoclick_ms);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_tol), cfg.tolerancia);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_lock), cfg.autolock);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_juego), cfg.solo_en_juego);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_mover), cfg.parar_si_mueves);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_pausas), cfg.grabar_pausas);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_panel), cfg.panel);
    gtk_entry_set_text(GTK_ENTRY(txt_proceso), cfg.proceso);
    refrescar_slots();
    mostrar_texto();

    gtk_widget_show_all(ventana);
    panel_mostrar(cfg.panel);
    cargando = false;

    ui_log("AutoPrisión 2026 para Linux por Carlos Sanz · basado en AutoPrisión DX de Jagoba Los Arcos González (MoleSoft, moledj.com)");
    ui_logf("Carpeta de datos: %s", carpeta);
    for (guint i = 0; i < errs->len; i++) ui_log(g_ptr_array_index(errs, i));
    g_ptr_array_free(errs, TRUE);

    int ev, er, may, men;
    bool xtest = XTestQueryExtension(dpy_ui, &ev, &er, &may, &men), record = XRecordQueryVersion(dpy_ui, &may, &men);
    if (!xtest) ui_log("FALTA la extensión XTest de X11: no se podrá mover el ratón ni pulsar teclas.");
    if (!record) ui_log("FALTA la extensión RECORD de X11: no se podrá grabar (sí reproducir lo escrito a mano).");
    const char *sesion = g_getenv("XDG_SESSION_TYPE");
    if (sesion && !g_ascii_strcasecmp(sesion, "wayland"))
        ui_log("Estás en una sesión Wayland: AutoPrisión trabaja por XWayland, igual que el juego. "
               "Si no lee el TAB o no mueve el ratón, entra en una sesión «X11» o «Xorg» desde la pantalla de inicio.");
    if (xtest) ui_log("Listo. TAB abre el menú y TAB lo para todo (F11 y Pausa también paran).");
    listo(NULL);

    g_timeout_add_seconds(1, ui_reloj, NULL);
    g_thread_unref(g_thread_new("vigia", vigia_hilo, NULL));
    gtk_main();
    return 0;
}
