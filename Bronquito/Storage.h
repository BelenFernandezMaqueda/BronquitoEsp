#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

#define SD_CS_DEFAULT 5

enum StorageStatus {
  STORAGE_OK,
  ERR_NO_CARD,         // No hay tarjeta SD conectada o no responde
  ERR_FILE_NOT_FOUND,  // No existe el archivo /perfil.json
  ERR_JSON_PARSE       // El archivo existe pero el formato JSON es inválido
};

struct PerfilPaciente {
  String dni;
  String pin;
  String sexo;
  String altura;
  String peso;
  String fumador;
  String nacimiento;
  String registro;
  String enfermedad;
};

class BronquitoStorage {
public:
  BronquitoStorage(uint8_t csPin = SD_CS_DEFAULT);

  bool begin();

  // Monta la SD si hace falta y lee /perfil.json.
  // verbose = false para chequeos repetidos (no llena el monitor serie).
  StorageStatus sync(bool verbose = true);

  // true si la tarjeta sigue ahí (si no, la desmonta para poder volver a montar)
  bool tarjetaPresente();

  PerfilPaciente getPerfil() const { return paciente; }
  String getPin() const { return paciente.pin; }
  String getDNI() const { return paciente.dni; }
  bool isSynced() const { return synced; }
  StorageStatus getLastError() const { return lastError; }

private:
  uint8_t chipSelectPin;
  bool sdMontada;
  PerfilPaciente paciente;
  bool synced;
  StorageStatus lastError;

  void marcarError(StorageStatus e, const char *textoDNI);
};

#endif