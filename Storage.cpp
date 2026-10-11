#include "Storage.h"

BronquitoStorage::BronquitoStorage(uint8_t csPin) 
  : chipSelectPin(csPin), synced(false), lastError(ERR_NO_CARD) {
  paciente.dni = "Sin Sincronizar";
  paciente.pin = "1234";
}

bool BronquitoStorage::begin() {
  return (sync() == STORAGE_OK);
}

StorageStatus BronquitoStorage::sync() {
  Serial.println("--------------------------------------------------");
  Serial.println("[STORAGE] Intentando inicializar lector MicroSD...");

  if (!SD.begin(chipSelectPin)) {
    Serial.println("[STORAGE] ¡ERROR! No se detectó la tarjeta MicroSD.");
    paciente.dni = "Sin Sincronizar";
    synced = false;
    lastError = ERR_NO_CARD;
    Serial.println("--------------------------------------------------");
    return ERR_NO_CARD;
  }

  File perfilFile = SD.open("/perfil.json", FILE_READ);
  if (!perfilFile) {
    Serial.println("[STORAGE] ¡ERROR! No se encontró /perfil.json en la raíz.");
    paciente.dni = "Sin Sincronizar";
    synced = false;
    lastError = ERR_FILE_NOT_FOUND;
    Serial.println("--------------------------------------------------");
    return ERR_FILE_NOT_FOUND;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, perfilFile);
  perfilFile.close();

  if (error) {
    Serial.print("[STORAGE] ¡ERROR! Formato JSON inválido: ");
    Serial.println(error.f_str());
    paciente.dni = "Error en JSON";
    synced = false;
    lastError = ERR_JSON_PARSE;
    Serial.println("--------------------------------------------------");
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

  Serial.println("[STORAGE] ¡Perfil cargado correctamente!");
  Serial.print(" - DNI Paciente: "); Serial.println(paciente.dni);
  Serial.println("--------------------------------------------------");

  return STORAGE_OK;
}