#include "SystemState.h"
#include "UI.h"
#include "Storage.h"
#include "BTPSSensor.h"
#include "BronquitoNet.h"
#include "RespiratorySensors.h"

BronquitoUI ui;
BronquitoStorage storage(19, 22, 23, 21);
BronquitoNetwork net;
RespiratorySensors sensoresResp(36, 39);

bool compuOk = false;
bool wifiAntes = false;
bool syncInicialPendiente = false;
EstadoConexion ultimoEstado = CON_NINGUNA;

SystemState currentState = STATE_LOGIN;
const unsigned long INTERVALO_SD_MS = 2000;
unsigned long ultimoChequeoSD = 0;
String ultimaClaveSD = "";

void manejarResultadoSincronizacion(StorageStatus status) {
  ultimaClaveSD = String((int)status) + "|" + storage.getDNI();
  ui.setDNI(storage.getDNI());
  ui.setSynced(storage.isSynced());
  ui.drawLoginScreen();

  switch (status) {
    case ERR_NO_CARD:
      Serial.println("[FSM] Error: No hay tarjeta MicroSD");
      ui.showErrorMessage("¡Error de tarjeta!", "No se detectó la microSD");
      delay(1800);
      ui.drawLoginScreen();
      break;

    case ERR_FILE_NOT_FOUND:
      Serial.println("[FSM] Error: Archivo perfil.json no encontrado");
      ui.showErrorMessage("¡Archivo faltante!", "No existe perfil.json");
      delay(1800);
      ui.drawLoginScreen();
      break;

    case ERR_JSON_PARSE:
      Serial.println("[FSM] Error: Formato JSON invalido");
      ui.showErrorMessage("¡Archivo dañado!", "Error en los datos del perfil");
      delay(1800);
      ui.drawLoginScreen();
      break;

    case STORAGE_OK:
    default:
      break;
  }
}

void refrescarPerfilEnUI() {
  StorageStatus st = storage.sync(false);
  ultimaClaveSD = String((int)st) + "|" + storage.getDNI();
  ui.setDNI(storage.getDNI());
  ui.setSynced(storage.isSynced());
}

EstadoConexion calcularEstado() {
  if (!net.estaConectado()) return CON_NINGUNA;
  return compuOk ? CON_WIFI_Y_COMPU : CON_SOLO_WIFI;
}

void actualizarConexion() {
  bool wifiAhora = net.estaConectado();
  if (wifiAhora && !wifiAntes && net.servidorGuardado().length() > 0)
    syncInicialPendiente = true;
  if (!wifiAhora) compuOk = false;
  wifiAntes = wifiAhora;
}

void redibujarPantallaActual() {
  if (currentState == STATE_MAIN_MENU) ui.drawMainMenu();
  else if (currentState == STATE_LOGIN) ui.drawLoginScreen();
}

void formatearDispositivo(bool forzar) {
  ui.mostrarMensaje("Preparando para nuevo paciente...");
  FormatoResultado r = storage.formatearFabrica(forzar);

  switch (r) {
    case FORMATO_OK:
      ui.mostrarMensaje("Listo para un nuevo paciente", "Se borraron los datos", COLOR_VERDE_OK);
      break;
    case FORMATO_PENDIENTES:
      ui.mostrarMensaje("No se pudo preparar", "Hay pruebas sin subir", COLOR_ROJO_DEL);
      break;
    case FORMATO_SIN_SD:
      ui.mostrarMensaje("No se pudo preparar", "No hay tarjeta SD", COLOR_ROJO_DEL);
      break;
    default:
      ui.mostrarMensaje("Preparación incompleta", "Revisá la tarjeta SD", COLOR_ROJO_DEL);
      break;
  }
  delay(2000);

  if (r == FORMATO_OK || r == FORMATO_ERROR) {
    currentState = STATE_LOGIN;
    ui.clearPin();
    manejarResultadoSincronizacion(storage.sync());
  } else {
    redibujarPantallaActual();
  }
}

void leerComandosSerie() {
  if (!Serial.available()) return;
  String c = Serial.readStringUntil('\n');
  c.trim();
  c.toUpperCase();
  if (c == "FORMATEAR")        formatearDispositivo(false);
  else if (c == "FORMATEAR!")  formatearDispositivo(true);
  else if (c == "OLVIDAR_RED") {
    net.olvidarRedes();
    compuOk = false;
  }
}

bool dispositivoVacio()   { return storage.getLastError() == ERR_FILE_NOT_FOUND; }
String usuarioActual()    { return storage.isSynced() ? storage.getDNI() : ""; }
int archivosPendientes()  { return storage.isSynced() ? storage.contarPendientes() : 0; }

bool identificarCompu(const String& ip, RespuestaIdentificar& r) {
  bool ok = net.identificar(ip, dispositivoVacio(), usuarioActual(), archivosPendientes(), r);
  if (ok) net.ponerHora(r.epoch, r.offsetMin);
  compuOk = ok;
  return ok;
}

int verificarPinOnline(const String& pin) {
  ui.mostrarMensaje("Verificando PIN...");
  RespuestaPin r = net.verificarPin(storage.getDNI(), pin);

  if (r.ok) {
    storage.guardarPin(pin, r.pinVersion);
    return 1;
  }
  if (r.http == 401) {
    ui.mostrarMensaje("PIN incorrecto", "", COLOR_ROJO_DEL);
    delay(1500);
    return 0;
  }
  String motivo = net.textoError(r.http);
  if (r.error == "sin_paciente_asignado")    motivo = "No hay un paciente asignado";
  else if (r.error == "usuario_no_asignado") motivo = "Este usuario no está asignado";
  ui.mostrarMensaje("No se pudo verificar", motivo, COLOR_ROJO_DEL);
  delay(2200);
  return -1;
}

bool pedirYVerificarPin(const String& titulo, const String& subtitulo) {
  while (true) {
    String pin = ui.pedirPin(titulo, subtitulo);
    if (pin.length() == 0) return false;
    int v = verificarPinOnline(pin);
    if (v == 1) {
      ui.mostrarMensaje("¡PIN correcto!", "", COLOR_VERDE_OK);
      delay(1000);
      return true;
    }
    if (v == -1) return false;
  }
}

bool confirmarSolicitud(int id, bool ok, const char* detalle) {
  JsonDocument cuerpo;
  cuerpo["ok"] = ok;
  if (detalle != nullptr && detalle[0] != '\0') cuerpo["detalle"] = detalle;
  String enviar;
  serializeJson(cuerpo, enviar);

  JsonDocument resp;
  int http = net.llamarApi(net.servidorGuardado(), "POST",
                           "/dispositivo/solicitudes/" + String(id) + "/confirmar", enviar, resp);
  return http == 200;
}

void atenderSolicitud(JsonObject s) {
  int id = s["id"] | 0;
  String tipo = String(s["tipo"] | "");
  Serial.println("[SYNC] Solicitud " + String(id) + ": " + tipo);

  if (tipo == "RESET") {
    bool acepta = ui.pedirConfirmacion("Preparar para nuevo paciente", "Tu médico pide borrar", "los datos del paciente actual");
    if (!acepta) { confirmarSolicitud(id, false, "rechazado_por_usuario"); return; }

    FormatoResultado fr = storage.formatearFabrica(false);
    if (fr == FORMATO_OK) {
      confirmarSolicitud(id, true, "");
      currentState = STATE_LOGIN;
      ui.clearPin();
      refrescarPerfilEnUI();
    } else if (fr == FORMATO_PENDIENTES) {
      confirmarSolicitud(id, false, "archivos_pendientes");
    } else if (fr == FORMATO_SIN_SD) {
      ui.mostrarMensaje("No se pudo preparar", "No se detectó la tarjeta SD", COLOR_ROJO_DEL);
      delay(2200);
      confirmarSolicitud(id, false, "sin_tarjeta_sd");
    } else {
      confirmarSolicitud(id, false, "error_formato");
    }
    return;
  }

  if (tipo == "CARGAR_PERFIL") {
    JsonObject perfil = s["payload"]["perfil"];
    String nombre = String(perfil["nombre"] | "") + " " + String(perfil["apellido"] | "");
    String usuario = String(perfil["usuario"] | "");

    bool acepta = ui.pedirConfirmacion("Cargar paciente", nombre, "Usuario: " + usuario);
    if (!acepta) { confirmarSolicitud(id, false, "rechazado_por_usuario"); return; }

    if (!storage.guardarPerfil(perfil)) {
      bool hayTarjeta = storage.tarjetaPresente();
      ui.mostrarMensaje("No se pudo cargar", hayTarjeta ? "Error al escribir la SD" : "No se detectó la tarjeta SD",
                        COLOR_ROJO_DEL);
      delay(2200);
      confirmarSolicitud(id, false, hayTarjeta ? "error_sd" : "sin_tarjeta_sd");
      return;
    }
    refrescarPerfilEnUI();

    pedirYVerificarPin("Hola " + String(perfil["nombre"] | ""), "Ingresá tu PIN para activarlo");
    confirmarSolicitud(id, true, "");
    return;
  }

  confirmarSolicitud(id, false, "tipo_no_soportado");
}

int procesarSolicitudes() {
  for (int vuelta = 0; vuelta < 10; vuelta++) {
    JsonDocument resp;
    int http = net.llamarApi(net.servidorGuardado(), "GET", "/dispositivo/solicitudes", "", resp);
    if (http != 200) return http;

    JsonArray lista = resp["solicitudes"].as<JsonArray>();
    if (lista.size() == 0) return 200;

    for (JsonObject s : lista) atenderSolicitud(s);
  }
  return 200;
}

void sincronizar() {
  ui.mostrarMensaje("Sincronizando...");
  RespuestaIdentificar r;
  if (!identificarCompu(net.servidorGuardado(), r)) {
    ui.mostrarMensaje("No se pudo sincronizar", net.textoError(r.http), COLOR_ROJO_DEL);
    delay(2200);
    return;
  }
  Serial.println("[SYNC] Hora local: " + net.fechaHoraLocal());

  if (storage.isSynced() && !storage.pinPendiente() && r.usuario.length() > 0 &&
      r.usuario == storage.getDNI() && r.pinVersion.length() > 0 && r.pinVersion != storage.getPinVersion()) {
    pedirYVerificarPin("PIN actualizado", "Ingresá tu nuevo PIN");
  }

  int http = procesarSolicitudes();
  refrescarPerfilEnUI();

  if (http == 200) ui.mostrarMensaje("¡Sincronizado!", "", COLOR_VERDE_OK);
  else             ui.mostrarMensaje("No se pudo sincronizar", net.textoError(http), COLOR_ROJO_DEL);
  delay(1200);
}

bool actualizarFrecuencia() {
  if (!net.estaConectado() || net.servidorGuardado().length() == 0 || !storage.isSynced()) return false;

  ui.mostrarMensaje("Actualizando frecuencia...");
  JsonDocument resp;
  int http = net.llamarApi(net.servidorGuardado(), "GET",
                           "/dispositivo/frecuencia?usuario=" + storage.getDNI(), "", resp);
  if (http != 200) return false;

  JsonObject f = resp["frecuencia"];
  if (f.isNull()) return false;
  if (!storage.guardarFrecuencia(f)) {
    Serial.println("[FREC] No se pudo guardar en la SD");
    return false;
  }

  JsonDocument cuerpo;
  cuerpo["id_frecuencia"] = f["id"];
  String enviar;
  serializeJson(cuerpo, enviar);
  JsonDocument confirmacion;
  net.llamarApi(net.servidorGuardado(), "POST", "/dispositivo/frecuencia/confirmar", enviar, confirmacion);
  return true;
}

bool flujoCompu() {
  while (true) {
    String lineas[3] = {
      "Asegurate de que la computadora",
      "esté en la misma red e ingresá",
      "el código que aparece en pantalla"
    };
    String codigo = ui.pedirCodigo(lineas, 3, net.prefijoRed());
    if (codigo.length() == 0) return false;

    if (codigo.toInt() > 255) {
      ui.mostrarMensaje("Código inválido", "Tiene que ser de 0 a 255", COLOR_ROJO_DEL);
      delay(1800);
      continue;
    }
    String ip = net.prefijoRed() + String(codigo.toInt());

    ui.mostrarMensaje("Probando la computadora...", ip);
    RespuestaIdentificar r;
    if (identificarCompu(ip, r)) {
      net.guardarServidor(ip);
      ui.mostrarMensaje("¡Computadora conectada!", ip, COLOR_VERDE_OK);
      delay(1500);
      return true;
    }
    ui.mostrarMensaje("La computadora no responde", net.textoError(r.http), COLOR_ROJO_DEL);
    delay(2200);
  }
}

void flujoWifi() {
  while (true) {
    ui.mostrarMensaje("Buscando redes...");
    RedWifi redes[MAX_REDES];
    int n = net.escanear(redes, MAX_REDES);
    String nombres[MAX_REDES];
    int rssi[MAX_REDES];
    for (int i = 0; i < n; i++) { nombres[i] = redes[i].ssid; rssi[i] = redes[i].rssi; }

    int sel = ui.elegirRed(nombres, rssi, n);
    if (sel == -3) {
      net.reconectarGuardada();
      return;
    }
    if (sel == -1) continue;

    String ssid;
    if (sel == -2) {
      ssid = ui.pedirTexto("Nombre de la red");
      if (ssid == "") continue;
    } else {
      ssid = nombres[sel];
    }

    String clave = ui.pedirTexto("Contraseña de " + ssid);
    if (clave == "") continue;

    ui.mostrarMensaje("Conectando...", ssid);
    ResultadoWifi r = net.conectar(ssid, clave);

    if (r == RES_WIFI_OK) {
      ui.mostrarMensaje("¡Wi-Fi conectado!", ssid, COLOR_VERDE_OK);
      delay(1000);
      wifiAntes = true;
      compuOk = false;
      flujoCompu();
      return;
    }
    if (r == RES_WIFI_INCOMPATIBLE) {
      ui.mostrarMensaje("Red no compatible", "Usá el Wi-Fi de tu casa o un hotspot", COLOR_ROJO_DEL);
    } else {
      ui.mostrarMensaje("No se pudo conectar", "Revisá la contraseña", COLOR_ROJO_DEL);
    }
    delay(1800);
  }
}

// ======================= HISTORIAL DE EVALUACIONES =======================
// Carpeta /EVALUACIONES: cada calibración guiada genera dos pruebas (PIM y Espirometría),
// y cada prueba tiene dos archivos con el mismo nombre:
//   eval_AAAAMMDD_HHMMSS_PIM.csv / _ESP.csv   -> señal cruda (tiempo_ms,presion_cmh2o,flujo_lmin)
//   eval_AAAAMMDD_HHMMSS_PIM.json / _ESP.json -> métricas globales (es el que lista el Historial)
// El calibracion_id (eval_AAAAMMDD_HHMMSS) es el mismo en las dos y las identifica como un par.
const int MAX_HIST = 30;
const char *DIR_EVAL = "/EVALUACIONES";

MetricaItem metrica(const char *etiqueta, const String &valor, const char *unidad) {
  MetricaItem m;
  m.etiqueta = etiqueta;
  m.valor = valor;
  m.unidad = unidad;
  return m;
}

// ID de la evaluación = fecha y hora: eval_AAAAMMDD_HHMMSS
String generarIdEvaluacion() {
  SD.mkdir(DIR_EVAL);
  String hora = net.fechaHoraLocal();
  String d = "";
  for (unsigned int i = 0; i < hora.length(); i++)
    if (isDigit(hora[i])) d += hora[i];

  if (d.length() >= 14 && d.substring(0, 4).toInt() >= 2024)
    return "eval_" + d.substring(0, 8) + "_" + d.substring(8, 14);

  int i = 1;
  String id;
  do { id = "eval_SINHORA_" + String(i++); } while (SD.exists(String(DIR_EVAL) + "/" + id + "_PIM.json") ||
                                                  SD.exists(String(DIR_EVAL) + "/" + id + "_PIM.csv") ||
                                                  SD.exists(String(DIR_EVAL) + "/" + id + ".json") ||
                                                  SD.exists(String(DIR_EVAL) + "/" + id + ".csv"));
  return id;
}

// "2026-09-05 15:00:00" -> "05/09/2026 15:00" (si falta, lo saca del ID)
String fechaLegible(const String &fechaHora, const String &id) {
  if (fechaHora.length() >= 16 && fechaHora[4] == '-' && fechaHora[7] == '-')
    return fechaHora.substring(8, 10) + "/" + fechaHora.substring(5, 7) + "/" + fechaHora.substring(0, 4) +
           " " + fechaHora.substring(11, 16);

  int g = id.indexOf('_');
  if (g >= 0 && (int)id.length() >= g + 16 && isDigit(id[g + 1]))
    return id.substring(g + 7, g + 9) + "/" + id.substring(g + 5, g + 7) + "/" + id.substring(g + 1, g + 5) +
           " " + id.substring(g + 10, g + 12) + ":" + id.substring(g + 12, g + 14);
  return "Sin fecha";
}

// Devuelve cantidad de evaluaciones (más nuevas primero) o -1 si no hay SD
int listarEvaluaciones(String *ids, String *fechas, String *tipos, int max) {
  if (!storage.tarjetaPresente()) return -1;

  File dir = SD.open(DIR_EVAL);
  if (!dir || !dir.isDirectory()) return 0;

  int cnt = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String nombre = String(f.name());
    bool esDir = f.isDirectory();
    f.close();
    if (esDir) continue;

    nombre = nombre.substring(nombre.lastIndexOf('/') + 1);
    if (!nombre.endsWith(".json") || !nombre.startsWith("eval_")) continue;
    String id = nombre.substring(0, nombre.length() - 5);

    int pos = cnt;
    while (pos > 0 && id > ids[pos - 1]) pos--;       // el ID ordena por fecha
    if (pos >= max) continue;
    int fin = (cnt < max) ? cnt : max - 1;
    for (int k = fin; k > pos; k--) ids[k] = ids[k - 1];
    ids[pos] = id;
    if (cnt < max) cnt++;
  }
  dir.close();

  for (int i = 0; i < cnt; i++) {
    String tipo = "", fh = "";
    JsonDocument doc;
    File f = SD.open(String(DIR_EVAL) + "/" + ids[i] + ".json", FILE_READ);
    if (f) {
      if (!deserializeJson(doc, f)) {
        tipo = String(doc["tipo"] | "");
        fh   = String(doc["fecha_hora"] | "");
      }
      f.close();
    }
    tipo.toUpperCase();
    tipos[i]  = tipo.startsWith("PIM") ? "PIM" : tipo.startsWith("ESPIRO") ? "Espirometría" : "Desconocido";
    fechas[i] = fechaLegible(fh, ids[i]);
  }
  return cnt;
}

// SerieEval está declarada en UI.h (las structs no pueden vivir en el .ino
// porque el IDE genera los prototipos de las funciones antes que ellas)
const int MAX_PUNTOS = 3000;       // 30 s a 100 Hz

bool cargarSerieEvaluacion(const String &id, SerieEval &s) {
  File f = SD.open(String(DIR_EVAL) + "/" + id + ".csv", FILE_READ);
  if (!f) return false;

  // Columnas: tiempo_ms,presion_cmh2o,flujo_lmin (el volumen se integra acá)
  float vol = 0, tPrevMs = -1;
  while (f.available() && (int)s.t.size() < MAX_PUNTOS) {
    String linea = f.readStringUntil('\n');
    linea.trim();
    if (linea.length() == 0 || !(isDigit(linea[0]) || linea[0] == '-')) continue;   // salta el encabezado

    int c1 = linea.indexOf(',');
    int c2 = linea.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) continue;

    float tMs = linea.substring(0, c1).toFloat();
    float p   = linea.substring(c1 + 1, c2).toFloat();
    float q   = linea.substring(c2 + 1).toFloat();

    if (tPrevMs >= 0) vol += q * (tMs - tPrevMs) / 60000.0f;
    tPrevMs = tMs;

    s.t.push_back(tMs / 1000.0f);
    s.p.push_back(p);
    s.q.push_back(q);
    s.v.push_back(vol);
  }
  f.close();
  return s.t.size() >= 2;
}

void graficarEvaluacion(const String &id, bool esPim) {
  ui.mostrarMensaje("Cargando señal...");
  SerieEval s;
  if (!cargarSerieEvaluacion(id, s)) {
    ui.mostrarMensaje("No se pudo graficar", "Falta el archivo de datos", COLOR_ROJO_DEL);
    delay(2000);
    return;
  }
  int n = s.t.size();

  if (esPim) {
    ui.mostrarGrafico("Presión en el tiempo", "Tiempo (s)", "Presión (cmH2O)", s.t.data(), s.p.data(), n);
    return;
  }

  int g = 0;
  while (true) {
    int r = (g == 0)
      ? ui.mostrarGrafico("Volumen en el tiempo", "Tiempo (s)", "Volumen (L)", s.t.data(), s.v.data(), n, "Flujo / Volumen")
      : ui.mostrarGrafico("Flujo en función del volumen", "Volumen (L)", "Flujo (L/min)", s.v.data(), s.q.data(), n, "Volumen / Tiempo");
    if (r == 0) return;
    g ^= 1;
  }
}

void verEvaluacion(const String &id, const String &fecha, const String &tipo) {
  JsonDocument doc;
  File f = SD.open(String(DIR_EVAL) + "/" + id + ".json", FILE_READ);
  DeserializationError err = DeserializationError::InvalidInput;
  if (f) { err = deserializeJson(doc, f); f.close(); }
  if (err) {
    ui.mostrarMensaje("No se pudo leer", "Archivo vacío o dañado", COLOR_ROJO_DEL);
    delay(2000);
    return;
  }

  // Valor numérico del JSON, o "--" si falta o es null
  auto val = [&](const char *clave, int dec) -> String {
    return doc[clave].isNull() ? String("--") : String(doc[clave].as<float>(), dec);
  };

  MetricaItem m[6];
  int n = 0;
  bool esPim = (tipo == "PIM");

  if (esPim) {
    m[n++] = metrica("PIM sostenida", val("pim_sostenida_cmh2o", 1), "cmH2O");
  } else {
    float fvc = doc["fvc"] | 0.0f, fev1 = doc["fev1"] | 0.0f;
    bool ratioOk = !doc["fvc"].isNull() && !doc["fev1"].isNull() && fvc > 0.05f;
    m[n++] = metrica("FVC (capacidad vital forzada)", val("fvc", 2), "L");
    m[n++] = metrica("FEV1 (volumen en 1 s)", val("fev1", 2), "L");
    m[n++] = metrica("FEV1 / FVC", ratioOk ? String(100.0f * fev1 / fvc, 1) : String("--"), "%");
    m[n++] = metrica("PEF (flujo pico)", val("pef", 0), "L/min");
    m[n++] = metrica("FIVC (cap. vital inspiratoria)", val("fivc", 2), "L");
    m[n++] = metrica("FIV1 (vol. inspirado en 1 s)", val("fiv1", 2), "L");
  }

  String pie = "";
  if (!doc["temperatura_c"].isNull() || !doc["humedad_rel_pct"].isNull())
    pie = "Temp: " + val("temperatura_c", 1) + " C | Humedad: " + val("humedad_rel_pct", 0) + " %";

  while (true) {
    int r = ui.mostrarResumenCalibracion(esPim ? "Resumen: PIM" : "Resumen: Espirometría", fecha, m, n, pie);
    if (r != 1) return;
    graficarEvaluacion(id, esPim);
  }
}

void flujoHistorialCalibraciones() {
  String ids[MAX_HIST], fechas[MAX_HIST], tipos[MAX_HIST];

  ui.mostrarMensaje("Leyendo tarjeta SD...");
  int n = listarEvaluaciones(ids, fechas, tipos, MAX_HIST);
  if (n < 0) {
    ui.mostrarMensaje("No se pudo leer", "No se detectó la tarjeta SD", COLOR_ROJO_DEL);
    delay(2000);
    return;
  }

  while (true) {
    int sel = ui.elegirCalibracion(fechas, tipos, n);
    if (sel < 0) return;
    verEvaluacion(ids[sel], fechas[sel], tipos[sel]);
  }
}

void flujoHistorial() {
  while (true) {
    int op = ui.mostrarHistorial();
    if (op < 0) return;
    if (op == 0) {
      flujoHistorialCalibraciones();
    } else {
      ui.mostrarMensaje("Entrenamientos", "Próximamente", BRONQUITO_CYAN);
      delay(1500);
    }
  }
}

// ======================= CALIBRACIÓN GUIADA (PIM + ESPIROMETRÍA) =======================
// Recorrido bloqueante, como flujoHistorial(): Paso 1 (PIM) -> descanso -> Paso 2 (Espirometría).
// Cada maniobra termina en Aceptar / Repetir. Las dos quedan en RAM y recién se escriben en la SD
// cuando ambas fueron aceptadas, así que si se cancela en cualquier punto no queda nada guardado.
const uint32_t DURACION_PIM_MS = 10000;   // 10 s a 100 Hz
const uint32_t DURACION_ESP_MS = 15000;   // 15 s a 100 Hz

// Tara + captura a 100 Hz. Devuelve false si cancelaron (no queda nada en `salida`)
bool capturarManiobra(bool esPim, uint32_t duracionMs, std::vector<MuestraEspiro> &salida) {
  ui.mostrarMensaje("Realizando Tara...", "No soplar ni aspirar");
  sensoresResp.realizarTara();

  Serial.println(esPim ? "[CAL] Captura PIM" : "[CAL] Captura Espirometría");
  Serial.println("t_s,v_flujo_V,v_pim_V,Q_Lmin,P_cmH2O,V_L,fase");

  if (esPim) ui.drawCalibPimRunning();
  else       ui.drawCalibEspRunning();
  sensoresResp.iniciarManiobra();

  uint32_t tInicio = millis();
  uint32_t proxMuestra = micros();
  bool cancelado = false;
  int contador = 0;

  while (!cancelado && (millis() - tInicio < duracionMs)) {
    if ((int32_t)(micros() - proxMuestra) < 0) continue;
    proxMuestra += 10000;

    MuestraEspiro m = sensoresResp.procesarMuestra();
    sensoresResp.bufferMuestras.push_back(m);
    Serial.println(sensoresResp.obtenerLineaCSV(m));

    if (++contador % 10 == 0) {   // pantalla y botón Cancelar a 10 Hz
      if (esPim) ui.updateCalibPimProgress(m.presion_cmH2O, m.tiempo_s);
      else       ui.updateCalibEspProgress(m.flujo_Lmin, m.volumen_L, m.tiempo_s);
      if (ui.cancelarTocado()) cancelado = true;
    }
  }

  if (cancelado) {
    sensoresResp.bufferMuestras.clear();
    return false;
  }
  salida.swap(sensoresResp.bufferMuestras);
  sensoresResp.bufferMuestras.clear();
  return salida.size() > 0;
}

// Escribe <id>.csv (señal) y <id>.json (tipo, fecha, calibracion_id; las métricas se agregan después)
bool guardarManiobra(const String &id, const String &calibId, const char *tipo, const String &fechaHora,
                     const std::vector<MuestraEspiro> &datos) {
  String base = String(DIR_EVAL) + "/" + id;

  File fc = SD.open(base + ".csv", FILE_WRITE);
  if (!fc) return false;
  fc.println("tiempo_ms,presion_cmh2o,flujo_lmin");
  char buf[48];
  for (const auto &m : datos) {
    snprintf(buf, sizeof(buf), "%ld,%.2f,%.2f", lroundf(m.tiempo_s * 1000.0f), m.presion_cmH2O, m.flujo_Lmin);
    fc.println(buf);
  }
  fc.close();

  // Métricas en null hasta implementar las fórmulas (el Historial las muestra como "--")
  JsonDocument doc;
  doc["id_evaluacion"] = id;
  doc["calibracion_id"] = calibId;   // une la PIM y la espirometría de una misma calibración
  doc["tipo"] = tipo;
  if (fechaHora.length() >= 16) doc["fecha_hora"] = fechaHora;
  doc["sincronizado"] = false;
  doc["fvc"] = nullptr;
  doc["fev1"] = nullptr;
  doc["pef"] = nullptr;
  doc["fivc"] = nullptr;
  doc["fiv1"] = nullptr;
  doc["pim_sostenida_cmh2o"] = nullptr;
  doc["temperatura_c"] = nullptr;
  doc["humedad_rel_pct"] = nullptr;

  File fj = SD.open(base + ".json", FILE_WRITE);
  if (!fj) { SD.remove(base + ".csv"); return false; }
  serializeJson(doc, fj);
  fj.close();

  Serial.printf("[SD] %s: %d muestras\n", base.c_str(), (int)datos.size());
  return true;
}

void calibracionCancelada() {
  Serial.println("[CAL] Cancelada: no se guarda nada");
  ui.mostrarMensaje("Calibración cancelada", "No se guardó nada", COLOR_ROJO_DEL);
  delay(1500);
}

void flujoCalibracion() {
  std::vector<MuestraEspiro> pim, esp;

  const String lPim[4] = {
    "1. Ocluí la boquilla.",
    "2. Exhalá todo el aire.",
    "3. Inhalá con la máxima fuerza",
    "   sostenida durante 3 segundos."
  };
  const String lDescanso[3] = {
    "Descansá unos segundos y",
    "respirá con normalidad.",
    "Seguimos con la espirometría."
  };
  const String lEsp[4] = {
    "1. Inhalá al máximo fuera.",
    "2. Colocá la boquilla.",
    "3. Exhalá con máxima fuerza (6s).",
    "4. Inhalá rápido y completo."
  };

  // ---- Paso 1: PIM ----
  while (true) {
    if (!ui.mostrarPasoCalibracion("Paso 1 de 2: PIM", "Instrucciones:", lPim, 4, "COMENZAR", COLOR_VERDE_OK)) return;
    if (!capturarManiobra(true, DURACION_PIM_MS, pim)) { calibracionCancelada(); return; }

    float pico = 0;
    for (const auto &m : pim) pico = fmaxf(pico, fabsf(m.presion_cmH2O));
    int r = ui.revisarManiobra("Paso 1 de 2: PIM", String(pico, 1) + " cmH2O", "Presión pico (" + String((int)pim.size()) + " muestras)");
    if (r == 1) break;
    if (r < 0) { calibracionCancelada(); return; }
    pim.clear();   // Repetir
  }

  // ---- Descanso ----
  if (!ui.mostrarPasoCalibracion("Descanso", "Muy bien. PIM lista.", lDescanso, 3, "CONTINUAR", BRONQUITO_CYAN)) {
    calibracionCancelada();
    return;
  }

  // ---- Paso 2: Espirometría ----
  while (true) {
    if (!ui.mostrarPasoCalibracion("Paso 2 de 2: Espirometría", "Instrucciones:", lEsp, 4, "COMENZAR", COLOR_VERDE_OK)) {
      calibracionCancelada();
      return;
    }
    if (!capturarManiobra(false, DURACION_ESP_MS, esp)) { calibracionCancelada(); return; }

    float qMax = 0, vMax = 0;
    for (const auto &m : esp) {
      qMax = fmaxf(qMax, fabsf(m.flujo_Lmin));
      vMax = fmaxf(vMax, fabsf(m.volumen_L));
    }
    int r = ui.revisarManiobra("Paso 2 de 2: Espirometría", String(vMax, 2) + " L",
                               "Flujo pico " + String(qMax, 0) + " L/min");
    if (r == 1) break;
    if (r < 0) { calibracionCancelada(); return; }
    esp.clear();   // Repetir
  }

  // ---- Guardado: las dos juntas o ninguna ----
  ui.mostrarMensaje("Guardando en SD...", "Por favor aguarde");
  String calibId = generarIdEvaluacion();
  String fecha = net.fechaHoraLocal();
  String idPim = calibId + "_PIM", idEsp = calibId + "_ESP";

  bool ok = guardarManiobra(idPim, calibId, "PIM", fecha, pim);
  if (ok) ok = guardarManiobra(idEsp, calibId, "ESPIROMETRIA", fecha, esp);
  if (!ok) {   // que no quede un archivo huérfano
    SD.remove(String(DIR_EVAL) + "/" + idPim + ".csv");
    SD.remove(String(DIR_EVAL) + "/" + idPim + ".json");
    SD.remove(String(DIR_EVAL) + "/" + idEsp + ".csv");
    SD.remove(String(DIR_EVAL) + "/" + idEsp + ".json");
    Serial.println("[SD] Error al guardar la calibración");
    ui.showErrorMessage("Error en SD", "No se pudo guardar la prueba");
    delay(2000);
    return;
  }

  ui.mostrarMensaje("¡Calibración guardada!", "PIM y espirometría", COLOR_VERDE_OK);
  delay(1500);
}

void setup() {
  Serial.begin(115200);

  pinMode(15, OUTPUT); digitalWrite(15, HIGH); // TFT CS
  pinMode(19, OUTPUT); digitalWrite(19, HIGH); // SD CS
  delay(100);

  net.begin();
  ui.begin();
  sensoresResp.begin();

  StorageStatus result = storage.sync();
  if (result == ERR_NO_CARD) {
    ui.mostrarMensaje("Buscando la tarjeta microSD...");
    unsigned long t0 = millis();
    while (result == ERR_NO_CARD && millis() - t0 < 5000) {
      delay(500);
      result = storage.sync(false);
    }
  }
  manejarResultadoSincronizacion(result);

  Serial.println(">>> Dispositivo Bronquito iniciado correctamente. <<<");
}

void loop() {
  UIEvent event = ui.checkTouch(currentState);

  actualizarConexion();
  leerComandosSerie();

  if (syncInicialPendiente && currentState == STATE_LOGIN) {
    syncInicialPendiente = false;
    sincronizar();
    redibujarPantallaActual();
  }

  EstadoConexion est = calcularEstado();
  if (est != ultimoEstado) {
    ultimoEstado = est;
    ui.setEstadoConexion(est);
    redibujarPantallaActual();
  }

  if (currentState == STATE_LOGIN && (millis() - ultimoChequeoSD) > INTERVALO_SD_MS) {
    ultimoChequeoSD = millis();
    StorageStatus st = storage.sync(false);
    String clave = String((int)st) + "|" + storage.getDNI();
    if (clave != ultimaClaveSD) {
      ultimaClaveSD = clave;
      Serial.println("[FSM] La SD cambió -> actualizando login");
      ui.setDNI(storage.getDNI());
      ui.setSynced(storage.isSynced());
      ui.drawLoginScreen();
    }
  }

  switch (event) {
    case EVENT_CONNECT_WEB: {
      if (!net.estaConectado()) {
        flujoWifi();
      } else {
        int op = ui.elegirConexion(net.ssidActual());
        if (op == 0) flujoWifi();
        else if (op == 1) flujoCompu();
        else if (op == 2) {
          if (net.servidorGuardado().length() == 0) flujoCompu();
          else sincronizar();
        }
      }
      actualizarConexion();
      syncInicialPendiente = false;
      ultimoEstado = calcularEstado();
      ui.setEstadoConexion(ultimoEstado);
      redibujarPantallaActual();
      break;
    }

    case EVENT_SHOW_CONNECTION:
      ui.mostrarEstadoConexion(net.estaConectado() ? net.ssidActual() : "", net.servidorGuardado());
      redibujarPantallaActual();
      break;

    case EVENT_GOTO_PIN:
      if (storage.isSynced()) {
        Serial.println("[FSM] Perfil válido -> pantalla de PIN");
        ui.showLoginFeedback();
        delay(300);
        currentState = STATE_PIN_INPUT;
        ui.clearPin();
        ui.drawPinScreen();
      } else {
        Serial.println("[FSM] BLOQUEADO: no hay perfil cargado.");
        ui.showErrorMessage("¡Acceso bloqueado!", "Sincronizá con la web");
        delay(1800);
        ui.drawLoginScreen();
      }
      break;

    case EVENT_GOTO_LOGIN:
      Serial.println("[FSM] Volviendo a pantalla de Login");
      currentState = STATE_LOGIN;
      ui.drawLoginScreen();
      break;

    case EVENT_PIN_SUBMIT: {
      Serial.println("[FSM] Validando PIN");
      String pin = ui.getPinEntered();
      bool entra = false;
      bool usoOnline = storage.pinPendiente();

      if (usoOnline) {
        entra = (verificarPinOnline(pin) == 1);
      } else {
        entra = (pin == storage.getPin());
      }

      if (entra) {
        Serial.println("[FSM] PIN CORRECTO -> Menú Principal");
        ui.showPinStatus(true);
        delay(800);
        currentState = STATE_MAIN_MENU;
        ui.drawMainMenu();
      } else {
        Serial.println("[FSM] PIN INCORRECTO");
        if (!usoOnline) { ui.showPinStatus(false); delay(1000); }
        ui.clearPin();
        ui.drawPinScreen();
      }
      break;
    }

    case EVENT_SELECT_TRAINING:
      Serial.println("[FSM] Opción: ENTRENAMIENTO MUSCULAR");
      break;

    case EVENT_SELECT_CALIBRATION:
      Serial.println("[FSM] Entrando al Menú de Calibración");
      currentState = STATE_CALIB_MENU;
      ui.drawCalibMenu();
      break;

    case EVENT_SELECT_CONFIG: {
      Serial.println("[FSM] Opción seleccionada: CONFIGURACIÓN");
      bool actualizada = actualizarFrecuencia();
      FrecuenciaGuardada f = storage.leerFrecuencia();
      ui.mostrarFrecuencia(f.sesiones, f.dias, f.vigenteDesde, actualizada);
      redibujarPantallaActual();
      break;
    }

    case EVENT_SELECT_HISTORY:
      Serial.println("[FSM] Opción seleccionada: HISTORIAL");
      flujoHistorial();
      redibujarPantallaActual();
      break;

    case EVENT_CALIB_START:
      Serial.println("[FSM] Calibración guiada: PIM + Espirometría");
      flujoCalibracion();
      currentState = STATE_CALIB_MENU;
      ui.drawCalibMenu();
      break;

    case EVENT_CALIB_BACK_TO_MENU:
      if (currentState == STATE_CALIB_MENU) {
        Serial.println("[FSM] Volviendo al Menú Principal");
        currentState = STATE_MAIN_MENU;
        ui.drawMainMenu();
      } else {
        Serial.println("[FSM] Volviendo al Menú de Calibración");
        currentState = STATE_CALIB_MENU;
        ui.drawCalibMenu();
      }
      break;

    case EVENT_NONE:
    default:
      break;
  }
}