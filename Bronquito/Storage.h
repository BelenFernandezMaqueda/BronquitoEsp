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

enum FormatoResultado {
  FORMATO_OK,
  FORMATO_SIN_SD,      // No hay tarjeta
  FORMATO_PENDIENTES,  // Hay pruebas con "sincronizado": false (no se borró nada)
  FORMATO_ERROR        // Algún archivo no se pudo borrar
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
  String nombre;
  String apellido;
  String pinVersion;
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

  bool pinPendiente() const { return paciente.pin.length() == 0; }   // el PIN todavía no fue validado
  String getPinVersion() const { return paciente.pinVersion; }
  String getNombre() const { return paciente.nombre; }
  String getApellido() const { return paciente.apellido; }

  bool guardarPerfil(JsonVariantConst perfil);                       // escribe /perfil.json (sin PIN)
  bool guardarPin(const String& pin, const String& version);         // guarda el PIN ya validado

  // Cantidad de pruebas (.json en EVALUACIONES/ y ENTRENAMIENTOS/) con "sincronizado": false
  int contarPendientes();

  // Borra perfil.json, CONFIGURACION.JSON y los archivos de EVALUACIONES/ y ENTRENAMIENTOS/.
  // Si hay pruebas sin subir no borra nada, salvo que forzar = true.
  FormatoResultado formatearFabrica(bool forzar = false);

private:
  uint8_t chipSelectPin;
  bool sdMontada;
  PerfilPaciente paciente;
  bool synced;
  StorageStatus lastError;

  void marcarError(StorageStatus e, const char *textoDNI);
  bool escribirJson(const char *ruta, JsonDocument &doc);
};

#endif