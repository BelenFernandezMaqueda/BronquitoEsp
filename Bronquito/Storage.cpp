#include "Storage.h"

BronquitoStorage::BronquitoStorage(uint8_t csPin)
  : chipSelectPin(csPin), sdMontada(false), synced(false), lastError(ERR_NO_CARD) {
  paciente.dni = "Sin Sincronizar";
  paciente.pin = "1234";
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

  paciente.dni        = doc["dni"] | "Desconocido";
  paciente.pin        = doc["pin"] | "1234";
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