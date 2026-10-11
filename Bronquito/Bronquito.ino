#include "SystemState.h"
#include "UI.h"
#include "Storage.h"
#include "BTPSSensor.h"

BronquitoUI ui;
BronquitoStorage storage(5);

SystemState currentState = STATE_LOGIN;
// --- Chequeo automático de la SD mientras estamos en la pantalla de login ---
const unsigned long INTERVALO_SD_MS = 2000;
unsigned long ultimoChequeoSD = 0;
String ultimaClaveSD = "";   // para redibujar solo cuando algo cambió

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

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Desactivar ambos chips SPI antes de iniciar
  pinMode(15, OUTPUT); digitalWrite(15, HIGH); // TFT CS
  pinMode(5, OUTPUT);  digitalWrite(5, HIGH);  // SD CS
  delay(100);

  // Inicializar interfaz gráfica
  ui.begin();
  
  // Sincronizar almacenamiento
  StorageStatus result = storage.sync();
  manejarResultadoSincronizacion(result);

  Serial.println(">>> Dispositivo Bronquito iniciado correctamente. <<<");
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
  UIEvent event = ui.checkTouch(currentState);
    // Releer la SD sola cada 2 s, solo en la pantalla de login
  if (currentState == STATE_LOGIN && millis() - ultimoChequeoSD > INTERVALO_SD_MS) {
    ultimoChequeoSD = millis();
    StorageStatus st = storage.sync(false);      // false = sin imprimir en el monitor serie
    String clave = String((int)st) + "|" + storage.getDNI();
    if (clave != ultimaClaveSD) {                // cambió la tarjeta, el estado o el paciente
      ultimaClaveSD = clave;
      Serial.println("[FSM] La SD cambió -> actualizando login");
      ui.setDNI(storage.getDNI());
      ui.setSynced(storage.isSynced());
      ui.drawLoginScreen();
    }
  }

  switch (event) {
    case EVENT_CONNECT_WEB:
      Serial.println("[FSM] Sincronizando con MicroSD...");
      ui.showSyncFeedback();
      delay(400);

      {
        StorageStatus result = storage.sync();
        manejarResultadoSincronizacion(result);
      }
      break;

    case EVENT_GOTO_PIN:
      // Validación de seguridad obligatoria
      if (storage.isSynced()) {
        Serial.println("[FSM] Sincronización válida -> Cambiando a pantalla de PIN");
        ui.showLoginFeedback();
        delay(300);
        currentState = STATE_PIN_INPUT;
        ui.clearPin();
        ui.drawPinScreen();
      } else {
        Serial.println("[FSM] BLOQUEADO: Se requiere sincronizar antes de ingresar.");
        ui.showErrorMessage("! ACCESO BLOQUEADO !", "Sincronice con la MicroSD");
        delay(1800);
        ui.drawLoginScreen();
      }
      break;

    case EVENT_GOTO_LOGIN:
      Serial.println("[FSM] Volviendo a pantalla de Login");
      currentState = STATE_LOGIN;
      ui.drawLoginScreen();
      break;

    case EVENT_PIN_SUBMIT:
      Serial.print("[FSM] Validando PIN ingresado: ");
      Serial.println(ui.getPinEntered());

      if (ui.getPinEntered() == storage.getPin()) {
        Serial.println("[FSM] PIN CORRECTO -> Accediendo al Menú Principal");
        ui.showPinStatus(true);
        delay(800);
        currentState = STATE_MAIN_MENU;
        ui.drawMainMenu();
      } else {
        Serial.println("[FSM] PIN INCORRECTO");
        ui.showPinStatus(false);
        delay(1000);
        ui.clearPin();
        ui.drawPinScreen();
      }
      break;

    case EVENT_SELECT_TRAINING:
      Serial.println("[FSM] Opción seleccionada: ENTRENAMIENTO MUSCULAR");
      break;

    case EVENT_SELECT_CALIBRATION:
      Serial.println("[FSM] Opción seleccionada: CALIBRACIÓN DE SENSORES");
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