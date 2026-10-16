#include "SystemState.h"
#include "UI.h"
#include "Storage.h"
#include "BTPSSensor.h"
#include "BronquitoNet.h"

BronquitoUI ui;
// Instanciación con los pines reales de J8 
BronquitoStorage storage(19, 22, 23, 21);
BronquitoNetwork net;

bool compuOk = false;
bool wifiAntes = false;
bool syncInicialPendiente = false;
EstadoConexion ultimoEstado = CON_NINGUNA;

SystemState currentState = STATE_LOGIN;
const unsigned long INTERVALO_SD_MS = 2000;
unsigned long ultimoChequeoSD = 0;
String ultimaClaveSD = "";

// ============================================================================
// AUXILIAR: GESTIÓN Y FEEDBACK DE ERRORES DE SINCRONIZACIÓN
// ============================================================================
void manejarResultadoSincronizacion(StorageStatus status) {
  ultimaClaveSD = String((int)status) + "|" + storage.getDNI();
  ui.setDNI(storage.getDNI());
  ui.setSynced(storage.isSynced());
  ui.drawLoginScreen();

  switch (status) {
    case ERR_NO_CARD:
      Serial.println("[FSM] Error: No hay tarjeta MicroSD");
      ui.showErrorMessage("! ERROR DE TARJETA !", "No se detecto la MicroSD");
      delay(1800);
      ui.drawLoginScreen();
      break;

    case ERR_FILE_NOT_FOUND:
      Serial.println("[FSM] Error: Archivo perfil.json no encontrado");
      ui.showErrorMessage("! ARCHIVO FALTANTE !", "No existe perfil.json");
      delay(1800);
      ui.drawLoginScreen();
      break;

    case ERR_JSON_PARSE:
      Serial.println("[FSM] Error: Formato JSON invalido");
      ui.showErrorMessage("! ARCHIVO CORRUPTO !", "Error en datos de perfil");
      delay(1800);
      ui.drawLoginScreen();
      break;

    case STORAGE_OK:
    default:
      // Sincronización exitosa
      break;
  }
}

// Relee la SD y actualiza lo que muestra el login (sin popups)
void refrescarPerfilEnUI() {
  StorageStatus st = storage.sync(false);
  ultimaClaveSD = String((int)st) + "|" + storage.getDNI();
  ui.setDNI(storage.getDNI());
  ui.setSynced(storage.isSynced());
}

// ============================================================================
// CONEXIÓN: estado del botón
// ============================================================================
EstadoConexion calcularEstado() {
  if (!net.estaConectado()) return CON_NINGUNA;
  return compuOk ? CON_WIFI_Y_COMPU : CON_SOLO_WIFI;
}

void actualizarConexion() {
  bool wifiAhora = net.estaConectado();
  if (wifiAhora && !wifiAntes && net.servidorGuardado().length() > 0)
    syncInicialPendiente = true;                 // WiFi recién conectado: sincronizar una vez
  if (!wifiAhora) compuOk = false;
  wifiAntes = wifiAhora;
}

void redibujarPantallaActual() {
  if (currentState == STATE_MAIN_MENU) ui.drawMainMenu();
  else if (currentState == STATE_LOGIN) ui.drawLoginScreen();
}

// ============================================================================
// FORMATO DE FÁBRICA (comandos por el Monitor Serie, solo para desarrollo)
// ============================================================================
void formatearDispositivo(bool forzar) {
  ui.mostrarMensaje("Formateando...");
  FormatoResultado r = storage.formatearFabrica(forzar);

  switch (r) {
    case FORMATO_OK:
      ui.mostrarMensaje("Dispositivo formateado", "Sin paciente", COLOR_VERDE_OK);
      break;
    case FORMATO_PENDIENTES:
      ui.mostrarMensaje("No se formateo", "Hay pruebas sin subir", COLOR_ROJO_DEL);
      break;
    case FORMATO_SIN_SD:
      ui.mostrarMensaje("No se formateo", "No hay tarjeta SD", COLOR_ROJO_DEL);
      break;
    default:
      ui.mostrarMensaje("Formato incompleto", "Revisa la SD", COLOR_ROJO_DEL);
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

// ============================================================================
// SINCRONIZACIÓN CON LA WEB
// ============================================================================
bool dispositivoVacio()   { return storage.getLastError() == ERR_FILE_NOT_FOUND; }
String usuarioActual()    { return storage.isSynced() ? storage.getDNI() : ""; }
int archivosPendientes()  { return storage.isSynced() ? storage.contarPendientes() : 0; }

// identificar con la IP dada. Si sale bien, pone en hora el reloj.
bool identificarCompu(const String& ip, RespuestaIdentificar& r) {
  bool ok = net.identificar(ip, dispositivoVacio(), usuarioActual(), archivosPendientes(), r);
  if (ok) net.ponerHora(r.epoch, r.offsetMin);
  compuOk = ok;
  return ok;
}

// Valida un PIN contra la web (sin límite de intentos).
// Devuelve: 1 = correcto (queda guardado), 0 = incorrecto (se puede reintentar),
//          -1 = no se pudo verificar (ya mostró el motivo)
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
  String motivo = (r.http == 404 && r.error.length() > 0) ? r.error : net.textoError(r.http);
  ui.mostrarMensaje("No se pudo verificar", motivo, COLOR_ROJO_DEL);
  delay(2200);
  return -1;
}

// Pide el PIN en pantalla hasta que sea correcto o se cancele. true si quedó validado.
bool pedirYVerificarPin(const String& titulo, const String& subtitulo) {
  while (true) {
    String pin = ui.pedirPin(titulo, subtitulo);
    if (pin.length() == 0) return false;            // VOLVER
    int v = verificarPinOnline(pin);
    if (v == 1) {
      ui.mostrarMensaje("PIN correcto!", "", COLOR_VERDE_OK);
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

// Atiende UNA solicitud: pide confirmación en pantalla, la aplica y la confirma a la web.
void atenderSolicitud(JsonObject s) {
  int id = s["id"] | 0;
  String tipo = String(s["tipo"] | "");
  Serial.println("[SYNC] Solicitud " + String(id) + ": " + tipo);

  if (tipo == "RESET") {
    bool acepta = ui.pedirConfirmacion("Formatear dispositivo", "El medico pide borrar", "los datos del paciente");
    if (!acepta) { confirmarSolicitud(id, false, "rechazado_por_usuario"); return; }

    FormatoResultado fr = storage.formatearFabrica(false);
    if (fr == FORMATO_OK) {
      confirmarSolicitud(id, true, "");
      currentState = STATE_LOGIN;                   // sin sesión
      ui.clearPin();
      refrescarPerfilEnUI();
    } else if (fr == FORMATO_PENDIENTES) {
      confirmarSolicitud(id, false, "archivos_pendientes");
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

    if (!storage.guardarPerfil(perfil)) { confirmarSolicitud(id, false, "error_sd"); return; }
    refrescarPerfilEnUI();

    // Si el paciente no pone el PIN ahora, queda pendiente y se pide al ingresar
    pedirYVerificarPin("Hola " + String(perfil["nombre"] | ""), "Ingresa tu PIN para activar");
    confirmarSolicitud(id, true, "");
    return;
  }

  confirmarSolicitud(id, false, "tipo_no_soportado");   // RUTINA y otras: etapa siguiente
}

// Retira la cola completa. Devuelve 200 si todo salió bien, o el código de error.
int procesarSolicitudes() {
  for (int vuelta = 0; vuelta < 10; vuelta++) {         // tope de seguridad
    JsonDocument resp;
    int http = net.llamarApi(net.servidorGuardado(), "GET", "/dispositivo/solicitudes", "", resp);
    if (http != 200) return http;

    JsonArray lista = resp["solicitudes"].as<JsonArray>();
    if (lista.size() == 0) return 200;                  // cola vacía: listo

    for (JsonObject s : lista) atenderSolicitud(s);
  }
  return 200;
}

// Una sincronización completa
void sincronizar() {
  ui.mostrarMensaje("Sincronizando...");
  RespuestaIdentificar r;
  if (!identificarCompu(net.servidorGuardado(), r)) {
    ui.mostrarMensaje("No se pudo sincronizar", net.textoError(r.http), COLOR_ROJO_DEL);
    delay(2200);
    return;
  }
  Serial.println("[SYNC] Hora local: " + net.fechaHoraLocal());

  // 1) ¿Le cambiaron el PIN desde la web? (mismo usuario, otra versión)
  if (storage.isSynced() && !storage.pinPendiente() && r.usuario.length() > 0 &&
      r.usuario == storage.getDNI() && r.pinVersion.length() > 0 && r.pinVersion != storage.getPinVersion()) {
    pedirYVerificarPin("PIN actualizado", "Ingresa tu nuevo PIN");
  }

  // 2) Solicitudes pendientes
  int http = procesarSolicitudes();
  refrescarPerfilEnUI();

  if (http == 200) ui.mostrarMensaje("Sincronizado!", "", COLOR_VERDE_OK);
  else             ui.mostrarMensaje("No se pudo sincronizar", net.textoError(http), COLOR_ROJO_DEL);
  delay(1200);
}

// Al entrar a ENTRENAMIENTO: trae la frecuencia vigente, pisa la de la SD y le avisa
// a la web que ya la tiene (ahí se va el punto amarillo de la web).
// Sin WiFi o sin compu sigue con la que ya tiene: entrenar no depende de la web.
// true si la SD quedó con la que mandó la web.
bool actualizarFrecuencia() {
  if (!net.estaConectado() || net.servidorGuardado().length() == 0 || !storage.isSynced()) return false;

  ui.mostrarMensaje("Actualizando frecuencia...");
  JsonDocument resp;
  int http = net.llamarApi(net.servidorGuardado(), "GET",
                           "/dispositivo/frecuencia?usuario=" + storage.getDNI(), "", resp);
  if (http != 200) return false;

  JsonObject f = resp["frecuencia"];
  if (f.isNull()) return false;                    // el médico todavía no definió una
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

// ============================================================================
// FLUJOS DE CONEXIÓN
// ============================================================================
bool flujoCompu() {
  while (true) {
    String lineas[3] = {
      "Por favor asegurese de tener la",
      "computadora en la misma red e inserte",
      "el pin que aparece en pantalla"
    };
    String codigo = ui.pedirCodigo(lineas, 3, net.prefijoRed());
    if (codigo.length() == 0) return false;               // VOLVER

    if (codigo.toInt() > 255) {
      ui.mostrarMensaje("Codigo invalido", "Debe ser de 0 a 255", COLOR_ROJO_DEL);
      delay(1800);
      continue;
    }
    String ip = net.prefijoRed() + String(codigo.toInt());

    ui.mostrarMensaje("Probando la compu...", ip);
    RespuestaIdentificar r;
    if (identificarCompu(ip, r)) {
      net.guardarServidor(ip);
      ui.mostrarMensaje("COMPU CONECTADA!", ip, COLOR_VERDE_OK);
      delay(1500);
      return true;
    }
    ui.mostrarMensaje("No responde la compu", net.textoError(r.http), COLOR_ROJO_DEL);
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
    if (sel == -3) return;       // VOLVER
    if (sel == -1) continue;     // reescanear

    String ssid;
    if (sel == -2) {             // otra red (nombre a mano)
      ssid = ui.pedirTexto("Nombre de la red");
      if (ssid == "") continue;
    } else {
      ssid = nombres[sel];
    }

    String clave = ui.pedirTexto("Clave de " + ssid);
    if (clave == "") continue;   // VOLVER: vuelve a la lista de redes

    ui.mostrarMensaje("Conectando...", ssid);
    ResultadoWifi r = net.conectar(ssid, clave);

    if (r == RES_WIFI_OK) {
      ui.mostrarMensaje("WiFi OK!", ssid, COLOR_VERDE_OK);
      delay(1000);
      wifiAntes = true;          // el WiFi ya está conectado, no es una "reconexión"
      compuOk = false;           // la compu guardada era de otra red
      flujoCompu();              // si toca VOLVER ahí, queda en amarillo
      return;
    }
    if (r == RES_WIFI_INCOMPATIBLE) {
      ui.mostrarMensaje("Red no compatible", "Use WiFi de casa o hotspot", COLOR_ROJO_DEL);
    } else {
      ui.mostrarMensaje("No se pudo conectar", "Revisa la clave", COLOR_ROJO_DEL);
    }
    delay(1800);                 // vuelve a la lista
  }
}

void setup() {
  Serial.begin(115200);

  // Desactivar ambos Chip Selects antes de iniciar los buses SPI
  pinMode(15, OUTPUT); digitalWrite(15, HIGH); // TFT CS (J9)
  pinMode(19, OUTPUT); digitalWrite(19, HIGH); // SD CS (J8)
  delay(100);

  net.begin();
  ui.begin();

  StorageStatus result = storage.sync();
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

  // CORRECCIÓN SINTÁCTICA AQUÍ: (millis() - ultimoChequeoSD)
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
        ui.showErrorMessage("! ACCESO BLOQUEADO !", "Sincronice con la web");
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
    case EVENT_SELECT_CALIBRATION:
      if (!net.horaValida()) {
        ui.mostrarMensaje("Falta poner la hora", "Conectate a la compu", COLOR_ROJO_DEL);
        delay(2200);
        redibujarPantallaActual();
        break;
      }
      Serial.println(event == EVENT_SELECT_TRAINING ? "[FSM] Opción: ENTRENAMIENTO MUSCULAR"
                                                    : "[FSM] Opción: CALIBRACIÓN DE SENSORES");
      if (event == EVENT_SELECT_TRAINING) {
        bool actualizada = actualizarFrecuencia();
        FrecuenciaGuardada f = storage.leerFrecuencia();   // siempre se muestra lo que quedó en la SD
        ui.mostrarFrecuencia(f.sesiones, f.dias, f.vigenteDesde, actualizada);
        redibujarPantallaActual();
      }
      break;

    case EVENT_SELECT_CONFIG:
      Serial.println("[FSM] Opción seleccionada: CONFIGURACIÓN");
      break;

    case EVENT_SELECT_HISTORY:
      Serial.println("[FSM] Opción seleccionada: HISTORIAL");
      break;

    case EVENT_NONE:
    default:
      break;
  }
}