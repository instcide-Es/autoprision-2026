// AutoPrisión 2026 para Mac — versión macOS de AutoPrisión 2026 (Windows, src\AutoPrision.cs).
// Original: Jagoba Los Arcos González · MoleSoft (2003-2007) · www.moledj.com
// Versión 2026: Carlos Sanz, con ayuda de Claude (Anthropic).
// Mismo formato de secuencias.txt y autoprision.ini que en Windows, guardados en ~/AutoPrision 2026.
//
// Diferencias con Windows:
//  · Las teclas de K,código son códigos de tecla de Mac (A=0, S=1, R=15, 1=18…), no de Windows.
//    Modificadores: C+ Control · S+ Mayúsculas · A+ Opción · M+ Comando.
//  · Parada de reserva: F11 o Comando+punto (los Mac no tienen tecla Pausa).
//  · Necesita tres permisos en Ajustes del Sistema > Privacidad y seguridad:
//    Accesibilidad (mover el ratón y pulsar teclas), Monitorización de entrada (leer TAB y los números)
//    y Grabación de pantalla (solo para la señal de turno y la acción P de esperar color).

import AppKit
import ApplicationServices
import ScreenCaptureKit

let carpeta: URL = {
    let u = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("AutoPrision 2026", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}()

// ---------------------------------------------------------------- teclado, ratón y pantalla

enum Teclas {
    static let tab: CGKeyCode = 48
    static let fila: [CGKeyCode] = [29, 18, 19, 20, 21, 23, 22, 26, 28, 25]     // 0..9 de la fila de números
    static let numerico: [CGKeyCode] = [82, 83, 84, 85, 86, 87, 88, 89, 91, 92] // 0..9 del teclado numérico
    static let f7: CGKeyCode = 98, f8: CGKeyCode = 100, f9: CGKeyCode = 101, f10: CGKeyCode = 109, f11: CGKeyCode = 103
    static let punto: CGKeyCode = 47

    static func pulsada(_ k: CGKeyCode) -> Bool { CGEventSource.keyState(.combinedSessionState, key: k) }
    static func flags() -> CGEventFlags { CGEventSource.flagsState(.combinedSessionState) }
    static func parada() -> Bool { pulsada(f11) || (pulsada(punto) && flags().contains(.maskCommand)) }

    static let nombres: [Int: String] = [
        0: "A", 11: "B", 8: "C", 2: "D", 14: "E", 3: "F", 5: "G", 4: "H", 34: "I", 38: "J", 40: "K", 37: "L", 46: "M",
        45: "N", 31: "O", 35: "P", 12: "Q", 15: "R", 1: "S", 17: "T", 32: "U", 9: "V", 13: "W", 7: "X", 16: "Y", 6: "Z",
        18: "1", 19: "2", 20: "3", 21: "4", 23: "5", 22: "6", 26: "7", 28: "8", 25: "9", 29: "0",
        36: "Intro", 48: "Tab", 49: "Espacio", 51: "Borrar", 53: "Esc", 123: "←", 124: "→", 125: "↓", 126: "↑",
        122: "F1", 120: "F2", 99: "F3", 118: "F4", 96: "F5", 97: "F6", 98: "F7", 100: "F8", 101: "F9", 109: "F10",
        103: "F11", 111: "F12",
    ]

    static func enviar(_ k: CGKeyCode, _ abajo: Bool, _ flags: CGEventFlags) {
        guard let e = CGEvent(keyboardEventSource: nil, virtualKey: k, keyDown: abajo) else { return }
        e.flags = flags
        e.post(tap: .cghidEventTap)
    }

    // Pulsar y soltar van siempre emparejados para no dejar nunca una tecla «pegada».
    static func pulsar(_ a: Accion) {
        var mods: [(CGKeyCode, CGEventFlags)] = []
        if a.ctrl { mods.append((59, .maskControl)) }
        if a.shift { mods.append((56, .maskShift)) }
        if a.alt { mods.append((58, .maskAlternate)) }
        if a.cmd { mods.append((55, .maskCommand)) }
        var f: CGEventFlags = []
        for (k, m) in mods { f.insert(m); enviar(k, true, f) }
        enviar(CGKeyCode(a.sc), true, f)
        Thread.sleep(forTimeInterval: 0.04)
        enviar(CGKeyCode(a.sc), false, f)
        for (k, m) in mods.reversed() { f.remove(m); enviar(k, false, f); Thread.sleep(forTimeInterval: 0.005) }
    }
}

enum Raton {
    static func posicion() -> CGPoint { CGEvent(source: nil)?.location ?? .zero }

    static func mover(_ p: CGPoint) {
        CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: p, mouseButton: .left)?.post(tap: .cghidEventTap)
    }

    static func clic(izquierdo: Bool) {
        let p = posicion()
        let boton: CGMouseButton = izquierdo ? .left : .right
        let abajo = CGEvent(mouseEventSource: nil, mouseType: izquierdo ? .leftMouseDown : .rightMouseDown, mouseCursorPosition: p, mouseButton: boton)
        let arriba = CGEvent(mouseEventSource: nil, mouseType: izquierdo ? .leftMouseUp : .rightMouseUp, mouseCursorPosition: p, mouseButton: boton)
        abajo?.setIntegerValueField(.mouseEventClickState, value: 1)
        arriba?.setIntegerValueField(.mouseEventClickState, value: 1)
        abajo?.post(tap: .cghidEventTap)
        Thread.sleep(forTimeInterval: 0.03)
        arriba?.post(tap: .cghidEventTap)
    }
}

final class Caja { var rgb: Int?; var filtro: SCContentFilter? }

// Color de un punto de la pantalla (necesita el permiso de Grabación de pantalla).
// Bloquea hasta tener la respuesta: no llamarlo desde el hilo principal.
final class Pantalla {
    static let comun = Pantalla()
    private var filtro: SCContentFilter?
    private let cerrojo = NSLock()

    func color(_ x: Int, _ y: Int) -> Int? {
        if !CGPreflightScreenCaptureAccess() { return nil }
        cerrojo.lock(); let f = filtro; cerrojo.unlock()
        let caja = Caja()
        let sem = DispatchSemaphore(value: 0)
        Task.detached {
            defer { sem.signal() }
            do {
                var fil = f
                if fil == nil {
                    let c = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: true)
                    guard let d = c.displays.first(where: { $0.displayID == CGMainDisplayID() }) ?? c.displays.first else { return }
                    let yo = c.applications.filter { $0.processID == getpid() } // sin el panel de estado
                    fil = SCContentFilter(display: d, excludingApplications: yo, exceptingWindows: [])
                    caja.filtro = fil
                }
                let conf = SCStreamConfiguration()
                conf.sourceRect = CGRect(x: x, y: y, width: 1, height: 1)
                conf.width = 1
                conf.height = 1
                conf.showsCursor = false
                let img = try await SCScreenshotManager.captureImage(contentFilter: fil!, configuration: conf)
                caja.rgb = Pantalla.rgbDe(img)
            } catch {}
        }
        _ = sem.wait(timeout: .now() + 3)
        if let nf = caja.filtro { cerrojo.lock(); filtro = nf; cerrojo.unlock() }
        return caja.rgb
    }

    static func rgbDe(_ img: CGImage) -> Int {
        var px = [UInt8](repeating: 0, count: 4)
        px.withUnsafeMutableBytes { b in
            if let ctx = CGContext(data: b.baseAddress, width: 1, height: 1, bitsPerComponent: 8, bytesPerRow: 4,
                                   space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) {
                ctx.draw(img, in: CGRect(x: 0, y: 0, width: 1, height: 1))
            }
        }
        return (Int(px[0]) << 16) | (Int(px[1]) << 8) | Int(px[2])
    }
}

// Qué programa está delante. NSWorkspace avisa en el hilo principal; el motor lo lee desde el suyo.
enum Juego {
    static let cerrojo = NSLock()
    static var delante: pid_t = NSWorkspace.shared.frontmostApplication?.processIdentifier ?? 0
    static var cachePid: pid_t = -1
    static var cacheNombres: [String] = []

    static func vigilar() {
        NSWorkspace.shared.notificationCenter.addObserver(forName: NSWorkspace.didActivateApplicationNotification, object: nil, queue: .main) { n in
            if let a = n.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication {
                Juego.cerrojo.lock(); Juego.delante = a.processIdentifier; Juego.cerrojo.unlock()
            }
        }
    }

    static func pidDelante() -> pid_t { cerrojo.lock(); defer { cerrojo.unlock() }; return delante }

    static func nombres(_ pid: pid_t) -> [String] {
        cerrojo.lock(); defer { cerrojo.unlock() }
        if pid != cachePid {
            let a = NSRunningApplication(processIdentifier: pid)
            cacheNombres = [a?.localizedName, a?.executableURL?.lastPathComponent, a?.bundleIdentifier].compactMap { $0?.lowercased() }
            cachePid = pid
        }
        return cacheNombres
    }

    // «proceso» admite varios nombres separados por comas; basta con que el nombre del programa los contenga.
    static func es(_ pid: pid_t, _ proceso: String) -> Bool {
        let n = nombres(pid)
        for p in proceso.split(separator: ",") {
            let t = p.trimmingCharacters(in: .whitespaces).lowercased()
            if !t.isEmpty && n.contains(where: { $0.contains(t) }) { return true }
        }
        return false
    }

    static func nombreDelante() -> String {
        NSRunningApplication(processIdentifier: pidDelante())?.localizedName ?? "?"
    }
}

func hex6(_ c: Int) -> String { String(format: "%06X", c) }

// ---------------------------------------------------------------- acciones

enum Tipo { case clicIzq, clicDer, tecla, espera, color }

struct Fallo: Error { let m: String }

struct Accion {
    var t: Tipo = .espera
    var x = 0, y = 0, sc = 0, ms = 0, rgb = 0, tol = 25, timeout = 0
    var ctrl = false, shift = false, alt = false, cmd = false

    // ML,x,y · MR,x,y · K,[C+][S+][A+][M+]código · E,ms · P,x,y,RRGGBB[,tolerancia[,maxms]]
    static func parse(_ s: String) throws -> Accion {
        let p = s.trimmingCharacters(in: .whitespaces).split(separator: ",", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespaces) }
        func n(_ i: Int) throws -> Int {
            guard let v = Int(p[i]) else { throw Fallo(m: "«\(p[i])» no es un número") }
            return v
        }
        var a = Accion()
        switch p[0].uppercased() {
        case "ML", "MR":
            guard p.count == 3 else { throw Fallo(m: "se esperaba ML,x,y") }
            a.t = p[0].uppercased() == "ML" ? .clicIzq : .clicDer
            a.x = try n(1); a.y = try n(2)
        case "K":
            guard p.count == 2 else { throw Fallo(m: "se esperaba K,código") }
            a.t = .tecla; a.sc = -1
            for t in p[1].split(separator: "+") {
                let u = t.trimmingCharacters(in: .whitespaces).uppercased()
                switch u {
                case "C": a.ctrl = true
                case "S": a.shift = true
                case "A": a.alt = true
                case "M": a.cmd = true
                default:
                    guard let v = Int(u) else { throw Fallo(m: "código de tecla no válido") }
                    a.sc = v
                }
            }
            guard (0...127).contains(a.sc) else { throw Fallo(m: "código de tecla no válido (0-127)") }
        case "E":
            guard p.count == 2 else { throw Fallo(m: "se esperaba E,milisegundos") }
            a.t = .espera; a.ms = try max(0, n(1))
        case "P":
            guard p.count >= 4 else { throw Fallo(m: "se esperaba P,x,y,RRGGBB") }
            a.t = .color; a.x = try n(1); a.y = try n(2)
            guard let c = Int(p[3].replacingOccurrences(of: "#", with: ""), radix: 16) else { throw Fallo(m: "color no válido") }
            a.rgb = c
            a.tol = try p.count > 4 ? n(4) : 25
            a.timeout = try p.count > 5 ? n(5) : 0
        default:
            throw Fallo(m: "acción desconocida «\(p[0])»")
        }
        return a
    }

    var texto: String {
        switch t {
        case .clicIzq: return "ML,\(x),\(y)"
        case .clicDer: return "MR,\(x),\(y)"
        case .tecla: return "K," + (ctrl ? "C+" : "") + (shift ? "S+" : "") + (alt ? "A+" : "") + (cmd ? "M+" : "") + "\(sc)"
        case .espera: return "E,\(ms)"
        case .color: return "P,\(x),\(y),\(hex6(rgb)),\(tol)" + (timeout > 0 ? ",\(timeout)" : "")
        }
    }

    func describir() -> String {
        switch t {
        case .clicIzq: return "clic izquierdo en (\(x), \(y))"
        case .clicDer: return "clic derecho en (\(x), \(y))"
        case .tecla:
            return "tecla " + (ctrl ? "Control+" : "") + (shift ? "Mayús+" : "") + (alt ? "Opción+" : "") + (cmd ? "Comando+" : "") +
                (Teclas.nombres[sc] ?? "código \(sc)")
        case .espera: return "esperar " + String(format: "%.1f", Double(ms) / 1000) + " s"
        case .color:
            return "esperar color #\(hex6(rgb)) en (\(x), \(y))" + (timeout > 0 ? ", máx. " + String(format: "%.1f", Double(timeout) / 1000) + " s" : "")
        }
    }
}

final class Secuencias {
    var s = [[Accion]](repeating: [], count: 10) // 1..9
    let ruta = carpeta.appendingPathComponent("secuencias.txt")

    func cargar() -> [String] {
        var errores: [String] = []
        guard let datos = try? Data(contentsOf: ruta) else { return errores }
        let texto = String(data: datos, encoding: .utf8) ?? String(data: datos, encoding: .isoLatin1) ?? ""
        // El original a veces deja basura delante («10Secuencia 3=...»): se busca el patrón en la línea.
        let re = try! NSRegularExpression(pattern: "Secuencia\\s*([1-9])\\s*=(.*)$")
        for linea in texto.components(separatedBy: .newlines) {
            let ns = linea as NSString
            guard let m = re.firstMatch(in: linea, range: NSRange(location: 0, length: ns.length)) else { continue }
            let n = Int(ns.substring(with: m.range(at: 1)))!
            s[n] = []
            for trozo in ns.substring(with: m.range(at: 2)).split(separator: ";") {
                let t = trozo.trimmingCharacters(in: .whitespaces)
                if t.isEmpty { continue }
                do { s[n].append(try Accion.parse(t)) } catch let f as Fallo { errores.append("Secuencia \(n): «\(t)» ignorada (\(f.m))") } catch {}
            }
        }
        return errores
    }

    func guardar() throws {
        var r = ""
        for i in 1...9 { r += "Secuencia \(i)=" + s[i].map { $0.texto + ";" }.joined() + "\r\n" }
        try r.write(to: ruta, atomically: true, encoding: .utf8)
    }
}

// ---------------------------------------------------------------- configuración

final class Config {
    let ruta = carpeta.appendingPathComponent("autoprision.ini")
    var velocidad = 200, autoClickMs = 1000, tolerancia = 40, slot = 1, color = 0
    var autoLock = false, soloEnJuego = true, pararSiMueves = true, grabarPausas = true, panel = true
    var proceso = "Prison"
    var peleaMs = 1500
    var peleaSenal = ""        // "x,y,RRGGBB" o vacío
    var peleaTecla = "C+15"    // Control+R

    static func leerSenal(_ s: String) -> (x: Int, y: Int, rgb: Int)? {
        let p = s.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }
        guard p.count == 3, let x = Int(p[0]), let y = Int(p[1]), let c = Int(p[2], radix: 16) else { return nil }
        return (x, y, c)
    }

    init() {
        guard let texto = try? String(contentsOf: ruta, encoding: .utf8) else { return }
        var d: [String: String] = [:]
        for l in texto.components(separatedBy: .newlines) {
            if let i = l.firstIndex(of: "="), i != l.startIndex {
                d[l[..<i].trimmingCharacters(in: .whitespaces).lowercased()] = l[l.index(after: i)...].trimmingCharacters(in: .whitespaces)
            }
        }
        func int(_ k: String, _ def: Int) -> Int { d[k].flatMap { Int($0) } ?? def }
        color = int("color", color)
        velocidad = max(20, int("reproduccion", velocidad))
        autoLock = int("autolock", 0) != 0
        autoClickMs = max(20, int("autoclick_ms", autoClickMs))
        tolerancia = max(5, int("tolerancia_raton", tolerancia))
        slot = min(9, max(1, int("secuencia_activa", slot)))
        soloEnJuego = int("solo_en_juego", 1) != 0
        pararSiMueves = int("parar_si_mueves_raton", 1) != 0
        grabarPausas = int("grabar_pausas", 1) != 0
        panel = int("panel", 1) != 0
        if let v = d["proceso"], !v.isEmpty { proceso = v }
        peleaMs = max(300, int("autopelea_ms", peleaMs))
        if let v = d["autopelea_senal"] { peleaSenal = v }
        if let v = d["autopelea_tecla"], !v.isEmpty { peleaTecla = v }
    }

    func guardar() {
        let l = [
            "[configuracion]",
            "color=\(color)",
            "auto=1",
            "reproduccion=\(velocidad)",
            "autolock=\(autoLock ? 1 : 0)",
            "autoclick_ms=\(autoClickMs)",
            "secuencia_activa=\(slot)",
            "solo_en_juego=\(soloEnJuego ? 1 : 0)",
            "proceso=\(proceso)",
            "parar_si_mueves_raton=\(pararSiMueves ? 1 : 0)",
            "tolerancia_raton=\(tolerancia)",
            "grabar_pausas=\(grabarPausas ? 1 : 0)",
            "panel=\(panel ? 1 : 0)",
            "autopelea_ms=\(peleaMs)",
            "autopelea_senal=\(peleaSenal)",
            "autopelea_tecla=\(peleaTecla)",
        ]
        try? (l.joined(separator: "\n") + "\n").write(to: ruta, atomically: true, encoding: .utf8)
    }

    func juegoDelante() -> Bool { Juego.es(Juego.pidDelante(), proceso) }
}

// ---------------------------------------------------------------- motor (hilo propio)

struct Detenido: Error { let m: String }

func ahora() -> Double { CFAbsoluteTimeGetCurrent() }

final class Motor {
    let cfg: Config
    private let cerrojo = NSLock()
    private var _ocupado = false
    private var parar = false
    private var motivoParar: String?
    private var vigilarRaton = false
    private var esperado = CGPoint.zero
    var finTick: Double = 0 // cuándo terminó el último trabajo

    var estado: ((String) -> Void)?      // texto del panel mientras trabaja
    var terminado: ((String) -> Void)?   // motivo

    init(_ cfg: Config) { self.cfg = cfg }

    var ocupado: Bool { cerrojo.lock(); defer { cerrojo.unlock() }; return _ocupado }

    func pararCon(_ motivo: String) { cerrojo.lock(); motivoParar = motivo; parar = true; cerrojo.unlock() }

    func reproducir(_ slot: Int, _ lista: [Accion], bucle: Bool, retraso: Int) {
        lanzar(retraso) { [unowned self] in
            if lista.isEmpty { throw Detenido(m: "La secuencia \(slot) está vacía") }
            var vuelta = 0
            repeat {
                vuelta += 1
                for (i, a) in lista.enumerated() {
                    self.avisar((bucle ? "BUCLE sec. \(slot) · vuelta \(vuelta)" : "Reproduciendo sec. \(slot)") + " · \(i + 1)/\(lista.count) · TAB para")
                    try self.comprobar()
                    try self.ejecutar(a)
                    try self.esperar(self.cfg.velocidad)
                }
            } while bucle
            return "Reproducción terminada (sec. \(slot))"
        }
    }

    func autoClick(retraso: Int) {
        lanzar(retraso) { [unowned self] in
            let fijo = self.cfg.autoLock
            let p = Raton.posicion()
            var n = 0
            while true {
                if fijo { self.moverA(p) }
                try self.comprobar()
                Raton.clic(izquierdo: true)
                n += 1
                self.avisar("AUTOCLICK " + (fijo ? "fijo en (\(Int(p.x)), \(Int(p.y)))" : "donde está el ratón") + " · \(n) clics · TAB para")
                try self.esperar(self.cfg.autoClickMs)
            }
        }
    }

    // Autopelea (turno continuo): pulsa Control+R, que en Prison Legacy repite las acciones del turno anterior;
    // el juego solo lo acepta en tu turno (también en un TURNO DOBLE). Con «señal de turno» (un punto que solo
    // tiene ese color cuando es tu turno) pulsa justo al empezar tu turno; sin ella, pulsa cada peleaMs.
    func autoPelea(retraso: Int) {
        let senal = Config.leerSenal(cfg.peleaSenal)
        let tecla = (try? Accion.parse("K," + cfg.peleaTecla)) ?? (try! Accion.parse("K,C+15"))
        lanzar(retraso) { [unowned self] in
            var n = 0
            while true {
                if let s = senal {
                    self.avisar("AUTOPELEA · esperando tu turno · \(n) turnos · TAB para")
                    while !(try self.colorParecido(s.x, s.y, s.rgb, 30)) { try self.esperar(100) }
                }
                try self.comprobar()
                Teclas.pulsar(tecla)
                n += 1
                self.avisar("AUTOPELEA · \(tecla.describir()) (\(n))" + (senal == nil ? " cada \(self.cfg.peleaMs) ms" : "") + " · TAB para")
                if let s = senal {
                    // Si la señal sigue ahí pasado el tiempo es un turno doble (o no entró): se repite.
                    let t0 = ahora()
                    while try self.colorParecido(s.x, s.y, s.rgb, 30), (ahora() - t0) * 1000 < Double(self.cfg.peleaMs * 2) { try self.esperar(100) }
                } else {
                    try self.esperar(self.cfg.peleaMs)
                }
            }
        }
    }

    private func lanzar(_ retrasoMs: Int, _ trabajo: @escaping () throws -> String) {
        cerrojo.lock()
        if _ocupado { cerrojo.unlock(); return }
        _ocupado = true; parar = false; motivoParar = nil; vigilarRaton = false
        cerrojo.unlock()
        let h = Thread { [unowned self] in
            var fin: String
            do {
                var r = retrasoMs
                while r > 0 {
                    if self.debeParar() { throw Detenido(m: self.motivo()) }
                    self.avisar("Empieza en \((r + 999) / 1000) s: cambia al juego")
                    Thread.sleep(forTimeInterval: 0.1)
                    r -= 100
                }
                Sonido.inicio()
                fin = try trabajo()
            } catch let d as Detenido { fin = d.m } catch { fin = "Error: \(error)" }
            self.vigilarRaton = false
            self.finTick = ahora()
            self.cerrojo.lock(); self._ocupado = false; self.cerrojo.unlock()
            Sonido.fin()
            self.terminado?(fin)
        }
        h.qualityOfService = .userInteractive
        h.start()
    }

    private func debeParar() -> Bool { cerrojo.lock(); defer { cerrojo.unlock() }; return parar }
    private func motivo() -> String { cerrojo.lock(); defer { cerrojo.unlock() }; return motivoParar ?? "Parado" }

    private func avisar(_ s: String) { estado?(s) }

    // Se llama constantemente: cualquiera de estas condiciones para el trabajo en menos de 15 ms.
    private func comprobar() throws {
        if debeParar() { throw Detenido(m: motivo()) }
        if Teclas.pulsada(Teclas.tab) { throw Detenido(m: "Parado con el tabulador") }
        if Teclas.parada() { throw Detenido(m: "Parado con la tecla de parada") }
        if cfg.soloEnJuego && !cfg.juegoDelante() { throw Detenido(m: "Parado: el juego no está delante (delante: \(Juego.nombreDelante()))") }
        if cfg.pararSiMueves && vigilarRaton {
            let c = Raton.posicion()
            let dx = c.x - esperado.x, dy = c.y - esperado.y
            let tol = CGFloat(cfg.tolerancia)
            if dx * dx + dy * dy > tol * tol { throw Detenido(m: "Parado: has movido el ratón") }
        }
    }

    private func esperar(_ ms: Int) throws {
        let t0 = ahora()
        while true {
            try comprobar()
            let resta = Double(ms) / 1000 - (ahora() - t0)
            if resta <= 0 { return }
            Thread.sleep(forTimeInterval: min(0.015, resta))
        }
    }

    private func moverA(_ p: CGPoint) {
        Raton.mover(p)
        esperado = p; vigilarRaton = true
    }

    private func colorParecido(_ x: Int, _ y: Int, _ rgb: Int, _ tol: Int) throws -> Bool {
        guard let c = Pantalla.comun.color(x, y) else {
            throw Detenido(m: "Parado: falta el permiso de Grabación de pantalla para leer colores")
        }
        return Motor.parecido(c, rgb, tol)
    }

    private func ejecutar(_ a: Accion) throws {
        switch a.t {
        case .clicIzq, .clicDer:
            moverA(CGPoint(x: a.x, y: a.y))
            try esperar(30) // el juego necesita ver el ratón encima antes del clic
            Raton.clic(izquierdo: a.t == .clicIzq)
        case .tecla:
            Teclas.pulsar(a)
        case .espera:
            try esperar(a.ms)
        case .color:
            let t0 = ahora()
            while !(try colorParecido(a.x, a.y, a.rgb, a.tol)) {
                if a.timeout > 0 && (ahora() - t0) * 1000 > Double(a.timeout) {
                    throw Detenido(m: "Parado: no apareció el color #\(hex6(a.rgb)) en (\(a.x), \(a.y))")
                }
                try esperar(50)
            }
        }
    }

    static func parecido(_ a: Int, _ b: Int, _ tol: Int) -> Bool {
        abs(((a >> 16) & 0xFF) - ((b >> 16) & 0xFF)) <= tol &&
            abs(((a >> 8) & 0xFF) - ((b >> 8) & 0xFF)) <= tol &&
            abs((a & 0xFF) - (b & 0xFF)) <= tol
    }
}

enum Sonido {
    static func sonar(_ nombre: String) { DispatchQueue.main.async { NSSound(named: NSSound.Name(nombre))?.play() } }
    static func inicio() { sonar("Tink") }
    static func fin() { sonar("Bottle") }
    static func grabar() { sonar("Morse") }
    static func anotar() { sonar("Pop") }
}

// ---------------------------------------------------------------- grabación (CGEventTap solo de escucha)

final class Grabador {
    let cfg: Config
    private var tap: CFMachPort?
    private var fuente: CFRunLoopSource?
    private var ultimo: Double?
    var acciones: [Accion]?
    var anotada: ((Accion) -> Void)?

    static let nuestras: Set<Int> = [Int(Teclas.tab), Int(Teclas.f7), Int(Teclas.f8), Int(Teclas.f9), Int(Teclas.f10), Int(Teclas.f11)]

    init(_ cfg: Config) { self.cfg = cfg }

    var activo: Bool { acciones != nil }

    func empezar() -> Bool {
        acciones = []
        ultimo = nil
        let mask = (CGEventMask(1) << CGEventMask(CGEventType.leftMouseDown.rawValue)) |
            (CGEventMask(1) << CGEventMask(CGEventType.rightMouseDown.rawValue)) |
            (CGEventMask(1) << CGEventMask(CGEventType.keyDown.rawValue))
        let yo = Unmanaged.passUnretained(self).toOpaque()
        guard let t = CGEvent.tapCreate(tap: .cgSessionEventTap, place: .headInsertEventTap, options: .listenOnly, eventsOfInterest: mask,
                                        callback: { _, tipo, evento, info in
                                            if let info = info { Unmanaged<Grabador>.fromOpaque(info).takeUnretainedValue().evento(tipo, evento) }
                                            return Unmanaged.passUnretained(evento)
                                        }, userInfo: yo) else {
            acciones = nil
            return false
        }
        tap = t
        fuente = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, t, 0)
        CFRunLoopAddSource(CFRunLoopGetMain(), fuente, .commonModes)
        CGEvent.tapEnable(tap: t, enable: true)
        return true
    }

    func terminar() -> [Accion]? {
        if let t = tap { CGEvent.tapEnable(tap: t, enable: false); CFMachPortInvalidate(t) }
        if let f = fuente { CFRunLoopRemoveSource(CFRunLoopGetMain(), f, .commonModes) }
        tap = nil; fuente = nil
        let r = acciones
        acciones = nil
        return r
    }

    func anotarColor() {
        guard activo else { return }
        let p = Raton.posicion()
        DispatchQueue.global().async {
            let c = Pantalla.comun.color(Int(p.x), Int(p.y))
            DispatchQueue.main.async {
                guard let c = c else { return }
                var a = Accion(); a.t = .color; a.x = Int(p.x); a.y = Int(p.y); a.rgb = c; a.tol = 25
                self.anotar(a)
            }
        }
    }

    private func seGraba(_ pid: pid_t) -> Bool {
        if pid == getpid() { return false } // clics y teclas en esta ventana, no
        return !cfg.soloEnJuego || Juego.es(pid, cfg.proceso)
    }

    private func anotar(_ a: Accion) {
        guard activo else { return }
        if cfg.grabarPausas, let u = ultimo {
            let hueco = (ahora() - u) * 1000 - Double(cfg.velocidad)
            if hueco > 800 { var e = Accion(); e.t = .espera; e.ms = Int((hueco / 100).rounded()) * 100; acciones?.append(e) }
        }
        ultimo = ahora()
        acciones?.append(a)
        anotada?(a)
    }

    // Dueño de la ventana que hay bajo el ratón (no hace falta permiso de pantalla para el PID).
    private static func duenoVentana(_ id: Int64) -> pid_t? {
        guard id > 0, let l = CGWindowListCopyWindowInfo([.optionIncludingWindow], CGWindowID(id)) as? [[String: Any]],
              let pid = l.first?[kCGWindowOwnerPID as String] as? Int else { return nil }
        return pid_t(pid)
    }

    fileprivate func evento(_ tipo: CGEventType, _ e: CGEvent) {
        if tipo == .tapDisabledByTimeout || tipo == .tapDisabledByUserInput {
            if let t = tap { CGEvent.tapEnable(tap: t, enable: true) }
            return
        }
        guard activo, e.getIntegerValueField(.eventSourceUnixProcessID) != Int64(getpid()) else { return }
        switch tipo {
        case .leftMouseDown, .rightMouseDown:
            let pid = Grabador.duenoVentana(e.getIntegerValueField(.mouseEventWindowUnderMousePointer)) ?? Juego.pidDelante()
            guard seGraba(pid) else { return }
            var a = Accion()
            a.t = tipo == .leftMouseDown ? .clicIzq : .clicDer
            a.x = Int(e.location.x.rounded()); a.y = Int(e.location.y.rounded())
            anotar(a)
        case .keyDown:
            let k = Int(e.getIntegerValueField(.keyboardEventKeycode))
            guard e.getIntegerValueField(.keyboardEventAutorepeat) == 0, !Grabador.nuestras.contains(k), seGraba(Juego.pidDelante()) else { return }
            var a = Accion()
            a.t = .tecla; a.sc = k
            a.ctrl = e.flags.contains(.maskControl)
            a.shift = e.flags.contains(.maskShift)
            a.alt = e.flags.contains(.maskAlternate)
            a.cmd = e.flags.contains(.maskCommand)
            anotar(a)
        default:
            break
        }
    }
}

// ---------------------------------------------------------------- panel encima del juego

final class PanelEstado {
    let ventana: NSPanel
    let texto: NSTextField

    init() {
        let ancho: CGFloat = 820, alto: CGFloat = 26
        let pant = NSScreen.main?.frame ?? NSRect(x: 0, y: 0, width: 1440, height: 900)
        let vis = NSScreen.main?.visibleFrame ?? pant
        ventana = NSPanel(contentRect: NSRect(x: pant.midX - ancho / 2, y: vis.maxY - alto, width: ancho, height: alto),
                          styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        // Nunca coge el foco ni los clics, y se ve también sobre el juego a pantalla completa.
        ventana.level = .screenSaver
        ventana.ignoresMouseEvents = true
        ventana.hidesOnDeactivate = false
        ventana.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary, .ignoresCycle]
        ventana.alphaValue = 0.88
        texto = NSTextField(labelWithString: "")
        texto.font = .boldSystemFont(ofSize: 13)
        texto.textColor = .white
        texto.alignment = .center
        texto.lineBreakMode = .byTruncatingTail
        texto.frame = NSRect(x: 6, y: 4, width: ancho - 12, height: 18)
        ventana.contentView?.addSubview(texto)
    }

    func poner(_ s: String, _ fondo: NSColor) { texto.stringValue = s; ventana.backgroundColor = fondo }
    func mostrar(_ si: Bool) { if si { ventana.orderFrontRegardless() } else { ventana.orderOut(nil) } }
}

// ---------------------------------------------------------------- ventana principal

final class Volteada: NSView { override var isFlipped: Bool { true } }

final class Principal: NSObject {
    let cfg: Config
    let secs = Secuencias()
    let motor: Motor
    let grabador: Grabador
    let panel = PanelEstado()
    var slotGrabando = 1

    let ventana: NSWindow
    let vista = Volteada(frame: NSRect(x: 0, y: 0, width: 820, height: 660))
    var popSlots: NSPopUpButton!
    var txtAcciones: NSTextView!
    var txtLog: NSTextView!
    var numVel: NSTextField!, numAuto: NSTextField!, numTol: NSTextField!, txtProceso: NSTextField!
    var chkLock: NSButton!, chkJuego: NSButton!, chkMover: NSButton!, chkPausas: NSButton!, chkPanel: NSButton!

    static let gris = NSColor(calibratedWhite: 0.16, alpha: 1), verde = NSColor(calibratedRed: 0.08, green: 0.43, blue: 0.16, alpha: 1),
        rojo = NSColor(calibratedRed: 0.67, green: 0.12, blue: 0.12, alpha: 1), azul = NSColor(calibratedRed: 0.12, green: 0.27, blue: 0.59, alpha: 1)

    let horaF: DateFormatter = { let f = DateFormatter(); f.dateFormat = "HH:mm:ss"; return f }()
    let fechaF: DateFormatter = { let f = DateFormatter(); f.dateFormat = "yyyy-MM-dd "; return f }()

    override init() {
        let c = Config()
        cfg = c
        motor = Motor(c)
        grabador = Grabador(c)
        ventana = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 820, height: 660), styleMask: [.titled, .closable, .miniaturizable],
                           backing: .buffered, defer: false)
        super.init()
        ventana.title = "AutoPrisión 2026 por Carlos Sanz · basado en AutoPrisión DX de MoleSoft"
        ventana.isReleasedWhenClosed = false
        ventana.contentView = vista
        construir()
        menuApp()

        for e in secs.cargar() { log(e) }
        numVel.integerValue = cfg.velocidad
        numAuto.integerValue = cfg.autoClickMs
        numTol.integerValue = cfg.tolerancia
        chkLock.state = cfg.autoLock ? .on : .off
        chkJuego.state = cfg.soloEnJuego ? .on : .off
        chkMover.state = cfg.pararSiMueves ? .on : .off
        chkPausas.state = cfg.grabarPausas ? .on : .off
        chkPanel.state = cfg.panel ? .on : .off
        txtProceso.stringValue = cfg.proceso
        refrescarSlots()
        popSlots.selectItem(at: cfg.slot - 1)
        mostrarTexto()

        motor.estado = { [weak self] s in DispatchQueue.main.async { self?.panel.poner(s, Principal.verde) } }
        motor.terminado = { [weak self] s in DispatchQueue.main.async { self?.log(s); self?.listo(s) } }
        grabador.anotada = { [weak self] a in
            guard let self = self else { return }
            Sonido.anotar()
            self.panel.poner("GRABANDO sec. \(self.slotGrabando) · \(self.grabador.acciones?.count ?? 0) acciones · TAB termina", Principal.rojo)
            self.log("  + " + a.describir())
        }

        NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: ventana, queue: .main) { [weak self] _ in
            self?.pararTodo("Cerrando")
            NSApp.terminate(nil)
        }
        Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            guard let self = self else { return }
            if self.menu != .cerrado && Date() > self.menuHasta { self.cerrarMenu() }
        }

        Juego.vigilar()
        ventana.center()
        ventana.makeKeyAndOrderFront(nil)
        NSApp.activate()
        mostrarPanel()
        log("AutoPrisión 2026 para Mac por Carlos Sanz · basado en AutoPrisión DX de Jagoba Los Arcos González (MoleSoft, moledj.com)")
        log("Carpeta de datos: " + carpeta.path)
        comprobarPermisos(pedir: true)
        listo(nil)
        arrancarVigia()
    }

    // ---- permisos de macOS

    func comprobarPermisos(pedir: Bool) {
        let acc = pedir ? AXIsProcessTrustedWithOptions([kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary)
            : AXIsProcessTrusted()
        let entrada = CGPreflightListenEventAccess()
        if pedir && !entrada { _ = CGRequestListenEventAccess() }
        if acc && entrada {
            log("Listo. TAB abre el menú y TAB lo para todo (F11 y Comando+punto también paran).")
        } else {
            if !acc { log("FALTA PERMISO: Accesibilidad (para mover el ratón y pulsar teclas)") }
            if !entrada { log("FALTA PERMISO: Monitorización de entrada (para leer el TAB y los números)") }
            log("Actívalos en Ajustes del Sistema > Privacidad y seguridad y vuelve a abrir AutoPrisión (botón «Permisos…»).")
        }
        if !CGPreflightScreenCaptureAccess() {
            log("Opcional: Grabación de pantalla, solo para la señal de turno (TAB+7) y la acción P de esperar color.")
        }
    }

    @objc func abrirPermisos() {
        let acc = AXIsProcessTrusted(), entrada = CGPreflightListenEventAccess()
        let destino = !acc ? "Privacy_Accessibility" : !entrada ? "Privacy_ListenEvent" : "Privacy_ScreenCapture"
        if acc && entrada && !CGPreflightScreenCaptureAccess() { _ = CGRequestScreenCaptureAccess() }
        if let u = URL(string: "x-apple.systempreferences:com.apple.preference.security?" + destino) { NSWorkspace.shared.open(u) }
        comprobarPermisos(pedir: false)
    }

    // ---- menú del tabulador (como el original)
    //
    // Un hilo propio mira el teclado cada 10 ms. No depende del foco: por eso no «pierde el teclado».
    // El tabulador y los números siguen llegando al juego, igual que con el programa viejo.

    enum Modo { case cerrado, principal, grabar, reproducir, bucle, velocidad }
    var menu = Modo.cerrado
    var menuHasta = Date()

    // Códigos internos del vigía: 0-9 números, 100 TAB, 107-110 F7-F10, 111 parada, 117 Mayús+F7.
    func arrancarVigia() {
        let t = Thread { [weak self] in
            var antes: [Int: Bool] = [:]
            while true {
                var nuevos: [Int] = []
                func mira(_ id: Int, _ si: Bool) { if si && !(antes[id] ?? false) { nuevos.append(id) }; antes[id] = si }
                mira(100, Teclas.pulsada(Teclas.tab))
                for i in 0...9 { mira(i, Teclas.pulsada(Teclas.fila[i]) || Teclas.pulsada(Teclas.numerico[i])) }
                let mayus = Teclas.flags().contains(.maskShift)
                let f7 = Teclas.pulsada(Teclas.f7)
                mira(107, f7 && !mayus)
                mira(117, f7 && mayus)
                mira(108, Teclas.pulsada(Teclas.f8))
                mira(109, Teclas.pulsada(Teclas.f9))
                mira(110, Teclas.pulsada(Teclas.f10))
                mira(111, Teclas.parada())
                for id in nuevos { DispatchQueue.main.async { self?.tecla(id) } }
                Thread.sleep(forTimeInterval: 0.01)
            }
        }
        t.qualityOfService = .userInteractive
        t.start()
    }

    func tecla(_ id: Int) {
        switch id {
        case 100: teclaMenu(-1)
        case 0...9: teclaMenu(id)
        case 107: alternarGrabacion()
        case 108: if motor.ocupado { motor.pararCon("Parado con F8") } else { reproducir(false, 0) }
        case 109: if motor.ocupado { motor.pararCon("Parado con F9") } else { autoClick(0) }
        case 110: if motor.ocupado { motor.pararCon("Parado con F10") } else { reproducir(true, 0) }
        case 111: pararTodo("Parado con la tecla de parada")
        case 117:
            if grabador.activo { grabador.anotarColor() } else {
                let p = Raton.posicion()
                DispatchQueue.global().async {
                    let c = Pantalla.comun.color(Int(p.x), Int(p.y))
                    DispatchQueue.main.async {
                        self.log(c.map { "Color en (\(Int(p.x)), \(Int(p.y))): #\(hex6($0))" } ?? "Sin permiso de Grabación de pantalla para leer colores")
                    }
                }
            }
        default: break
        }
    }

    func teclaMenu(_ n: Int) {
        if n < 0 { // tabulador
            if motor.ocupado || grabador.activo { cerrarMenu(); pararTodo("Parado con el tabulador") }
            else if ahora() - motor.finTick < 0.7 {} // ese tabulador ya paró la reproducción
            else if menu != .cerrado { cerrarMenu() }
            else { abrirMenu(.principal) }
            return
        }
        if menu == .cerrado { return }
        if n == 0 { cerrarMenu(); return }
        switch menu {
        case .principal:
            switch n {
            case 1: cerrarMenu(); autoClick(0)
            case 2: abrirMenu(.grabar)
            case 3: abrirMenu(.bucle)
            case 4: abrirMenu(.velocidad)
            case 5: abrirMenu(.reproducir)
            case 6: cerrarMenu(); autoPelea(0)
            case 7: cerrarMenu(); guardarSenal()
            case 8:
                cerrarMenu(); cfg.peleaSenal = ""; cfg.guardar()
                log("Señal de turno borrada: la autopelea pulsará la tecla cada \(cfg.peleaMs) ms"); listo("Señal de turno borrada")
            default: break
            }
        case .grabar:
            cerrarMenu(); seleccionar(n); empezarGrabacion()
        case .reproducir, .bucle:
            let bucle = menu == .bucle
            cerrarMenu(); seleccionar(n); reproducir(bucle, 0)
        case .velocidad:
            if n == 1 { cfg.velocidad = max(20, cfg.velocidad - 100) }
            else if n == 2 { cfg.velocidad = min(10000, cfg.velocidad + 100) }
            else if n == 3 { cfg.autoClickMs = max(20, cfg.autoClickMs - 100) }
            else if n == 4 { cfg.autoClickMs = min(60000, cfg.autoClickMs + 100) }
            numVel.integerValue = cfg.velocidad; numAuto.integerValue = cfg.autoClickMs; cfg.guardar()
            abrirMenu(.velocidad)
        case .cerrado: break
        }
    }

    func abrirMenu(_ m: Modo) {
        menu = m
        menuHasta = Date().addingTimeInterval(8)
        Sonido.anotar()
        let con = (1...9).filter { !secs.s[$0].isEmpty }.map(String.init).joined(separator: ",")
        let datos = "   con datos: " + (con.isEmpty ? "ninguna" : con)
        switch m {
        case .principal: panel.poner("1 Autoclick · 2 Grabar · 3 Reproducir · 4 Velocidad · 5 Una vez · 6 Autopelea · 7/8 Señal turno · 0 Quitar", Principal.azul)
        case .grabar: panel.poner("GRABAR en la secuencia nº… (1-9)" + datos, Principal.azul)
        case .reproducir: panel.poner("REPRODUCIR UNA VEZ la secuencia nº… (1-9)" + datos, Principal.azul)
        case .bucle: panel.poner("REPRODUCIR (hasta TAB) la secuencia nº… (1-9)" + datos, Principal.azul)
        case .velocidad:
            panel.poner("Una acción cada \(cfg.velocidad) ms (1 más rápido · 2 más lento) · autoclick \(cfg.autoClickMs) ms (3 · 4)", Principal.azul)
        case .cerrado: break
        }
    }

    func cerrarMenu() {
        if menu == .cerrado { return }
        menu = .cerrado
        listo(nil)
    }

    func seleccionar(_ n: Int) { if (1...9).contains(n) { popSlots.selectItem(at: n - 1); cambiarSlot() } }

    // ---- construcción de la ventana

    func construir() {
        let ayuda = NSTextField(wrappingLabelWithString:
            "TAB+1 autoclick · TAB+2+nº grabar (TAB termina) · TAB+3+nº reproducir hasta TAB · TAB+4 velocidad · TAB+5+nº una vez\n" +
            "TAB+6 autopelea (Control+R cada turno) · TAB+7 guardar señal de turno bajo el ratón · TAB+8 borrarla · 0 quita el menú\n" +
            "PARAR: tabulador (también termina la grabación). De reserva: F11 o Comando+punto. También para si mueves el ratón o el juego no está delante.\n" +
            "En el Mac las F7–F11 pueden necesitar la tecla fn.")
        ayuda.frame = NSRect(x: 12, y: 10, width: 796, height: 80)
        vista.addSubview(ayuda)

        popSlots = NSPopUpButton(frame: NSRect(x: 12, y: 96, width: 220, height: 26), pullsDown: false)
        popSlots.target = self; popSlots.action = #selector(cambiarSlot)
        vista.addSubview(popSlots)

        let formato = NSTextField(wrappingLabelWithString:
            "ML,x,y clic izq. · MR,x,y clic der. · K,C+15 Control+R (C Control, S Mayús, A Opción, M Comando) · E,1500 esperar ms · P,x,y,RRGGBB,tol,máx-ms esperar color · # comentario")
        formato.frame = NSRect(x: 242, y: 94, width: 566, height: 34)
        formato.textColor = .secondaryLabelColor
        formato.font = .systemFont(ofSize: 11)
        vista.addSubview(formato)

        let sv = NSTextView.scrollableTextView()
        sv.frame = NSRect(x: 12, y: 134, width: 796, height: 170)
        sv.borderType = .bezelBorder
        txtAcciones = (sv.documentView as! NSTextView)
        txtAcciones.font = .monospacedSystemFont(ofSize: 12, weight: .regular)
        txtAcciones.allowsUndo = true
        txtAcciones.isAutomaticQuoteSubstitutionEnabled = false
        txtAcciones.isAutomaticDashSubstitutionEnabled = false
        txtAcciones.isAutomaticTextReplacementEnabled = false
        txtAcciones.isAutomaticSpellingCorrectionEnabled = false
        vista.addSubview(sv)

        boton("Guardar cambios de la secuencia", 12, 310, 230, #selector(guardarTexto))
        boton("Abrir carpeta", 248, 310, 130, #selector(abrirCarpeta))
        boton("Permisos…", 384, 310, 110, #selector(abrirPermisos))
        boton("Acerca de… / créditos", 500, 310, 170, #selector(creditos))

        var x: CGFloat = 12
        for (t, s) in [("Grabar (F7)", #selector(btnGrabar)), ("Reproducir (F8)", #selector(btnReproducir)), ("Bucle (F10)", #selector(btnBucle)),
                       ("Autoclick (F9)", #selector(btnAutoclick)), ("Autopelea", #selector(btnAutopelea))] {
            boton(t, x, 350, 126, s); x += 132
        }
        let parar = boton("PARAR (TAB)", x, 350, 136, #selector(btnParar))
        parar.contentTintColor = .systemRed
        parar.font = .boldSystemFont(ofSize: 13)

        etiqueta("Una acción cada (ms):", 12, 394)
        numVel = numero(160, 392)
        etiqueta("Autoclick cada (ms):", 250, 394)
        numAuto = numero(390, 392)
        chkLock = casilla("Autoclick en posición fija", 480, 392)

        chkJuego = casilla("Solo con el juego delante. Nombre:", 12, 424)
        txtProceso = NSTextField(frame: NSRect(x: 270, y: 422, width: 150, height: 22))
        txtProceso.target = self; txtProceso.action = #selector(cambioAjustes)
        txtProceso.cell?.sendsActionOnEndEditing = true
        vista.addSubview(txtProceso)
        chkPausas = casilla("Grabar las pausas largas", 480, 424)

        chkMover = casilla("Parar si muevo el ratón más de (px):", 12, 456)
        numTol = numero(270, 454)
        chkPanel = casilla("Panel de estado encima del juego", 480, 456)

        let lv = NSTextView.scrollableTextView()
        lv.frame = NSRect(x: 12, y: 488, width: 796, height: 160)
        lv.borderType = .bezelBorder
        txtLog = (lv.documentView as! NSTextView)
        txtLog.isEditable = false
        txtLog.font = .systemFont(ofSize: 11)
        vista.addSubview(lv)
    }

    @discardableResult
    func boton(_ t: String, _ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ s: Selector) -> NSButton {
        let b = NSButton(title: t, target: self, action: s)
        b.frame = NSRect(x: x, y: y, width: w, height: 30)
        vista.addSubview(b)
        return b
    }

    func etiqueta(_ t: String, _ x: CGFloat, _ y: CGFloat) {
        let l = NSTextField(labelWithString: t)
        l.frame = NSRect(x: x, y: y, width: 150, height: 18)
        vista.addSubview(l)
    }

    func numero(_ x: CGFloat, _ y: CGFloat) -> NSTextField {
        let n = NSTextField(frame: NSRect(x: x, y: y, width: 70, height: 22))
        n.target = self; n.action = #selector(cambioAjustes)
        n.cell?.sendsActionOnEndEditing = true
        vista.addSubview(n)
        return n
    }

    func casilla(_ t: String, _ x: CGFloat, _ y: CGFloat) -> NSButton {
        let c = NSButton(checkboxWithTitle: t, target: self, action: #selector(cambioAjustes))
        c.frame = NSRect(x: x, y: y, width: 260, height: 22)
        vista.addSubview(c)
        return c
    }

    func menuApp() {
        let barra = NSMenu()
        let appItem = NSMenuItem(); barra.addItem(appItem)
        let appMenu = NSMenu()
        let cred = appMenu.addItem(withTitle: "Acerca de AutoPrisión 2026", action: #selector(creditos), keyEquivalent: "")
        cred.target = self
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Salir de AutoPrisión", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu
        let edItem = NSMenuItem(); barra.addItem(edItem)
        let ed = NSMenu(title: "Edición")
        ed.addItem(withTitle: "Deshacer", action: Selector(("undo:")), keyEquivalent: "z")
        ed.addItem(withTitle: "Rehacer", action: Selector(("redo:")), keyEquivalent: "Z")
        ed.addItem(.separator())
        ed.addItem(withTitle: "Cortar", action: #selector(NSText.cut(_:)), keyEquivalent: "x")
        ed.addItem(withTitle: "Copiar", action: #selector(NSText.copy(_:)), keyEquivalent: "c")
        ed.addItem(withTitle: "Pegar", action: #selector(NSText.paste(_:)), keyEquivalent: "v")
        ed.addItem(withTitle: "Seleccionar todo", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")
        edItem.submenu = ed
        NSApp.mainMenu = barra
    }

    // ---- botones y ajustes

    @objc func btnGrabar() { alternarGrabacion() }
    @objc func btnReproducir() { reproducir(false, 3000) }
    @objc func btnBucle() { reproducir(true, 3000) }
    @objc func btnAutoclick() { autoClick(3000) }
    @objc func btnAutopelea() { autoPelea(3000) }
    @objc func btnParar() { pararTodo("Parado desde la ventana") }

    @objc func cambioAjustes() {
        cfg.velocidad = min(10000, max(20, numVel.integerValue)); numVel.integerValue = cfg.velocidad
        cfg.autoClickMs = min(60000, max(20, numAuto.integerValue)); numAuto.integerValue = cfg.autoClickMs
        cfg.tolerancia = min(500, max(5, numTol.integerValue)); numTol.integerValue = cfg.tolerancia
        cfg.autoLock = chkLock.state == .on
        cfg.soloEnJuego = chkJuego.state == .on
        cfg.pararSiMueves = chkMover.state == .on
        cfg.grabarPausas = chkPausas.state == .on
        let p = txtProceso.stringValue.trimmingCharacters(in: .whitespaces)
        if !p.isEmpty { cfg.proceso = p }
        if cfg.panel != (chkPanel.state == .on) { cfg.panel = chkPanel.state == .on; mostrarPanel() }
        cfg.guardar()
    }

    @objc func abrirCarpeta() { NSWorkspace.shared.open(carpeta) }

    // ---- acciones

    func reproducir(_ bucle: Bool, _ retraso: Int) {
        if grabador.activo || motor.ocupado { return }
        log((bucle ? "Bucle" : "Reproducción") + " de la secuencia \(cfg.slot) (\(secs.s[cfg.slot].count) acciones, una cada \(cfg.velocidad) ms)")
        motor.reproducir(cfg.slot, secs.s[cfg.slot], bucle: bucle, retraso: retraso)
    }

    func autoPelea(_ retraso: Int) {
        if grabador.activo || motor.ocupado { return }
        if let s = Config.leerSenal(cfg.peleaSenal) {
            log("Autopelea: tecla al empezar cada turno tuyo (señal #\(hex6(s.rgb)) en \(s.x), \(s.y))")
        } else {
            log("Autopelea: tecla cada \(cfg.peleaMs) ms (sin señal de turno)")
        }
        motor.autoPelea(retraso: retraso)
    }

    // Con el ratón sobre algo que solo se ve en tu turno (p. ej. el botón FIN TURNO).
    func guardarSenal() {
        let p = Raton.posicion()
        if !CGPreflightScreenCaptureAccess() {
            _ = CGRequestScreenCaptureAccess()
            log("Para la señal de turno hace falta el permiso de Grabación de pantalla (Ajustes del Sistema > Privacidad y seguridad).")
            return
        }
        DispatchQueue.global().async {
            let c = Pantalla.comun.color(Int(p.x), Int(p.y))
            DispatchQueue.main.async {
                guard let c = c else { self.log("No se pudo leer el color de la pantalla"); return }
                self.cfg.peleaSenal = "\(Int(p.x)),\(Int(p.y)),\(hex6(c))"
                self.cfg.guardar()
                Sonido.grabar()
                self.log("Señal de turno guardada: #\(hex6(c)) en (\(Int(p.x)), \(Int(p.y)))")
                self.listo("Señal de turno guardada")
            }
        }
    }

    func autoClick(_ retraso: Int) {
        if grabador.activo || motor.ocupado { return }
        log("Autoclick cada \(cfg.autoClickMs) ms" + (cfg.autoLock ? " en posición fija" : ""))
        motor.autoClick(retraso: retraso)
    }

    func alternarGrabacion() {
        if motor.ocupado { motor.pararCon("Parado para grabar"); return }
        if grabador.activo { terminarGrabacion(); return }
        empezarGrabacion()
    }

    func empezarGrabacion() {
        if motor.ocupado || grabador.activo { return }
        slotGrabando = cfg.slot
        if !grabador.empezar() { log("ERROR: macOS no dejó escuchar el ratón y el teclado (falta el permiso de Monitorización de entrada)"); return }
        Sonido.grabar()
        log("Grabando en la secuencia \(slotGrabando)… (tabulador para terminar)")
        panel.poner("GRABANDO sec. \(slotGrabando) · 0 acciones · TAB termina", Principal.rojo)
    }

    func terminarGrabacion() {
        let r = grabador.terminar()
        Sonido.fin()
        guard let acciones = r, !acciones.isEmpty else {
            log("Grabación vacía: se conserva la secuencia \(slotGrabando) anterior"); listo(nil); return
        }
        secs.s[slotGrabando] = acciones
        guardarSecuencias()
        log("Grabación terminada: \(acciones.count) acciones en la secuencia \(slotGrabando)")
        if cfg.slot == slotGrabando { mostrarTexto() }
        listo(nil)
    }

    func pararTodo(_ motivo: String) {
        if grabador.activo { terminarGrabacion() }
        if motor.ocupado { motor.pararCon(motivo) }
    }

    func listo(_ ultimo: String?) {
        if motor.ocupado || grabador.activo { return }
        panel.poner((ultimo.map { $0 + " · " } ?? "") + "Listo · sec. \(cfg.slot) (\(secs.s[cfg.slot].count) acciones)",
                    ultimo?.hasPrefix("Parado") == true ? Principal.azul : Principal.gris)
    }

    // ---- secuencias en pantalla

    @objc func cambiarSlot() {
        let n = popSlots.indexOfSelectedItem + 1
        if !(1...9).contains(n) { return }
        if cfg.slot != n { cfg.slot = n; cfg.guardar() }
        mostrarTexto()
        listo(nil)
    }

    func mostrarTexto() {
        txtAcciones.string = secs.s[cfg.slot].map { $0.texto.padding(toLength: max(24, $0.texto.count), withPad: " ", startingAt: 0) + "# " + $0.describir() + "\n" }.joined()
    }

    @objc func guardarTexto() {
        var nueva: [Accion] = []
        for (i, linea) in txtAcciones.string.components(separatedBy: .newlines).enumerated() {
            let s = linea.split(separator: "#", maxSplits: 1, omittingEmptySubsequences: false).first.map(String.init) ?? ""
            for trozo in s.split(separator: ";") {
                let t = trozo.trimmingCharacters(in: .whitespaces)
                if t.isEmpty { continue }
                do { nueva.append(try Accion.parse(t)) } catch {
                    let al = NSAlert()
                    al.messageText = "No se ha guardado"
                    al.informativeText = "Línea \(i + 1): " + ((error as? Fallo)?.m ?? "\(error)")
                    al.alertStyle = .warning
                    al.runModal()
                    return
                }
            }
        }
        secs.s[cfg.slot] = nueva
        guardarSecuencias()
        mostrarTexto()
        log("Secuencia \(cfg.slot) guardada (\(nueva.count) acciones)")
    }

    func guardarSecuencias() {
        do { try secs.guardar() } catch { log("ERROR guardando secuencias.txt: \(error.localizedDescription)") }
        refrescarSlots()
    }

    func refrescarSlots() {
        let sel = popSlots.indexOfSelectedItem
        popSlots.removeAllItems()
        for i in 1...9 { popSlots.addItem(withTitle: "Secuencia \(i)  (\(secs.s[i].count) acciones)") }
        popSlots.selectItem(at: sel >= 0 ? sel : cfg.slot - 1)
    }

    @objc func creditos() {
        let al = NSAlert()
        al.messageText = "AutoPrisión 2026 para Mac · versión 1.0 para Prison Legacy"
        al.informativeText =
            "Basado en AutoPrisión DX Profesional (2003-2007)\ncreado por Jagoba Los Arcos González · MoleSoft\nwww.moledj.com\n\n" +
            "Versión 2026 (reescrita desde cero): Carlos Sanz, con ayuda de Claude (Anthropic).\n\n" +
            "Igual que avisaba el original: úsalo bajo tu responsabilidad. Las normas del juego pueden no permitir " +
            "automatizar acciones y se arriesga la cuenta.\n\nSaludos a los colegas :)"
        al.runModal()
    }

    func mostrarPanel() { panel.mostrar(cfg.panel) }

    func log(_ s: String) {
        let d = Date()
        let l = horaF.string(from: d) + "  " + s
        txtLog.textStorage?.append(NSAttributedString(string: l + "\n", attributes: [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor.labelColor]))
        txtLog.scrollToEndOfDocument(nil)
        let url = carpeta.appendingPathComponent("autoprision.log")
        if let datos = (fechaF.string(from: d) + l + "\n").data(using: .utf8) {
            if let h = try? FileHandle(forWritingTo: url) { h.seekToEndOfFile(); h.write(datos); h.closeFile() } else { try? datos.write(to: url) }
        }
    }
}

// ---------------------------------------------------------------- arranque

let app = NSApplication.shared
app.setActivationPolicy(.regular)
let principal = Principal()
app.run()
