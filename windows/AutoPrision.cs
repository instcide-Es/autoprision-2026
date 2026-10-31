// AutoPrisión 2026 — reescritura de AutoPrisión DX Profesional para Prison Legacy.
// Original: Jagoba Los Arcos González · MoleSoft (2003-2007) · www.moledj.com
// Versión 2026: Carlos Sanz, con ayuda de Claude (Anthropic).
// Mismo formato de secuencias.txt y autoprision.ini que el original.
//
// Por qué no "pierde el teclado": el original leía las teclas con DirectInput 8, que deja de
// recibirlas cuando el juego coge el foco. Aquí las teclas van por RegisterHotKey (Windows las
// entrega siempre) y, además, el hilo que reproduce consulta la tecla de parada cada 15 ms con
// GetAsyncKeyState. Son dos caminos independientes: si uno falla, el otro para.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("AutoPrisión 2026")]
[assembly: System.Reflection.AssemblyProduct("AutoPrisión 2026")]
[assembly: System.Reflection.AssemblyDescription("Basado en AutoPrisión DX de Jagoba Los Arcos González (MoleSoft, www.moledj.com)")]
[assembly: System.Reflection.AssemblyCompany("MoleSoft (original) · Carlos Sanz (versión 2026)")]
[assembly: System.Reflection.AssemblyCopyright("AutoPrisión DX © Jagoba Los Arcos González (MoleSoft) · Versión 2026 para Prison Legacy: Carlos Sanz")]
[assembly: System.Reflection.AssemblyVersion("1.0.0.0")]

namespace AutoPrision2026
{
    static class W
    {
        [DllImport("user32.dll", SetLastError = true)] public static extern bool RegisterHotKey(IntPtr h, int id, uint mods, uint vk);
        [DllImport("user32.dll")] public static extern bool UnregisterHotKey(IntPtr h, int id);
        [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int vk);
        [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
        [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
        [DllImport("user32.dll", SetLastError = true)] public static extern uint SendInput(uint n, INPUT[] inp, int size);
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
        [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
        [DllImport("user32.dll", SetLastError = true)] public static extern IntPtr SetWindowsHookEx(int id, HookProc proc, IntPtr hMod, uint tid);
        [DllImport("user32.dll")] public static extern bool UnhookWindowsHookEx(IntPtr h);
        [DllImport("user32.dll")] public static extern IntPtr CallNextHookEx(IntPtr h, int code, IntPtr w, IntPtr l);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr GetModuleHandle(string name);
        [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
        [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
        [DllImport("gdi32.dll")] public static extern uint GetPixel(IntPtr dc, int x, int y);
        [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetKeyNameText(int lParam, StringBuilder sb, int size);
        [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);

        public delegate IntPtr HookProc(int code, IntPtr w, IntPtr l);

        [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
        [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr extra; }
        [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr extra; }
        [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
        [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public INPUTUNION u; }
        [StructLayout(LayoutKind.Sequential)] public struct KBDLLHOOKSTRUCT { public uint vk, scan, flags, time; public IntPtr extra; }
        [StructLayout(LayoutKind.Sequential)] public struct MSLLHOOKSTRUCT { public POINT pt; public uint data, flags, time; public IntPtr extra; }

        public const int VK_TAB = 0x09, VK_PAUSE = 0x13, VK_F7 = 0x76, VK_F8 = 0x77, VK_F9 = 0x78, VK_F10 = 0x79, VK_F11 = 0x7A;
        public const int VK_SHIFT = 0x10, VK_CONTROL = 0x11, VK_MENU = 0x12;

        public static bool Pulsada(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

        public static W.POINT Cursor() { POINT p; GetCursorPos(out p); return p; }

        public static int ColorEn(int x, int y)
        {
            IntPtr dc = GetDC(IntPtr.Zero);
            try
            {
                uint c = GetPixel(dc, x, y); // 0x00BBGGRR
                return (int)(((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF));
            }
            finally { ReleaseDC(IntPtr.Zero, dc); }
        }

        static uint pidCache = 0; static string nombreCache = "";
        public static string ProcesoDe(IntPtr hwnd)
        {
            uint pid; GetWindowThreadProcessId(hwnd, out pid);
            if (pid == pidCache) return nombreCache;
            string n = "";
            try { n = Process.GetProcessById((int)pid).ProcessName; } catch { }
            pidCache = pid; nombreCache = n;
            return n;
        }

        public static bool EsMiProceso(IntPtr hwnd)
        {
            uint pid; GetWindowThreadProcessId(hwnd, out pid);
            return pid == (uint)Process.GetCurrentProcess().Id;
        }
    }

    // ---------------------------------------------------------------- acciones

    enum Tipo { ClicIzq, ClicDer, Tecla, Espera, Color }

    class Accion
    {
        public Tipo T;
        public int X, Y, Sc, Ms, Rgb, Tol, Timeout;
        public bool Ctrl, Shift, Alt;

        // ML,x,y · MR,x,y · K,[C+][S+][A+]scancode · E,ms · P,x,y,RRGGBB[,tolerancia[,maxms]]
        public static Accion Parse(string s)
        {
            string[] p = s.Trim().Split(',');
            Accion a = new Accion();
            switch (p[0].Trim().ToUpperInvariant())
            {
                case "ML": case "MR":
                    if (p.Length != 3) throw new FormatException("se esperaba ML,x,y");
                    a.T = p[0].Trim().ToUpperInvariant() == "ML" ? Tipo.ClicIzq : Tipo.ClicDer;
                    a.X = int.Parse(p[1]); a.Y = int.Parse(p[2]);
                    break;
                case "K":
                    if (p.Length != 2) throw new FormatException("se esperaba K,código");
                    a.T = Tipo.Tecla;
                    foreach (string t in p[1].Split('+'))
                    {
                        string u = t.Trim().ToUpperInvariant();
                        if (u == "C") a.Ctrl = true;
                        else if (u == "S") a.Shift = true;
                        else if (u == "A") a.Alt = true;
                        else a.Sc = int.Parse(u);
                    }
                    if (a.Sc <= 0 || a.Sc > 0x1FF) throw new FormatException("código de tecla no válido");
                    break;
                case "E":
                    if (p.Length != 2) throw new FormatException("se esperaba E,milisegundos");
                    a.T = Tipo.Espera; a.Ms = Math.Max(0, int.Parse(p[1]));
                    break;
                case "P":
                    if (p.Length < 4) throw new FormatException("se esperaba P,x,y,RRGGBB");
                    a.T = Tipo.Color; a.X = int.Parse(p[1]); a.Y = int.Parse(p[2]);
                    a.Rgb = Convert.ToInt32(p[3].Trim().TrimStart('#'), 16);
                    a.Tol = p.Length > 4 ? int.Parse(p[4]) : 25;
                    a.Timeout = p.Length > 5 ? int.Parse(p[5]) : 0;
                    break;
                default:
                    throw new FormatException("acción desconocida «" + p[0].Trim() + "»");
            }
            return a;
        }

        public override string ToString()
        {
            switch (T)
            {
                case Tipo.ClicIzq: return "ML," + X + "," + Y;
                case Tipo.ClicDer: return "MR," + X + "," + Y;
                case Tipo.Tecla: return "K," + (Ctrl ? "C+" : "") + (Shift ? "S+" : "") + (Alt ? "A+" : "") + Sc;
                case Tipo.Espera: return "E," + Ms;
                default: return "P," + X + "," + Y + "," + Rgb.ToString("X6") + "," + Tol + (Timeout > 0 ? "," + Timeout : "");
            }
        }

        public string Describir()
        {
            switch (T)
            {
                case Tipo.ClicIzq: return "clic izquierdo en (" + X + ", " + Y + ")";
                case Tipo.ClicDer: return "clic derecho en (" + X + ", " + Y + ")";
                case Tipo.Tecla:
                    return "tecla " + (Ctrl ? "Ctrl+" : "") + (Shift ? "Mayús+" : "") + (Alt ? "Alt+" : "") + NombreTecla(Sc);
                case Tipo.Espera: return "esperar " + (Ms / 1000.0).ToString("0.0#") + " s";
                default:
                    return "esperar color #" + Rgb.ToString("X6") + " en (" + X + ", " + Y + ")" +
                           (Timeout > 0 ? ", máx. " + (Timeout / 1000.0).ToString("0.#") + " s" : "");
            }
        }

        public static string NombreTecla(int sc)
        {
            StringBuilder sb = new StringBuilder(64);
            int lp = ((sc & 0xFF) << 16) | ((sc & 0x100) != 0 ? 1 << 24 : 0);
            return W.GetKeyNameText(lp, sb, sb.Capacity) > 0 ? sb.ToString() : "código " + sc;
        }
    }

    class Secuencias
    {
        public readonly List<Accion>[] S = new List<Accion>[10]; // 1..9
        readonly string ruta;

        public Secuencias(string ruta) { this.ruta = ruta; for (int i = 1; i <= 9; i++) S[i] = new List<Accion>(); }

        public List<string> Cargar()
        {
            List<string> errores = new List<string>();
            if (!File.Exists(ruta)) return errores;
            // El original a veces deja basura delante («10Secuencia 3=...»): se busca el patrón en la línea.
            Regex re = new Regex(@"Secuencia\s*([1-9])\s*=(.*)$");
            foreach (string linea in File.ReadAllLines(ruta, Encoding.Default))
            {
                Match m = re.Match(linea);
                if (!m.Success) continue;
                int n = int.Parse(m.Groups[1].Value);
                S[n].Clear();
                foreach (string trozo in m.Groups[2].Value.Split(';'))
                {
                    if (trozo.Trim().Length == 0) continue;
                    try { S[n].Add(Accion.Parse(trozo)); }
                    catch (Exception ex) { errores.Add("Secuencia " + n + ": «" + trozo.Trim() + "» ignorada (" + ex.Message + ")"); }
                }
            }
            return errores;
        }

        public void Guardar()
        {
            StringBuilder sb = new StringBuilder();
            for (int i = 1; i <= 9; i++)
            {
                sb.Append("Secuencia ").Append(i).Append('=');
                foreach (Accion a in S[i]) sb.Append(a).Append(';');
                sb.Append("\r\n");
            }
            File.WriteAllText(ruta, sb.ToString(), Encoding.Default);
        }
    }

    // ---------------------------------------------------------------- configuración

    class Config
    {
        readonly string ruta;
        public int Velocidad = 200, AutoClickMs = 1000, Tolerancia = 40, Slot = 1, Color;
        public bool AutoLock, SoloEnJuego = true, PararSiMueves = true, GrabarPausas = true, Panel = true;
        public string Proceso = "PrisonLegacy";
        public int PeleaMs = 1500;
        public string PeleaSenal = ""; // "x,y,RRGGBB" o vacío

        public static bool LeerSenal(string s, out int x, out int y, out int rgb)
        {
            x = y = rgb = 0;
            string[] p = (s ?? "").Split(',');
            return p.Length == 3 && int.TryParse(p[0], out x) && int.TryParse(p[1], out y) &&
                   int.TryParse(p[2].Trim(), System.Globalization.NumberStyles.HexNumber, null, out rgb);
        }

        public Config(string ruta)
        {
            this.ruta = ruta;
            if (!File.Exists(ruta)) return;
            Dictionary<string, string> d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (string l in File.ReadAllLines(ruta, Encoding.Default))
            {
                int i = l.IndexOf('=');
                if (i > 0) d[l.Substring(0, i).Trim()] = l.Substring(i + 1).Trim();
            }
            Color = Int(d, "color", Color);
            Velocidad = Math.Max(20, Int(d, "reproduccion", Velocidad));
            AutoLock = Int(d, "autolock", 0) != 0;
            AutoClickMs = Math.Max(20, Int(d, "autoclick_ms", AutoClickMs));
            Tolerancia = Math.Max(5, Int(d, "tolerancia_raton", Tolerancia));
            Slot = Math.Min(9, Math.Max(1, Int(d, "secuencia_activa", Slot)));
            SoloEnJuego = Int(d, "solo_en_juego", 1) != 0;
            PararSiMueves = Int(d, "parar_si_mueves_raton", 1) != 0;
            GrabarPausas = Int(d, "grabar_pausas", 1) != 0;
            Panel = Int(d, "panel", 1) != 0;
            string v;
            if (d.TryGetValue("proceso", out v) && v.Length > 0) Proceso = v;
            PeleaMs = Math.Max(300, Int(d, "autopelea_ms", PeleaMs));
            if (d.TryGetValue("autopelea_senal", out v)) PeleaSenal = v;
        }

        static int Int(Dictionary<string, string> d, string k, int def)
        {
            string v; int r;
            return d.TryGetValue(k, out v) && int.TryParse(v, out r) ? r : def;
        }

        public void Guardar()
        {
            string[] l = {
                "[configuracion]",
                "color=" + Color,
                "auto=1",
                "reproduccion=" + Velocidad,
                "autolock=" + (AutoLock ? 1 : 0),
                "autoclick_ms=" + AutoClickMs,
                "secuencia_activa=" + Slot,
                "solo_en_juego=" + (SoloEnJuego ? 1 : 0),
                "proceso=" + Proceso,
                "parar_si_mueves_raton=" + (PararSiMueves ? 1 : 0),
                "tolerancia_raton=" + Tolerancia,
                "grabar_pausas=" + (GrabarPausas ? 1 : 0),
                "panel=" + (Panel ? 1 : 0),
                "autopelea_ms=" + PeleaMs,
                "autopelea_senal=" + PeleaSenal,
            };
            try { File.WriteAllLines(ruta, l, Encoding.Default); } catch { }
        }

        public bool JuegoDelante()
        {
            string n = W.ProcesoDe(W.GetForegroundWindow());
            foreach (string p in Proceso.Split(','))
                if (p.Trim().Length > 0 && string.Equals(n, p.Trim(), StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }
    }

    // ---------------------------------------------------------------- motor (hilo propio)

    class Detenido : Exception { public Detenido(string m) : base(m) { } }

    class Motor
    {
        readonly Config cfg;
        Thread hilo;
        volatile bool parar;
        volatile string motivoParar;
        bool vigilarRaton; int esperadoX, esperadoY;

        public event Action<string> Estado;      // texto del panel mientras trabaja
        public event Action<string> Terminado;   // motivo

        public Motor(Config cfg) { this.cfg = cfg; }

        public bool Ocupado { get { Thread h = hilo; return h != null && h.IsAlive; } }
        public volatile int FinTick = int.MinValue / 2; // cuándo terminó el último trabajo

        public void Parar(string motivo) { motivoParar = motivo; parar = true; }

        public void Reproducir(int slot, List<Accion> acciones, bool bucle, int retrasoMs)
        {
            Accion[] lista = acciones.ToArray();
            Lanzar(delegate
            {
                if (lista.Length == 0) throw new Detenido("La secuencia " + slot + " está vacía");
                int vuelta = 0;
                do
                {
                    vuelta++;
                    for (int i = 0; i < lista.Length; i++)
                    {
                        Avisar((bucle ? "BUCLE sec. " + slot + " · vuelta " + vuelta : "Reproduciendo sec. " + slot) +
                               " · " + (i + 1) + "/" + lista.Length + " · F11 para");
                        Comprobar();
                        Ejecutar(lista[i]);
                        Esperar(cfg.Velocidad);
                    }
                } while (bucle);
                return "Reproducción terminada (sec. " + slot + ")";
            }, retrasoMs);
        }

        public void AutoClick(int retrasoMs)
        {
            Lanzar(delegate
            {
                bool fijo = cfg.AutoLock;
                W.POINT p = W.Cursor();
                long n = 0;
                while (true)
                {
                    if (fijo) MoverA(p.X, p.Y);
                    Comprobar();
                    Clic(true);
                    n++;
                    Avisar("AUTOCLICK " + (fijo ? "fijo en (" + p.X + ", " + p.Y + ")" : "donde está el ratón") + " · " + n + " clics · F9/F11 para");
                    Esperar(cfg.AutoClickMs);
                }
            }, retrasoMs);
        }

        // Autopelea (turno continuo): pulsa Ctrl+R, que en Prison Legacy repite las acciones del turno
        // anterior; el juego solo lo acepta en tu turno (también en un TURNO DOBLE). Tú haces el primer
        // turno a mano y esto repite. Con «señal de turno» (un punto de pantalla que solo tiene ese color
        // cuando es tu turno) pulsa justo al empezar tu turno; sin ella, pulsa cada PeleaMs.
        public void AutoPelea(int retrasoMs)
        {
            int sx = 0, sy = 0, srgb = 0;
            bool conSenal = Config.LeerSenal(cfg.PeleaSenal, out sx, out sy, out srgb);
            Accion ctrlR = new Accion { T = Tipo.Tecla, Ctrl = true, Sc = 0x13 };
            Lanzar(delegate
            {
                long n = 0;
                while (true)
                {
                    if (conSenal)
                    {
                        Avisar("AUTOPELEA · esperando tu turno · " + n + " turnos · TAB para");
                        while (!Parecido(W.ColorEn(sx, sy), srgb, 30)) Esperar(100);
                    }
                    Comprobar();
                    Ejecutar(ctrlR);
                    n++;
                    Avisar("AUTOPELEA · Ctrl+R (" + n + ")" + (conSenal ? "" : " cada " + cfg.PeleaMs + " ms") + " · TAB para");
                    if (conSenal)
                    {
                        // Si la señal sigue ahí pasado el tiempo es un turno doble (o no entró): se repite.
                        Stopwatch sw = Stopwatch.StartNew();
                        while (Parecido(W.ColorEn(sx, sy), srgb, 30) && sw.ElapsedMilliseconds < cfg.PeleaMs * 2) Esperar(100);
                    }
                    else Esperar(cfg.PeleaMs);
                }
            }, retrasoMs);
        }

        void Lanzar(Func<string> trabajo, int retrasoMs)
        {
            if (Ocupado) return;
            parar = false; motivoParar = null; vigilarRaton = false;
            hilo = new Thread(delegate ()
            {
                string fin;
                try
                {
                    for (int r = retrasoMs; r > 0; r -= 100)
                    {
                        if (parar) throw new Detenido(motivoParar ?? "Parado");
                        Avisar("Empieza en " + ((r + 999) / 1000) + " s: cambia al juego");
                        Thread.Sleep(100);
                    }
                    Sonido.Inicio();
                    fin = trabajo();
                }
                catch (Detenido d) { fin = d.Message; }
                catch (Exception ex) { fin = "Error: " + ex.Message; }
                vigilarRaton = false;
                FinTick = Environment.TickCount;
                Sonido.Fin();
                Action<string> t = Terminado;
                if (t != null) t(fin);
            });
            hilo.IsBackground = true;
            hilo.Priority = ThreadPriority.AboveNormal;
            hilo.Start();
        }

        void Avisar(string s) { Action<string> e = Estado; if (e != null) e(s); }

        // Se llama constantemente: cualquiera de estas condiciones para el trabajo en menos de 15 ms.
        void Comprobar()
        {
            if (parar) throw new Detenido(motivoParar ?? "Parado");
            if (W.Pulsada(W.VK_TAB)) throw new Detenido("Parado con el tabulador");
            if (W.Pulsada(W.VK_F11) || W.Pulsada(W.VK_PAUSE)) throw new Detenido("Parado con la tecla de parada");
            if (cfg.SoloEnJuego && !cfg.JuegoDelante()) throw new Detenido("Parado: el juego no está en primer plano");
            if (cfg.PararSiMueves && vigilarRaton)
            {
                W.POINT c = W.Cursor();
                int dx = c.X - esperadoX, dy = c.Y - esperadoY;
                if (dx * dx + dy * dy > cfg.Tolerancia * cfg.Tolerancia) throw new Detenido("Parado: has movido el ratón");
            }
        }

        void Esperar(int ms)
        {
            Stopwatch sw = Stopwatch.StartNew();
            while (true)
            {
                Comprobar();
                long resta = ms - sw.ElapsedMilliseconds;
                if (resta <= 0) return;
                Thread.Sleep((int)Math.Min(15, resta));
            }
        }

        void MoverA(int x, int y)
        {
            W.SetCursorPos(x, y);
            esperadoX = x; esperadoY = y; vigilarRaton = true;
        }

        void Ejecutar(Accion a)
        {
            switch (a.T)
            {
                case Tipo.ClicIzq:
                case Tipo.ClicDer:
                    MoverA(a.X, a.Y);
                    Esperar(30); // el juego necesita ver el ratón encima antes del clic
                    Clic(a.T == Tipo.ClicIzq);
                    break;
                case Tipo.Tecla:
                    Teclear(a);
                    break;
                case Tipo.Espera:
                    Esperar(a.Ms);
                    break;
                case Tipo.Color:
                    Stopwatch sw = Stopwatch.StartNew();
                    while (!Parecido(W.ColorEn(a.X, a.Y), a.Rgb, a.Tol))
                    {
                        if (a.Timeout > 0 && sw.ElapsedMilliseconds > a.Timeout)
                            throw new Detenido("Parado: no apareció el color #" + a.Rgb.ToString("X6") + " en (" + a.X + ", " + a.Y + ")");
                        Esperar(50);
                    }
                    break;
            }
        }

        static bool Parecido(int a, int b, int tol)
        {
            return Math.Abs(((a >> 16) & 0xFF) - ((b >> 16) & 0xFF)) <= tol &&
                   Math.Abs(((a >> 8) & 0xFF) - ((b >> 8) & 0xFF)) <= tol &&
                   Math.Abs((a & 0xFF) - (b & 0xFF)) <= tol;
        }

        // Pulsar y soltar van siempre emparejados (finally) para no dejar nunca un botón o tecla «pegados».
        static void Clic(bool izquierdo)
        {
            Raton(izquierdo ? 0x0002u : 0x0008u);
            try { Thread.Sleep(30); }
            finally { Raton(izquierdo ? 0x0004u : 0x0010u); }
        }

        static void Teclear(Accion a)
        {
            List<int> pulsadas = new List<int>();
            try
            {
                if (a.Ctrl) { Tecla(0x1D, false); pulsadas.Add(0x1D); }
                if (a.Shift) { Tecla(0x2A, false); pulsadas.Add(0x2A); }
                if (a.Alt) { Tecla(0x38, false); pulsadas.Add(0x38); }
                Tecla(a.Sc, false); pulsadas.Add(a.Sc);
                Thread.Sleep(40);
            }
            finally
            {
                for (int i = pulsadas.Count - 1; i >= 0; i--) { Tecla(pulsadas[i], true); Thread.Sleep(5); }
            }
        }

        static void Raton(uint flags)
        {
            W.INPUT[] i = new W.INPUT[1];
            i[0].type = 0;
            i[0].u.mi.dwFlags = flags;
            W.SendInput(1, i, Marshal.SizeOf(typeof(W.INPUT)));
        }

        // Por código de exploración (como DirectInput): el juego recibe la tecla física, sin depender del idioma.
        static void Tecla(int sc, bool soltar)
        {
            W.INPUT[] i = new W.INPUT[1];
            i[0].type = 1;
            i[0].u.ki.wScan = (ushort)(sc & 0xFF);
            i[0].u.ki.dwFlags = 0x0008u | (soltar ? 0x0002u : 0) | ((sc & 0x100) != 0 ? 0x0001u : 0);
            W.SendInput(1, i, Marshal.SizeOf(typeof(W.INPUT)));
        }
    }

    static class Sonido
    {
        static void Pitar(params int[] fd)
        {
            ThreadPool.QueueUserWorkItem(delegate
            {
                try { for (int i = 0; i + 1 < fd.Length; i += 2) Console.Beep(fd[i], fd[i + 1]); } catch { }
            });
        }
        public static void Inicio() { Pitar(1200, 90); }
        public static void Fin() { Pitar(600, 120, 400, 160); }
        public static void Grabar() { Pitar(900, 70, 900, 70); }
        public static void Anotar() { Pitar(1500, 40); }
    }

    // ---------------------------------------------------------------- grabación (ganchos de bajo nivel)

    class Grabador
    {
        readonly Config cfg;
        readonly W.HookProc procRaton, procTeclado; // referencias guardadas: si el GC se los lleva, el gancho muere
        IntPtr gRaton, gTeclado;
        readonly HashSet<uint> abajo = new HashSet<uint>();
        Stopwatch reloj;
        public List<Accion> Acciones;
        public event Action<Accion> Anotada;

        static readonly HashSet<uint> nuestras = new HashSet<uint> { W.VK_TAB, W.VK_F7, W.VK_F8, W.VK_F9, W.VK_F10, W.VK_F11, W.VK_PAUSE };
        static readonly HashSet<uint> modificadores = new HashSet<uint> { 0x10, 0x11, 0x12, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x5B, 0x5C };

        public Grabador(Config cfg)
        {
            this.cfg = cfg;
            procRaton = RatonLL;
            procTeclado = TecladoLL;
        }

        public bool Activo { get { return Acciones != null; } }

        public bool Empezar()
        {
            Acciones = new List<Accion>();
            abajo.Clear();
            reloj = null;
            IntPtr mod = W.GetModuleHandle(null);
            gRaton = W.SetWindowsHookEx(14, procRaton, mod, 0);
            gTeclado = W.SetWindowsHookEx(13, procTeclado, mod, 0);
            if (gRaton == IntPtr.Zero || gTeclado == IntPtr.Zero) { Terminar(); return false; }
            return true;
        }

        public List<Accion> Terminar()
        {
            if (gRaton != IntPtr.Zero) W.UnhookWindowsHookEx(gRaton);
            if (gTeclado != IntPtr.Zero) W.UnhookWindowsHookEx(gTeclado);
            gRaton = gTeclado = IntPtr.Zero;
            List<Accion> r = Acciones;
            Acciones = null;
            return r;
        }

        public void AnotarColor()
        {
            if (!Activo) return;
            W.POINT p = W.Cursor();
            Accion a = new Accion { T = Tipo.Color, X = p.X, Y = p.Y, Rgb = W.ColorEn(p.X, p.Y), Tol = 25 };
            Anotar(a);
        }

        bool SeGraba(IntPtr hwnd)
        {
            if (W.EsMiProceso(hwnd)) return false;            // clics y teclas en esta ventana, no
            return !cfg.SoloEnJuego || cfg.JuegoDelante();
        }

        void Anotar(Accion a)
        {
            if (cfg.GrabarPausas && reloj != null)
            {
                long hueco = reloj.ElapsedMilliseconds - cfg.Velocidad;
                if (hueco > 800) Acciones.Add(new Accion { T = Tipo.Espera, Ms = (int)(Math.Round(hueco / 100.0) * 100) });
            }
            reloj = Stopwatch.StartNew();
            Acciones.Add(a);
            Action<Accion> e = Anotada;
            if (e != null) e(a);
        }

        IntPtr RatonLL(int code, IntPtr w, IntPtr l)
        {
            if (code >= 0 && Activo && (w.ToInt32() == 0x201 || w.ToInt32() == 0x204))
            {
                W.MSLLHOOKSTRUCT m = (W.MSLLHOOKSTRUCT)Marshal.PtrToStructure(l, typeof(W.MSLLHOOKSTRUCT));
                if ((m.flags & 1) == 0 && SeGraba(W.WindowFromPoint(m.pt)))
                    Anotar(new Accion { T = w.ToInt32() == 0x201 ? Tipo.ClicIzq : Tipo.ClicDer, X = m.pt.X, Y = m.pt.Y });
            }
            return W.CallNextHookEx(gRaton, code, w, l);
        }

        IntPtr TecladoLL(int code, IntPtr w, IntPtr l)
        {
            if (code >= 0 && Activo)
            {
                W.KBDLLHOOKSTRUCT k = (W.KBDLLHOOKSTRUCT)Marshal.PtrToStructure(l, typeof(W.KBDLLHOOKSTRUCT));
                int msg = w.ToInt32();
                bool inyectada = (k.flags & 0x10) != 0;
                if (msg == 0x101 || msg == 0x105) abajo.Remove(k.vk);
                else if ((msg == 0x100 || msg == 0x104) && !inyectada && abajo.Add(k.vk)
                         && !nuestras.Contains(k.vk) && !modificadores.Contains(k.vk)
                         && SeGraba(W.GetForegroundWindow()))
                {
                    Anotar(new Accion
                    {
                        T = Tipo.Tecla,
                        Sc = (int)(k.scan & 0xFF) | ((k.flags & 1) != 0 ? 0x100 : 0),
                        Ctrl = W.Pulsada(W.VK_CONTROL),
                        Shift = W.Pulsada(W.VK_SHIFT),
                        Alt = W.Pulsada(W.VK_MENU) || (k.flags & 0x20) != 0,
                    });
                }
            }
            return W.CallNextHookEx(gTeclado, code, w, l);
        }
    }

    // ---------------------------------------------------------------- panel encima del juego

    class PanelEstado : Form
    {
        readonly Label lbl;

        public PanelEstado()
        {
            FormBorderStyle = FormBorderStyle.None;
            ShowInTaskbar = false;
            StartPosition = FormStartPosition.Manual;
            TopMost = true;
            Opacity = 0.85;
            Size = new Size(700, 28);
            Rectangle r = Screen.PrimaryScreen.Bounds;
            Location = new Point(r.X + (r.Width - Width) / 2, r.Y);
            lbl = new Label { Dock = DockStyle.Fill, ForeColor = System.Drawing.Color.White, TextAlign = ContentAlignment.MiddleCenter, Font = new Font("Segoe UI", 10f, FontStyle.Bold) };
            Controls.Add(lbl);
            Poner("AutoPrisión 2026 por Carlos Sanz · listo", System.Drawing.Color.FromArgb(40, 40, 40));
        }

        // Nunca coge el foco ni los clics: el juego se cierra si pierde el primer plano.
        protected override bool ShowWithoutActivation { get { return true; } }
        protected override CreateParams CreateParams
        {
            get
            {
                CreateParams cp = base.CreateParams;
                cp.ExStyle |= 0x08000000 /*NOACTIVATE*/ | 0x80 /*TOOLWINDOW*/ | 0x20 /*TRANSPARENT*/ | 0x8 /*TOPMOST*/ | 0x80000 /*LAYERED*/;
                return cp;
            }
        }

        public void Poner(string texto, Color fondo) { lbl.Text = texto; BackColor = fondo; }

        public void SubirArriba() { W.SetWindowPos(Handle, new IntPtr(-1), 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0010); }
    }

    // ---------------------------------------------------------------- ventana principal

    class Principal : Form
    {
        readonly string carpeta = AppDomain.CurrentDomain.BaseDirectory;
        readonly Config cfg;
        readonly Secuencias secs;
        readonly Motor motor;
        readonly Grabador grabador;
        readonly PanelEstado panel = new PanelEstado();
        readonly System.Windows.Forms.Timer reloj = new System.Windows.Forms.Timer { Interval = 500 };
        DateTime ocultarEn = DateTime.MaxValue;
        int slotGrabando;

        ListBox lstSlots, lstLog;
        TextBox txtAcciones, txtProceso;
        NumericUpDown numVel, numAuto, numTol;
        CheckBox chkLock, chkJuego, chkMover, chkPausas, chkPanel;
        bool cargando;

        static readonly Color Gris = Color.FromArgb(40, 40, 40), Verde = Color.FromArgb(20, 110, 40),
                              Rojo = Color.FromArgb(170, 30, 30), Azul = Color.FromArgb(30, 70, 150);

        public Principal()
        {
            cfg = new Config(Path.Combine(carpeta, "autoprision.ini"));
            secs = new Secuencias(Path.Combine(carpeta, "secuencias.txt"));
            motor = new Motor(cfg);
            grabador = new Grabador(cfg);

            Text = "AutoPrisión 2026 por Carlos Sanz · basado en AutoPrisión DX de MoleSoft";
            Icon = SystemIcons.Application;
            ClientSize = new Size(780, 600);
            MinimumSize = new Size(700, 560);
            Font = new Font("Segoe UI", 9f);
            Construir();

            foreach (string e in secs.Cargar()) Log(e);
            cargando = true;
            numVel.Value = Math.Min(numVel.Maximum, cfg.Velocidad);
            numAuto.Value = Math.Min(numAuto.Maximum, cfg.AutoClickMs);
            numTol.Value = Math.Min(numTol.Maximum, cfg.Tolerancia);
            chkLock.Checked = cfg.AutoLock; chkJuego.Checked = cfg.SoloEnJuego; chkMover.Checked = cfg.PararSiMueves;
            chkPausas.Checked = cfg.GrabarPausas; chkPanel.Checked = cfg.Panel; txtProceso.Text = cfg.Proceso;
            cargando = false;
            RefrescarSlots();
            lstSlots.SelectedIndex = cfg.Slot - 1;

            motor.Estado += delegate (string s) { EnUI(delegate { if (motor.Ocupado) Mostrar(s, Verde, false); }); };
            motor.Terminado += delegate (string s) { EnUI(delegate { Log(s); Listo(s); }); };
            grabador.Anotada += delegate (Accion a)
            {
                Sonido.Anotar();
                Mostrar("GRABANDO sec. " + slotGrabando + " · " + grabador.Acciones.Count + " acciones · TAB termina", Rojo, false);
                Log("  + " + a.Describir());
            };

            reloj.Tick += delegate
            {
                if (panel.Visible) panel.SubirArriba();
                if (menu != Modo.Cerrado && DateTime.Now > menuHasta) CerrarMenu();
                if (DateTime.Now > ocultarEn && menu == Modo.Cerrado && !motor.Ocupado && !grabador.Activo) Ocultar();
            };
            reloj.Start();
            ArrancarVigia();
        }

        // ---- menú del tabulador (como el original)
        //
        // Un hilo propio mira el teclado cada 10 ms con GetAsyncKeyState. No depende del foco, ni de
        // DirectInput, ni de ganchos que Windows pueda quitar: por eso no «pierde el teclado».
        // El tabulador y los números siguen llegando al juego, igual que con el programa viejo.

        enum Modo { Cerrado, Principal, Grabar, Reproducir, Bucle, Velocidad }
        Modo menu = Modo.Cerrado;
        DateTime menuHasta;

        void ArrancarVigia()
        {
            List<int> vks = new List<int> { W.VK_TAB };
            for (int i = 0; i <= 9; i++) { vks.Add(0x30 + i); vks.Add(0x60 + i); } // fila de números y teclado numérico
            Thread vigia = new Thread(delegate ()
            {
                bool[] antes = new bool[256];
                while (true)
                {
                    foreach (int vk in vks)
                    {
                        bool ahora = W.Pulsada(vk);
                        if (ahora && !antes[vk]) { int t = vk; EnUI(delegate { TeclaMenu(t); }); }
                        antes[vk] = ahora;
                    }
                    Thread.Sleep(10);
                }
            });
            vigia.IsBackground = true;
            vigia.Priority = ThreadPriority.AboveNormal;
            vigia.Start();
        }

        void TeclaMenu(int vk)
        {
            if (vk == W.VK_TAB)
            {
                if (motor.Ocupado || grabador.Activo) { CerrarMenu(); PararTodo("Parado con el tabulador"); }
                else if (Environment.TickCount - motor.FinTick < 700) { } // ese tabulador ya paró la reproducción
                else if (menu != Modo.Cerrado) CerrarMenu();
                else AbrirMenu(Modo.Principal);
                return;
            }
            if (menu == Modo.Cerrado) return;
            int n = vk >= 0x60 ? vk - 0x60 : vk - 0x30;
            if (n == 0) { CerrarMenu(); return; }
            switch (menu)
            {
                case Modo.Principal:
                    if (n == 1) { CerrarMenu(); AutoClick(0); }
                    else if (n == 2) AbrirMenu(Modo.Grabar);
                    else if (n == 3) AbrirMenu(Modo.Bucle);
                    else if (n == 4) AbrirMenu(Modo.Velocidad);
                    else if (n == 5) AbrirMenu(Modo.Reproducir);
                    else if (n == 6) { CerrarMenu(); AutoPelea(0); }
                    else if (n == 7) { CerrarMenu(); GuardarSenal(); }
                    else if (n == 8) { CerrarMenu(); cfg.PeleaSenal = ""; cfg.Guardar(); Log("Señal de turno borrada: la autopelea pulsará Ctrl+R cada " + cfg.PeleaMs + " ms"); Listo("Señal de turno borrada"); }
                    break;
                case Modo.Grabar:
                    CerrarMenu(); Seleccionar(n); EmpezarGrabacion();
                    break;
                case Modo.Reproducir:
                case Modo.Bucle:
                    bool bucle = menu == Modo.Bucle;
                    CerrarMenu(); Seleccionar(n); Reproducir(bucle, 0);
                    break;
                case Modo.Velocidad:
                    if (n == 1) numVel.Value = Math.Max(numVel.Minimum, numVel.Value - 100);
                    else if (n == 2) numVel.Value = Math.Min(numVel.Maximum, numVel.Value + 100);
                    else if (n == 3) numAuto.Value = Math.Max(numAuto.Minimum, numAuto.Value - 100);
                    else if (n == 4) numAuto.Value = Math.Min(numAuto.Maximum, numAuto.Value + 100);
                    AbrirMenu(Modo.Velocidad);
                    break;
            }
        }

        void AbrirMenu(Modo m)
        {
            menu = m;
            menuHasta = DateTime.Now.AddSeconds(8);
            Sonido.Anotar();
            string con = "";
            for (int i = 1; i <= 9; i++) if (secs.S[i].Count > 0) con += (con.Length > 0 ? "," : "") + i;
            switch (m)
            {
                case Modo.Principal: Mostrar("1 Autoclick · 2 Grabar · 3 Reproducir · 4 Velocidad · 5 Una vez · 6 Autopelea · 7/8 Señal turno · 0 Quitar", Azul, false); break;
                case Modo.Grabar: Mostrar("GRABAR en la secuencia nº… (1-9)   con datos: " + (con.Length > 0 ? con : "ninguna"), Azul, false); break;
                case Modo.Reproducir: Mostrar("REPRODUCIR UNA VEZ la secuencia nº… (1-9)   con datos: " + (con.Length > 0 ? con : "ninguna"), Azul, false); break;
                case Modo.Bucle: Mostrar("REPRODUCIR (hasta TAB) la secuencia nº… (1-9)   con datos: " + (con.Length > 0 ? con : "ninguna"), Azul, false); break;
                case Modo.Velocidad:
                    Mostrar("Una acción cada " + cfg.Velocidad + " ms (1 más rápido · 2 más lento) · autoclick " + cfg.AutoClickMs + " ms (3 · 4)", Azul, false);
                    break;
            }
        }

        void CerrarMenu()
        {
            if (menu == Modo.Cerrado) return;
            menu = Modo.Cerrado;
            Listo(null);
        }

        void Seleccionar(int n) { if (n >= 1 && n <= 9) lstSlots.SelectedIndex = n - 1; }

        void Construir()
        {
            Label ayuda = new Label
            {
                Location = new Point(12, 8), Size = new Size(756, 64), Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
                Text = "TAB+1 autoclick · TAB+2+nº grabar (TAB termina) · TAB+3+nº reproducir hasta TAB · TAB+4 velocidad · TAB+5+nº una vez\r\n" +
                       "TAB+6 autopelea (Ctrl+R cada turno) · TAB+7 guardar señal de turno bajo el ratón · TAB+8 borrarla · 0 quita el menú\r\n" +
                       "PARAR: tabulador (también termina la grabación). De reserva: F11 o Pausa. También para si mueves el ratón o el juego no está delante.\r\n" +
                       "No hagas clic en esta ventana con el juego abierto: el juego puede cerrarse si pierde el primer plano."
            };
            Controls.Add(ayuda);

            lstSlots = new ListBox { Location = new Point(12, 80), Size = new Size(200, 170), IntegralHeight = false };
            lstSlots.SelectedIndexChanged += delegate { CambiarSlot(lstSlots.SelectedIndex + 1); };
            Controls.Add(lstSlots);

            txtAcciones = new TextBox
            {
                Location = new Point(222, 80), Size = new Size(546, 170), Multiline = true, ScrollBars = ScrollBars.Vertical,
                Font = new Font("Consolas", 9.5f), WordWrap = false, Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };
            Controls.Add(txtAcciones);

            Boton("Acerca de… / créditos", 12, 256, 200, delegate { Creditos(); });
            Button btnGuardar = Boton("Guardar cambios de la secuencia", 222, 256, 230, delegate { GuardarTexto(); });
            btnGuardar.Anchor = AnchorStyles.Top | AnchorStyles.Left;
            Label formato = new Label
            {
                Location = new Point(460, 254), Size = new Size(308, 34), ForeColor = Color.DimGray, Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
                Text = "ML,x,y clic izq. · MR,x,y clic der. · K,C+30 Ctrl+A · E,1500 esperar ms · P,x,y,RRGGBB,tol,máx-ms esperar color · # comentario"
            };
            Controls.Add(formato);

            int y = 300;
            Boton("Grabar (F7)", 12, y, 120, delegate { AlternarGrabacion(); });
            Boton("Reproducir (F8)", 138, y, 120, delegate { Reproducir(false, 3000); });
            Boton("Bucle (F10)", 264, y, 120, delegate { Reproducir(true, 3000); });
            Boton("Autoclick (F9)", 390, y, 120, delegate { AutoClick(3000); });
            Boton("Autopelea", 516, y, 120, delegate { AutoPelea(3000); });
            Button parar = Boton("PARAR (TAB)", 642, y, 126, delegate { PararTodo("Parado desde la ventana"); });
            parar.ForeColor = Color.DarkRed; parar.Font = new Font(Font, FontStyle.Bold);

            y = 340;
            Etiqueta("Una acción cada (ms):", 12, y); numVel = Numero(160, y, 20, 10000, delegate { cfg.Velocidad = (int)numVel.Value; });
            Etiqueta("Autoclick cada (ms):", 250, y); numAuto = Numero(390, y, 20, 60000, delegate { cfg.AutoClickMs = (int)numAuto.Value; });
            chkLock = Casilla("Autoclick en posición fija", 480, y, delegate { cfg.AutoLock = chkLock.Checked; });

            y = 368;
            chkJuego = Casilla("Solo con el juego delante. Proceso:", 12, y, delegate { cfg.SoloEnJuego = chkJuego.Checked; });
            txtProceso = new TextBox { Location = new Point(250, y), Size = new Size(130, 23) };
            txtProceso.TextChanged += delegate { if (!cargando && txtProceso.Text.Trim().Length > 0) { cfg.Proceso = txtProceso.Text.Trim(); cfg.Guardar(); } };
            Controls.Add(txtProceso);
            chkPausas = Casilla("Grabar las pausas largas", 480, y, delegate { cfg.GrabarPausas = chkPausas.Checked; });

            y = 396;
            chkMover = Casilla("Parar si muevo el ratón más de (px):", 12, y, delegate { cfg.PararSiMueves = chkMover.Checked; });
            numTol = Numero(250, y, 5, 500, delegate { cfg.Tolerancia = (int)numTol.Value; });
            chkPanel = Casilla("Panel de estado encima del juego", 480, y, delegate { cfg.Panel = chkPanel.Checked; MostrarPanel(); });

            lstLog = new ListBox
            {
                Location = new Point(12, 430), Size = new Size(756, 158), IntegralHeight = false, HorizontalScrollbar = true,
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
            };
            Controls.Add(lstLog);
        }

        Button Boton(string t, int x, int y, int w, EventHandler h)
        {
            Button b = new Button { Text = t, Location = new Point(x, y), Size = new Size(w, 28) };
            b.Click += h; Controls.Add(b); return b;
        }
        void Etiqueta(string t, int x, int y) { Controls.Add(new Label { Text = t, Location = new Point(x, y + 3), AutoSize = true }); }
        NumericUpDown Numero(int x, int y, int min, int max, Action cambio)
        {
            NumericUpDown n = new NumericUpDown { Location = new Point(x, y), Size = new Size(80, 23), Minimum = min, Maximum = max, Increment = 50 };
            n.ValueChanged += delegate { if (!cargando) { cambio(); cfg.Guardar(); } };
            Controls.Add(n); return n;
        }
        CheckBox Casilla(string t, int x, int y, Action cambio)
        {
            CheckBox c = new CheckBox { Text = t, Location = new Point(x, y), AutoSize = true };
            c.CheckedChanged += delegate { if (!cargando) { cambio(); cfg.Guardar(); } };
            Controls.Add(c); return c;
        }

        // ---- teclas rápidas

        static readonly uint[,] teclas = {
            // id, modificador, tecla   (0x4000 = sin autorrepetición, 0x4 = Mayús)
            { 1, 0x4000, W.VK_F7 }, { 2, 0x4000, W.VK_F8 }, { 3, 0x4000, W.VK_F9 }, { 4, 0x4000, W.VK_F10 },
            { 5, 0x4000, W.VK_F11 }, { 6, 0x4000, W.VK_PAUSE }, { 7, 0x4004, W.VK_F7 }, { 8, 0x4004, W.VK_F8 },
        };

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            string[] nombres = { "", "F7", "F8", "F9", "F10", "F11", "Pausa", "Mayús+F7", "Mayús+F8" };
            for (int i = 0; i < teclas.GetLength(0); i++)
                if (!W.RegisterHotKey(Handle, (int)teclas[i, 0], teclas[i, 1], teclas[i, 2]))
                    Log("AVISO: no se pudo reservar " + nombres[teclas[i, 0]] + " (¿la usa otro programa?)");
            Log("AutoPrisión 2026 por Carlos Sanz · basado en AutoPrisión DX de Jagoba Los Arcos González (MoleSoft, moledj.com)"); Log("Listo. TAB abre el menú y TAB lo para todo (F11 y Pausa también paran).");
        }

        protected override void OnShown(EventArgs e) { base.OnShown(e); MostrarPanel(); }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == 0x0312) Tecla(m.WParam.ToInt32());
            base.WndProc(ref m);
        }

        void Tecla(int id)
        {
            switch (id)
            {
                case 1: AlternarGrabacion(); break;
                case 2: if (motor.Ocupado) motor.Parar("Parado con F8"); else Reproducir(false, 0); break;
                case 3: if (motor.Ocupado) motor.Parar("Parado con F9"); else AutoClick(0); break;
                case 4: if (motor.Ocupado) motor.Parar("Parado con F10"); else Reproducir(true, 0); break;
                case 5: case 6: PararTodo("Parado con la tecla de parada"); break;
                case 7:
                    if (grabador.Activo) grabador.AnotarColor();
                    else
                    {
                        W.POINT p = W.Cursor();
                        Log("Color en (" + p.X + ", " + p.Y + "): #" + W.ColorEn(p.X, p.Y).ToString("X6"));
                    }
                    break;
                case 8:
                    if (!grabador.Activo && !motor.Ocupado)
                    {
                        lstSlots.SelectedIndex = cfg.Slot % 9;
                        Sonido.Anotar();
                        Mostrar("Secuencia activa: " + cfg.Slot + " (" + secs.S[cfg.Slot].Count + " acciones)", Azul, true);
                    }
                    break;
            }
        }

        // ---- acciones

        void Reproducir(bool bucle, int retraso)
        {
            if (grabador.Activo || motor.Ocupado) return;
            Log((bucle ? "Bucle" : "Reproducción") + " de la secuencia " + cfg.Slot + " (" + secs.S[cfg.Slot].Count + " acciones, una cada " + cfg.Velocidad + " ms)");
            motor.Reproducir(cfg.Slot, secs.S[cfg.Slot], bucle, retraso);
        }

        void AutoPelea(int retraso)
        {
            if (grabador.Activo || motor.Ocupado) return;
            int x, y, c;
            Log(Config.LeerSenal(cfg.PeleaSenal, out x, out y, out c)
                ? "Autopelea: Ctrl+R al empezar cada turno tuyo (señal #" + c.ToString("X6") + " en " + x + ", " + y + ")"
                : "Autopelea: Ctrl+R cada " + cfg.PeleaMs + " ms (sin señal de turno)");
            motor.AutoPelea(retraso);
        }

        // Con el ratón sobre algo que solo se ve en tu turno (p. ej. el botón FIN TURNO).
        void GuardarSenal()
        {
            W.POINT p = W.Cursor();
            int c = W.ColorEn(p.X, p.Y);
            cfg.PeleaSenal = p.X + "," + p.Y + "," + c.ToString("X6");
            cfg.Guardar();
            Sonido.Grabar();
            Log("Señal de turno guardada: #" + c.ToString("X6") + " en (" + p.X + ", " + p.Y + ")");
            Listo("Señal de turno guardada");
        }

        void AutoClick(int retraso)
        {
            if (grabador.Activo || motor.Ocupado) return;
            Log("Autoclick cada " + cfg.AutoClickMs + " ms" + (cfg.AutoLock ? " en posición fija" : ""));
            motor.AutoClick(retraso);
        }

        void AlternarGrabacion()
        {
            if (motor.Ocupado) { motor.Parar("Parado para grabar"); return; }
            if (grabador.Activo) { TerminarGrabacion(); return; }
            EmpezarGrabacion();
        }

        void EmpezarGrabacion()
        {
            if (motor.Ocupado || grabador.Activo) return;
            slotGrabando = cfg.Slot;
            if (!grabador.Empezar()) { Log("ERROR: Windows no dejó instalar la captura de ratón y teclado"); return; }
            Sonido.Grabar();
            Log("Grabando en la secuencia " + slotGrabando + "… (tabulador para terminar)");
            Mostrar("GRABANDO sec. " + slotGrabando + " · 0 acciones · TAB termina", Rojo, false);
        }

        void TerminarGrabacion()
        {
            List<Accion> r = grabador.Terminar();
            Sonido.Fin();
            if (r == null || r.Count == 0) { Log("Grabación vacía: se conserva la secuencia " + slotGrabando + " anterior"); Listo(null); return; }
            secs.S[slotGrabando] = r;
            GuardarSecuencias();
            Log("Grabación terminada: " + r.Count + " acciones en la secuencia " + slotGrabando);
            if (cfg.Slot == slotGrabando) MostrarTexto();
            Listo("Grabación terminada: " + r.Count + " acciones");
        }

        void PararTodo(string motivo)
        {
            if (grabador.Activo) TerminarGrabacion();
            if (motor.Ocupado) motor.Parar(motivo);
        }

        // Sin nada que hacer el panel se esconde para jugar sin él; si hay algo que avisar, lo enseña 2,5 s.
        void Listo(string ultimo)
        {
            if (motor.Ocupado || grabador.Activo) return;
            if (ultimo == null) { Ocultar(); return; }
            Mostrar(ultimo + " · Listo · sec. " + cfg.Slot + " (" + secs.S[cfg.Slot].Count + " acciones)", ultimo.StartsWith("Parado") ? Azul : Gris, true);
        }

        // El panel solo se ve con el menú del tabulador abierto o mientras graba, reproduce o pelea.
        void Mostrar(string texto, Color fondo, bool temporal)
        {
            panel.Poner(texto, fondo);
            ocultarEn = temporal ? DateTime.Now.AddSeconds(2.5) : DateTime.MaxValue;
            if (cfg.Panel && !panel.Visible) { panel.Show(); panel.SubirArriba(); }
        }

        void Ocultar()
        {
            ocultarEn = DateTime.MaxValue;
            if (panel.Visible) panel.Hide();
        }

        // ---- secuencias en pantalla

        void CambiarSlot(int n)
        {
            if (n < 1 || n > 9) return;
            if (cfg.Slot != n) { cfg.Slot = n; cfg.Guardar(); }
            MostrarTexto();
            Listo(null);
        }

        void MostrarTexto()
        {
            StringBuilder sb = new StringBuilder();
            foreach (Accion a in secs.S[cfg.Slot]) sb.Append(a.ToString().PadRight(24)).Append("# ").Append(a.Describir()).Append("\r\n");
            txtAcciones.Text = sb.ToString();
        }

        void GuardarTexto()
        {
            List<Accion> nueva = new List<Accion>();
            int n = 0;
            foreach (string linea in txtAcciones.Lines)
            {
                n++;
                string s = linea; int h = s.IndexOf('#'); if (h >= 0) s = s.Substring(0, h);
                foreach (string trozo in s.Split(';'))
                {
                    if (trozo.Trim().Length == 0) continue;
                    try { nueva.Add(Accion.Parse(trozo)); }
                    catch (Exception ex) { MessageBox.Show(this, "Línea " + n + ": " + ex.Message, "No se ha guardado", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
                }
            }
            secs.S[cfg.Slot] = nueva;
            GuardarSecuencias();
            MostrarTexto();
            Log("Secuencia " + cfg.Slot + " guardada (" + nueva.Count + " acciones)");
        }

        void GuardarSecuencias()
        {
            try { secs.Guardar(); } catch (Exception ex) { Log("ERROR guardando secuencias.txt: " + ex.Message); }
            RefrescarSlots();
        }

        void RefrescarSlots()
        {
            int sel = lstSlots.SelectedIndex;
            cargando = true;
            lstSlots.Items.Clear();
            for (int i = 1; i <= 9; i++) lstSlots.Items.Add("Secuencia " + i + "  (" + secs.S[i].Count + " acciones)");
            if (sel >= 0) lstSlots.SelectedIndex = sel;
            cargando = false;
        }

        void Creditos()
        {
            MessageBox.Show(this,
                "AutoPrisión 2026 · versión 1.0 para Prison Legacy\r\n\r\n" +
                "Basado en AutoPrisión DX Profesional (2003-2007)\r\n" +
                "creado por Jagoba Los Arcos González · MoleSoft\r\n" +
                "www.moledj.com\r\n\r\n" +
                "Versión 2026 (reescrita desde cero en C#): Carlos Sanz, con ayuda de Claude (Anthropic).\r\n\r\n" +
                "Igual que avisaba el original: úsalo bajo tu responsabilidad. Las normas del juego\r\n" +
                "pueden no permitir automatizar acciones y se arriesga la cuenta.\r\n\r\n" +
                "Saludos a los colegas :)",
                "Créditos", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        void MostrarPanel() { if (!cfg.Panel) Ocultar(); }

        void Log(string s)
        {
            string l = DateTime.Now.ToString("HH:mm:ss") + "  " + s;
            lstLog.Items.Add(l);
            while (lstLog.Items.Count > 400) lstLog.Items.RemoveAt(0);
            lstLog.TopIndex = Math.Max(0, lstLog.Items.Count - 1);
            try { File.AppendAllText(Path.Combine(carpeta, "autoprision.log"), DateTime.Now.ToString("yyyy-MM-dd ") + l + "\r\n", Encoding.UTF8); } catch { }
        }

        void EnUI(MethodInvoker m) { try { if (IsHandleCreated) BeginInvoke(m); } catch (ObjectDisposedException) { } }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            PararTodo("Cerrando");
            for (int i = 0; i < teclas.GetLength(0); i++) W.UnregisterHotKey(Handle, (int)teclas[i, 0]);
            reloj.Stop();
            panel.Close();
            base.OnFormClosing(e);
        }
    }

    static class Programa
    {
        [STAThread]
        static void Main()
        {
            W.SetProcessDPIAware(); // coordenadas en píxeles reales, igual que las grabadas
            bool nuevo;
            using (Mutex m = new Mutex(true, "AutoPrision2026", out nuevo))
            {
                if (!nuevo) { MessageBox.Show("AutoPrisión ya está abierto.", "AutoPrisión 2026"); return; }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new Principal());
            }
        }
    }
}
