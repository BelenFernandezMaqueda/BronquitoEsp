#include "Storage.h"
#include <vector>

BronquitoStorage::BronquitoStorage(uint8_t csPin)
  : chipSelectPin(csPin), sdMontada(false), synced(false), lastError(ERR_NO_CARD) {
  paciente.dni = "Sin Sincronizar";
  paciente.pin = "";
}

bool BronquitoStorage::begin() {
  return (sync() == STORAGE_OK);
}

void BronquitoStorage::marcarError(StorageStatus e, const char *textoDNI) {
  paciente.dni = textoDNI;
  synced = false;
  lastError = e;
}

bool BronquitoStorage::tarjetaPresente() {
  if (!sdMontada) return false;
  File raiz = SD.open("/");
  bool ok = (bool)raiz;
  if (raiz) raiz.close();
  if (!ok) {              // la sacaron: desmontar para poder volver a montar después
    SD.end();
    sdMontada = false;
  }
  return ok;
}

StorageStatus BronquitoStorage::sync(bool verbose) {
  if (verbose) {
    Serial.println("--------------------------------------------------");
    Serial.println("[STORAGE] Intentando inicializar lector MicroSD...");
  }

  // 1) Montar la tarjeta si todavía no está montada
  if (!sdMontada) {
    if (!SD.begin(chipSelectPin)) {
      if (verbose) {
        Serial.println("[STORAGE] ¡ERROR! No se detectó la tarjeta MicroSD.");
        Serial.println("--------------------------------------------------");
      }
      marcarError(ERR_NO_CARD, "Sin Sincronizar");
      return ERR_NO_CARD;
    }
    sdMontada = true;
  }

  // 2) Abrir perfil.json
  File perfilFile = SD.open("/perfil.json", FILE_READ);
  if (!perfilFile) {
    // ¿falta el archivo, o sacaron la tarjeta?
    if (!tarjetaPresente()) {
      if (verbose) {
        Serial.println("[STORAGE] ¡ERROR! La tarjeta ya no está.");
        Serial.println("--------------------------------------------------");
      }
      marcarError(ERR_NO_CARD, "Sin Sincronizar");
      return ERR_NO_CARD;
    }
    if (verbose) {
      Serial.println("[STORAGE] ¡ERROR! No se encontró /perfil.json en la raíz.");
      Serial.println("--------------------------------------------------");
    }
    marcarError(ERR_FILE_NOT_FOUND, "Sin Sincronizar");
    return ERR_FILE_NOT_FOUND;
  }

  // 3) Leer el JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, perfilFile);
  perfilFile.close();

  if (error) {
    if (verbose) {
      Serial.print("[STORAGE] ¡ERROR! Formato JSON inválido: ");
      Serial.println(error.f_str());
      Serial.println("--------------------------------------------------");
    }
    marcarError(ERR_JSON_PARSE, "Error en JSON");
    return ERR_JSON_PARSE;
  }

  String usuario = doc["usuario"] | "";
  if (usuario.length() == 0) usuario = doc["dni"] | "Desconocido";   // perfiles viejos
  paciente.dni        = usuario;                     // (el campo se sigue llamando dni por dentro)
  paciente.nombre     = doc["nombre"] | "";
  paciente.apellido   = doc["apellido"] | "";
  paciente.pin        = doc["pin"] | "";             // vacío = PIN pendiente de validar
  paciente.pinVersion = doc["pin_version"] | "";
  paciente.sexo       = doc["sexo"] | "-";
  paciente.altura     = doc["altura"] | "-";
  paciente.peso       = doc["peso"] | "-";
  paciente.fumador    = doc["fumador"] | "-";
  paciente.nacimiento = doc["nacimiento"] | "-";
  paciente.registro   = doc["registro"] | "-";
  paciente.enfermedad = doc["enfermedad"] | "-";

  synced = true;
  lastError = STORAGE_OK;

  if (verbose) {
    Serial.println("[STORAGE] ¡Perfil cargado correctamente!");
    Serial.print(" - DNI Paciente: "); Serial.println(paciente.dni);
    Serial.println("--------------------------------------------------");
  }
  return STORAGE_OK;
}

// ---------------------------------------------------------------------------
// ESCRITURA DEL PERFIL
// ---------------------------------------------------------------------------
// Escribe a un archivo temporal y después lo renombra, para no dejar el perfil a medias.
bool BronquitoStorage::escribirJson(const char *ruta, JsonDocument &doc) {
  String tmp = String(ruta) + ".tmp";
  SD.remove(tmp);
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) return false;
  size_t n = serializeJsonPretty(doc, f);
  f.close();
  if (n == 0) { SD.remove(tmp); return false; }
  SD.remove(ruta);
  return SD.rename(tmp, ruta);
}

// Guarda el perfil que llegó de la web. Solo copia los campos conocidos y NO trae PIN.
bool BronquitoStorage::guardarPerfil(JsonVariantConst perfil) {
  if (!tarjetaPresente()) return false;
  static const char *CLAVES[] = {"usuario", "nombre", "apellido", "pin_version", "sexo", "altura",
                                 "peso", "fumador", "nacimiento", "registro", "enfermedad"};
  JsonDocument doc;
  for (const char *k : CLAVES) doc[k] = String(perfil[k] | "");
  String usuario = doc["usuario"] | "";
  if (usuario.length() == 0) return false;
  return escribirJson("/perfil.json", doc);
}

// Después de validar el PIN con la web: lo guarda en la SD junto con su versión.
bool BronquitoStorage::guardarPin(const String &pin, const String &version) {
  if (!tarjetaPresente()) return false;
  File f = SD.open("/perfil.json", FILE_READ);
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  doc["pin"] = pin;
  doc["pin_version"] = version;
  if (!escribirJson("/perfil.json", doc)) return false;
  paciente.pin = pin;
  paciente.pinVersion = version;
  return true;
}

// ---------------------------------------------------------------------------
// PRUEBAS PENDIENTES Y FORMATO DE FÁBRICA
// ---------------------------------------------------------------------------
static const char *CARPETAS_PRUEBAS[] = {"/EVALUACIONES", "/ENTRENAMIENTOS"};

// Rutas completas de los archivos (no carpetas) que hay dentro de 'carpeta'
static std::vector<String> listarArchivos(const char *carpeta) {
  std::vector<String> rutas;
  File dir = SD.open(carpeta);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return rutas; }
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) rutas.push_back(String(carpeta) + "/" + f.name());
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  return rutas;
}

// Si un .json no se puede leer o no dice "sincronizado", se cuenta como pendiente
// (es preferible no borrarlo).
int BronquitoStorage::contarPendientes() {
  if (!tarjetaPresente()) return 0;
  JsonDocument filtro;
  filtro["sincronizado"] = true;

  int pendientes = 0;
  for (const char *carpeta : CARPETAS_PRUEBAS) {
    for (const String &ruta : listarArchivos(carpeta)) {
      String r = ruta;
      r.toLowerCase();
      if (!r.endsWith(".json")) continue;
      File f = SD.open(ruta, FILE_READ);
      if (!f) { pendientes++; continue; }
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filtro));
      f.close();
      if (err || !(doc["sincronizado"] | false)) pendientes++;
    }
  }
  return pendientes;
}

FormatoResultado BronquitoStorage::formatearFabrica(bool forzar) {
  if (!sdMontada && SD.begin(chipSelectPin)) sdMontada = true;
  if (!tarjetaPresente()) return FORMATO_SIN_SD;

  int pendientes = contarPendientes();
  if (!forzar && pendientes > 0) {
    Serial.printf("[STORAGE] Formato cancelado: %d pruebas sin subir\n", pendientes);
    return FORMATO_PENDIENTES;
  }

  bool ok = true;
  const char *sueltos[] = {"/perfil.json", "/CONFIGURACION.JSON"};
  for (const char *ruta : sueltos) {
    if (SD.exists(ruta) && !SD.remove(ruta)) ok = false;
  }
  for (const char *carpeta : CARPETAS_PRUEBAS) {
    for (const String &ruta : listarArchivos(carpeta)) {
      if (!SD.remove(ruta)) ok = false;
    }
  }

  paciente = PerfilPaciente();                       // olvidar al paciente (incluido el PIN)
  marcarError(ERR_FILE_NOT_FOUND, "Sin Sincronizar");
  Serial.println(ok ? "[STORAGE] Formato de fabrica OK" : "[STORAGE] Formato incompleto");
  return ok ? FORMATO_OK : FORMATO_ERROR;
}
