#include "UI.h"

BronquitoUI::BronquitoUI() 
  : tft(), touchSPI(HSPI), touch(TOUCH_CS), pinEntered(""), userDNI("Sin Sincronizar"), lastTouchState(false), isSynced(false), dotX(0), dotY(0), hasDot(false) {}

void BronquitoUI::begin() {
  tft.init();
  tft.setRotation(1); // Apaisado (480x320)

  touchSPI.begin(TOUCH_CLK, TOUCH_DO, TOUCH_DIN, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(3);
}

void BronquitoUI::redrawArea(int x, int y, int w, int h, SystemState currentState) {
  tft.setViewport(x, y, w, h, false);
  if (currentState == STATE_LOGIN) {
    drawLoginScreen();
  } else if (currentState == STATE_PIN_INPUT) {
    drawPinScreen();
  } else if (currentState == STATE_MAIN_MENU) {
    drawMainMenu();
  }
  tft.resetViewport();
}

void BronquitoUI::clearTouchDot(SystemState currentState) {
  if (hasDot) {
    redrawArea(dotX - DOT_R - 1, dotY - DOT_R - 1, DOT_R * 2 + 3, DOT_R * 2 + 3, currentState);
    hasDot = false;
  }
}

void BronquitoUI::drawTouchDot(int x, int y) {
  x = constrain(x, DOT_R, 479 - DOT_R);
  y = constrain(y, DOT_R, 319 - DOT_R);
  tft.fillCircle(x, y, DOT_R, TFT_RED);
  dotX = x;
  dotY = y;
  hasDot = true;
}

// --- DIBUJO DE PANTALLA 1: LOGIN ---
void BronquitoUI::drawLoginScreen() {
  tft.fillScreen(COLOR_FONDO);

  // Mascota Pato Bronquito
  tft.fillEllipse(74, 258, 89, 96, BRONQUITO_YELLOW);     // Cuerpo
  tft.fillEllipse(-5, 197, 53, 43, BRONQUITO_YELLOW);    // Ala
  tft.fillRect(0, 197, 74, 123, BRONQUITO_YELLOW);
  tft.fillRect(10, 120, 74, 72, BRONQUITO_YELLOW);       // Espalda
  tft.fillEllipse(84, 120, 74, 69, BRONQUITO_YELLOW);    // Cabeza
  tft.fillCircle(110, 111, 14, POLLO_OJO_ARO);           // Ojo
  tft.fillCircle(110, 111, 11, POLLO_OJO);
  tft.fillCircle(113, 107, 4, COLOR_FONDO);
  tft.fillTriangle(155, 101, 190, 128, 157, 128, POLLO_PICO_CLARO); // Pico
  tft.fillTriangle(157, 127, 190, 128, 155, 141, POLLO_PICO_OSCURO);

  // Botón Conectar Web / SD (Abajo a la izquierda: 15, 265, 165, 40)
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  if (isSynced) {
    tft.fillRoundRect(15, 265, 165, 40, 6, COLOR_VERDE_OK);
    tft.drawRoundRect(15, 265, 165, 40, 6, BRONQUITO_CYAN);
    tft.setTextColor(COLOR_FONDO, COLOR_VERDE_OK);
    tft.drawString("SINCRONIZADO", 97, 285);
  } else {
    tft.fillRoundRect(15, 265, 165, 40, 6, BRONQUITO_CYAN);
    tft.drawRoundRect(15, 265, 165, 40, 6, BRONQUITO_ORANGE);
    tft.setTextColor(COLOR_FONDO, BRONQUITO_CYAN);
    tft.drawString("CONECTAR WEB / SD", 97, 285);
  }

  // Título centrado a la tarjeta (X = 332)
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.setTextSize(4);
  tft.drawString("BRONQUITO", 332, 50);

  // Tarjeta de Login
  tft.drawRoundRect(200, 125, 265, 180, 12, BRONQUITO_CYAN);
  tft.drawRoundRect(201, 126, 263, 178, 11, BRONQUITO_CYAN);

  tft.setTextSize(2);
  tft.drawString("INICIAR SESION", 332, 142);

  // Campo DNI de Usuario
  tft.fillRoundRect(215, 175, 235, 38, 6, COLOR_CAMPO_BG);
  tft.drawRoundRect(215, 175, 235, 38, 6, BRONQUITO_CYAN);
  tft.setTextColor(COLOR_TEXTO_GRIS, COLOR_CAMPO_BG);
  tft.setTextSize(2);
  
  String textoDNI = "DNI: " + userDNI;
  tft.drawString(textoDNI, 332, 186);

  // Botón INGRESAR
  tft.fillRoundRect(215, 230, 235, 48, 8, BRONQUITO_CYAN);
  tft.setTextColor(COLOR_FONDO, BRONQUITO_CYAN);
  tft.drawString("INGRESAR", 332, 245);

  tft.setTextDatum(TL_DATUM);
}

// --- DIBUJO DE PANTALLA 2: PIN ---
void BronquitoUI::updatePinDisplay() {
  int boxW = 30, boxH = 35, gap = 12, boxY = 52;
  int totalW = 4 * boxW + 3 * gap;
  int startX = 240 - totalW / 2;

  for (int i = 0; i < 4; i++) {
    int currentX = startX + i * (boxW + gap);
    tft.fillRoundRect(currentX, boxY, boxW, boxH, 5, COLOR_CAMPO_BG);
    tft.drawRoundRect(currentX, boxY, boxW, boxH, 5, BRONQUITO_CYAN);

    if (i < pinEntered.length()) {
      tft.fillCircle(currentX + boxW / 2, boxY + boxH / 2, 7, BRONQUITO_CYAN);
    }
  }
}

void BronquitoUI::drawPinScreen() {
  tft.fillScreen(COLOR_FONDO);

  // Botón Volver "<"
  tft.drawRoundRect(12, 12, 44, 36, 6, BRONQUITO_ORANGE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(BRONQUITO_ORANGE, COLOR_FONDO);
  tft.setTextSize(3);
  tft.drawString("<", 34, 31);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.setTextSize(2);
  tft.drawString("INGRESE SU PIN", 240, 18);

  updatePinDisplay();

  // Teclado Numérico
  const char* keys[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {"C", "0", "OK"}
  };

  tft.setTextDatum(MC_DATUM);
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 3; col++) {
      int x = 135 + col * 75;
      int y = 102 + row * 52;

      uint16_t btnColor = BRONQUITO_CYAN;
      if (row == 3 && col == 0) btnColor = COLOR_ROJO_DEL;
      if (row == 3 && col == 2) btnColor = COLOR_VERDE_OK;

      tft.fillRoundRect(x, y, 60, 42, 8, btnColor);
      tft.setTextColor(COLOR_FONDO, btnColor);
      tft.setTextSize((row == 3 && col == 2) ? 2 : 3);
      tft.drawString(keys[row][col], x + 30, y + 22);
    }
  }

  tft.setTextDatum(TL_DATUM);
}

// --- DIBUJO DE PANTALLA 3: MENÚ PRINCIPAL ---
void BronquitoUI::drawMainMenu() {
  tft.fillScreen(COLOR_FONDO);

  // Encabezado superior
  tft.fillRect(0, 0, 480, 45, BRONQUITO_CYAN);

  // Título del encabezado
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_FONDO, BRONQUITO_CYAN);
  tft.setTextSize(2);
  tft.drawString("Menu Principal", 15, 23);

  // Botón Cerrar Sesión
  tft.fillRoundRect(320, 6, 148, 33, 6, BRONQUITO_ORANGE);
  tft.drawRoundRect(320, 6, 148, 33, 6, COLOR_FONDO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_FONDO, BRONQUITO_ORANGE);
  tft.setTextSize(1);
  tft.drawString("Cerrar Sesion", 394, 22);

  // --- CUADRÍCULA DE 4 BOTONES (2x2) ---
  // 1. Botón Entrenamiento
  tft.fillRoundRect(25, 60, 205, 110, 10, COLOR_TARJETA_BG);
  tft.drawRoundRect(25, 60, 205, 110, 10, BRONQUITO_CYAN);
  tft.drawRoundRect(26, 61, 203, 108, 9, BRONQUITO_CYAN);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_TARJETA_BG);
  tft.setTextSize(2);
  tft.drawString("ENTRENAMIENTO", 127, 115);

  // 2. Botón Calibración
  tft.fillRoundRect(250, 60, 205, 110, 10, COLOR_TARJETA_BG);
  tft.drawRoundRect(250, 60, 205, 110, 10, BRONQUITO_CYAN);
  tft.drawRoundRect(251, 61, 203, 108, 9, BRONQUITO_CYAN);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_TARJETA_BG);
  tft.setTextSize(2);
  tft.drawString("CALIBRACION", 352, 115);

  // 3. Botón Configuración
  tft.fillRoundRect(25, 185, 205, 110, 10, COLOR_TARJETA_BG);
  tft.drawRoundRect(25, 185, 205, 110, 10, BRONQUITO_CYAN);
  tft.drawRoundRect(26, 186, 203, 108, 9, BRONQUITO_CYAN);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_TARJETA_BG);
  tft.setTextSize(2);
  tft.drawString("CONFIGURACION", 127, 240);

  // 4. Botón Historial
  tft.fillRoundRect(250, 185, 205, 110, 10, COLOR_TARJETA_BG);
  tft.drawRoundRect(250, 185, 205, 110, 10, BRONQUITO_CYAN);
  tft.drawRoundRect(251, 186, 203, 108, 9, BRONQUITO_CYAN);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_TARJETA_BG);
  tft.setTextSize(2);
  tft.drawString("HISTORIAL", 352, 240);

  tft.setTextDatum(TL_DATUM);
}

// --- FEEDBACKS VISUALES ---
void BronquitoUI::showSyncFeedback() {
  tft.fillRoundRect(15, 265, 165, 40, 6, BRONQUITO_ORANGE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_FONDO, BRONQUITO_ORANGE);
  tft.setTextSize(1);
  tft.drawString("SINCRONIZANDO...", 97, 285);
  tft.setTextDatum(TL_DATUM);
}

void BronquitoUI::showLoginFeedback() {
  tft.fillRoundRect(215, 230, 235, 48, 8, BRONQUITO_ORANGE);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COLOR_FONDO, BRONQUITO_ORANGE);
  tft.setTextSize(2);
  tft.drawString("CONECTADO!", 332, 245);
  tft.setTextDatum(TL_DATUM);
}

void BronquitoUI::showPinStatus(bool correct) {
  tft.fillRect(120, 48, 240, 45, COLOR_FONDO);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);

  if (correct) {
    tft.setTextColor(COLOR_VERDE_OK, COLOR_FONDO);
    tft.drawString("PIN CORRECTO!", 240, 62);
  } else {
    tft.setTextColor(COLOR_ROJO_DEL, COLOR_FONDO);
    tft.drawString("PIN INCORRECTO", 240, 62);
  }
  tft.setTextDatum(TL_DATUM);
}

void BronquitoUI::showErrorMessage(String title, String message) {
  tft.fillRoundRect(60, 80, 360, 160, 10, COLOR_FONDO);
  tft.drawRoundRect(60, 80, 360, 160, 10, COLOR_ROJO_DEL);
  tft.drawRoundRect(61, 81, 358, 158, 9, COLOR_ROJO_DEL);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COLOR_ROJO_DEL, COLOR_FONDO);
  tft.setTextSize(2);
  tft.drawString(title, 240, 100);

  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.setTextSize(2);
  tft.drawString(message, 240, 145);

  tft.setTextDatum(TL_DATUM);
}

// --- PROCESAMIENTO TÁCTIL Y GENERACIÓN DE EVENTOS ---
UIEvent BronquitoUI::checkTouch(SystemState currentState) {
  bool currentTouchState = touch.touched();
  UIEvent detectedEvent = EVENT_NONE;

  if (currentTouchState && !lastTouchState) {
    TS_Point p = touch.getPoint();

    int x = map(p.x, 300, 3800, 0, 480);
    int y = map(p.y, 300, 3800, 0, 320);
    x = constrain(x, 0, 480);
    y = constrain(y, 0, 320);

    clearTouchDot(currentState);

    if (currentState == STATE_LOGIN) {
      if (x >= 10 && x <= 185 && y >= 255 && y <= 315) {
        detectedEvent = EVENT_CONNECT_WEB;
      } else if (x >= 200 && x <= 460 && y >= 210 && y <= 290) {
        detectedEvent = EVENT_GOTO_PIN;
      }
    } else if (currentState == STATE_PIN_INPUT) {
      if (x >= 0 && x <= 70 && y >= 0 && y <= 60) {
        detectedEvent = EVENT_GOTO_LOGIN;
      } else {
        for (int row = 0; row < 4; row++) {
          for (int col = 0; col < 3; col++) {
            int btnX = 135 + col * 75;
            int btnY = 102 + row * 52;

            if (x >= (btnX - 6) && x <= (btnX + 66) && y >= (btnY - 5) && y <= (btnY + 47)) {
              if (row < 3) {
                int num = (row * 3) + (col + 1);
                if (pinEntered.length() < 4) {
                  pinEntered += String(num);
                  updatePinDisplay();
                }
              } else if (col == 0) {
                if (pinEntered.length() > 0) {
                  pinEntered.remove(pinEntered.length() - 1);
                  updatePinDisplay();
                }
              } else if (col == 1) {
                if (pinEntered.length() < 4) {
                  pinEntered += "0";
                  updatePinDisplay();
                }
              } else if (col == 2) {
                detectedEvent = EVENT_PIN_SUBMIT;
              }
            }
          }
        }
      }
    } else if (currentState == STATE_MAIN_MENU) {
      if (x >= 310 && x <= 475 && y >= 0 && y <= 45) {
        detectedEvent = EVENT_GOTO_LOGIN;
      } else if (x >= 25 && x <= 230 && y >= 60 && y <= 170) {
        detectedEvent = EVENT_SELECT_TRAINING;
      } else if (x >= 250 && x <= 455 && y >= 60 && y <= 170) {
        detectedEvent = EVENT_SELECT_CALIBRATION;
      } else if (x >= 25 && x <= 230 && y >= 185 && y <= 295) {
        detectedEvent = EVENT_SELECT_CONFIG;
      } else if (x >= 250 && x <= 455 && y >= 185 && y <= 295) {
        detectedEvent = EVENT_SELECT_HISTORY;
      }
    }

    drawTouchDot(x, y);
  }

  lastTouchState = currentTouchState;
  delay(15);
  return detectedEvent;
}