#include "UI.h"

BronquitoUI::BronquitoUI() 
  : tft(), touchSPI(HSPI), touch(TOUCH_CS), pinEntered(""), userDNI("Sin Sincronizar"), lastTouchState(false), isSynced(false), estadoCon(CON_NINGUNA), dotX(0), dotY(0), hasDot(false) {}

void BronquitoUI::begin() {
  // Pulso de Reset físico para el controlador TFT (GPIO4)
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  delay(50);
  digitalWrite(4, HIGH);
  delay(100);

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

  // Botón de conexión (abajo a la izquierda)
  // rojo = nada, amarillo = solo WiFi, verde = WiFi + compu
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  uint16_t colFondo, colBorde, colTexto;
  const char* txt;
  if (estadoCon == CON_WIFI_Y_COMPU) {
    colFondo = COLOR_VERDE_OK; colBorde = BRONQUITO_CYAN; colTexto = COLOR_FONDO;
    txt = "WIFI + COMPU OK";
  } else if (estadoCon == CON_SOLO_WIFI) {
    colFondo = 0xFE60; colBorde = BRONQUITO_BROWN; colTexto = BRONQUITO_BROWN;   // amarillo
    txt = "WIFI OK - SIN COMPU";
  } else {
    colFondo = COLOR_ROJO_DEL; colBorde = BRONQUITO_ORANGE; colTexto = COLOR_FONDO;
    txt = "CONECTAR WIFI";
  }
  tft.fillRoundRect(15, 265, 165, 40, 6, colFondo);
  tft.drawRoundRect(15, 265, 165, 40, 6, colBorde);
  tft.setTextColor(colTexto, colFondo);
  tft.drawString(txt, 97, 285);

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
  
  String textoUsuario = userDNI;
  if (textoUsuario.length() > 18) textoUsuario = textoUsuario.substring(0, 17) + ".";
  tft.drawString(textoUsuario, 332, 186);

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

  // Botón de conexión (junto a Cerrar Sesión)
  uint16_t cFondo, cTexto;
  const char* cTxt;
  if (estadoCon == CON_WIFI_Y_COMPU) {
    cFondo = COLOR_VERDE_OK; cTexto = COLOR_FONDO;       cTxt = "CONECTADO";
  } else if (estadoCon == CON_SOLO_WIFI) {
    cFondo = 0xFE60;         cTexto = BRONQUITO_BROWN;   cTxt = "SIN COMPU";
  } else {
    cFondo = COLOR_ROJO_DEL; cTexto = COLOR_FONDO;       cTxt = "SIN WIFI";
  }
  tft.fillRoundRect(195, 6, 110, 33, 6, cFondo);
  tft.drawRoundRect(195, 6, 110, 33, 6, COLOR_FONDO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cTexto, cFondo);
  tft.setTextSize(1);
  tft.drawString(cTxt, 250, 22);


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
      if (x >= 190 && x <= 306 && y >= 0 && y <= 45) {
        detectedEvent = EVENT_CONNECT_WEB;
      } else if (x >= 310 && x <= 475 && y >= 0 && y <= 45) {
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

// ============================================================================
// TECLADO EN PANTALLA
// ============================================================================
static bool dentro(int px, int py, int x, int y, int w, int h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

// 3 capas: 0 minúsculas, 1 mayúsculas, 2 símbolos
static const char *CAPAS[3][4] = {
  {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
  {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
  {"!@#$%^&*()", "-_=+[]{}|\\", ":;\"',/?<>", "~`.,?!/"}
};
static const int KW = 44, KH = 40, PASO_X = 46;     // ancho, alto, separación
static const int FILA_Y0 = 80, FILA_PASO = 44;      // la fila r está en y = 80 + r*44
static const int FILA_X[4] = {10, 10, 33, 74};      // x inicial de cada fila

// true UNA sola vez por cada toque nuevo, con las coordenadas ya mapeadas
bool BronquitoUI::leerToque(int &x, int &y) {
  bool ahora = touch.touched();
  bool nuevo = ahora && !lastTouchState;
  lastTouchState = ahora;
  if (nuevo) {
    TS_Point p = touch.getPoint();
    x = constrain(map(p.x, 300, 3800, 0, 480), 0, 479);
    y = constrain(map(p.y, 300, 3800, 0, 320), 0, 319);
  }
  delay(15);
  return nuevo;
}

void BronquitoUI::dibujarBoton(int x, int y, int w, int h, const String &texto, uint16_t color, int tam) {
  tft.fillRoundRect(x, y, w, h, 6, color);
  tft.setTextColor(COLOR_FONDO, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(tam);
  tft.drawString(texto, x + w / 2, y + h / 2);
}

void BronquitoUI::dibujarCampoTexto(const String &texto) {
  tft.fillRoundRect(10, 34, 460, 38, 6, COLOR_CAMPO_BG);
  tft.drawRoundRect(10, 34, 460, 38, 6, BRONQUITO_CYAN);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(BRONQUITO_BROWN, COLOR_CAMPO_BG);
  tft.setTextSize(2);
  String vista = texto;
  if (vista.length() > 36) vista = vista.substring(vista.length() - 36);   // últimos 36
  tft.drawString(vista + "_", 18, 53);
}

void BronquitoUI::dibujarTeclado(int capa) {
  tft.fillRect(0, FILA_Y0 - 2, 480, 240, COLOR_FONDO);
  for (int r = 0; r < 4; r++) {
    const char *fila = CAPAS[capa][r];
    for (int i = 0; fila[i]; i++) {
      dibujarBoton(FILA_X[r] + i * PASO_X, FILA_Y0 + r * FILA_PASO, KW, KH, String(fila[i]), BRONQUITO_BROWN);
    }
  }
  int y3 = FILA_Y0 + 3 * FILA_PASO, y4 = FILA_Y0 + 4 * FILA_PASO;
  dibujarBoton(10, y3, 60, KH, "Aa", capa == 1 ? BRONQUITO_ORANGE : COLOR_TEXTO_GRIS);
  dibujarBoton(400, y3, 70, KH, "<-", COLOR_ROJO_DEL);
  dibujarBoton(10, y4, 70, KH, capa == 2 ? "abc" : "123", COLOR_TEXTO_GRIS);
  dibujarBoton(84, y4, 250, KH, "espacio", BRONQUITO_BROWN, 1);
  dibujarBoton(338, y4, 60, KH, ".", BRONQUITO_BROWN);
  dibujarBoton(402, y4, 68, KH, "OK", COLOR_VERDE_OK);
}

// Devuelve el texto escrito, o "" si se toca VOLVER (OK con el campo vacío no hace nada)
String BronquitoUI::pedirTexto(const String &titulo, String texto) {
  hasDot = false;      // el punto rojo de depuración queda tapado por el teclado
  int capa = 0;

  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.setTextSize(2);
  tft.drawString(titulo.substring(0, 26), 10, 8);
  dibujarBoton(380, 4, 90, 28, "VOLVER", COLOR_TEXTO_GRIS, 1);
  dibujarCampoTexto(texto);
  dibujarTeclado(capa);

  int y3 = FILA_Y0 + 3 * FILA_PASO, y4 = FILA_Y0 + 4 * FILA_PASO;

  while (true) {
    int x, y;
    if (!leerToque(x, y)) continue;
    int capaAntes = capa;

    // VOLVER: cancela y devuelve vacío al instante
    if (dentro(x, y, 380, 4, 90, 28)) { tft.setTextDatum(TL_DATUM); return ""; }

    // 1) teclas de caracteres
    for (int r = 0; r < 4; r++) {
      if (!dentro(x, y, 0, FILA_Y0 + r * FILA_PASO, 480, KH)) continue;
      const char *fila = CAPAS[capa][r];
      for (int i = 0; fila[i]; i++) {
        if (dentro(x, y, FILA_X[r] + i * PASO_X, 0, KW, 480) && texto.length() < 63) {
          texto += fila[i];
          if (capa == 1) capa = 0;     // la mayúscula vale para una sola letra
        }
      }
    }
    // 2) teclas especiales
    if (dentro(x, y, 10, y3, 60, KH))        capa = (capa == 1) ? 0 : 1;      // Aa
    else if (dentro(x, y, 400, y3, 70, KH))  { if (texto.length()) texto.remove(texto.length() - 1); }
    else if (dentro(x, y, 10, y4, 70, KH))   capa = (capa == 2) ? 0 : 2;      // 123 / abc
    else if (dentro(x, y, 84, y4, 250, KH))  { if (texto.length() < 63) texto += ' '; }
    else if (dentro(x, y, 338, y4, 60, KH))  { if (texto.length() < 63) texto += '.'; }
    else if (dentro(x, y, 402, y4, 68, KH))  {                                // OK
      if (texto.length() > 0) { tft.setTextDatum(TL_DATUM); return texto; }
    }

    dibujarCampoTexto(texto);
    if (capa != capaAntes) dibujarTeclado(capa);
  }
}

void BronquitoUI::mostrarMensaje(const String &l1, const String &l2, uint16_t color) {
  hasDot = false;
  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(color, COLOR_FONDO);
  tft.drawString(l1, 240, 130);
  tft.setTextColor(COLOR_TEXTO_GRIS, COLOR_FONDO);
  tft.drawString(l2, 240, 170);
  tft.setTextDatum(TL_DATUM);
}

int BronquitoUI::elegirRed(const String *nombres, const int *rssi, int n) {
  hasDot = false;
  if (n > 6) n = 6;

  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.setTextSize(2);
  tft.drawString("Elegi tu red WiFi", 10, 8);

  tft.setTextDatum(ML_DATUM);
  for (int i = 0; i < n; i++) {
    int y = 36 + i * 40;
    tft.fillRoundRect(10, y, 460, 36, 6, COLOR_CAMPO_BG);
    tft.setTextColor(BRONQUITO_BROWN, COLOR_CAMPO_BG);
    tft.setTextSize(2);
    tft.drawString(nombres[i].substring(0, 28), 20, y + 18);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXTO_GRIS, COLOR_CAMPO_BG);
    tft.drawString(String(rssi[i]) + " dBm", 400, y + 18);
  }
  if (n == 0) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_ROJO_DEL, COLOR_FONDO);
    tft.drawString("No encontre redes", 20, 100);
  }
  dibujarBoton(10, 282, 150, 34, "REESCANEAR", BRONQUITO_CYAN);
  dibujarBoton(165, 282, 150, 34, "OTRA RED", COLOR_TEXTO_GRIS);
  dibujarBoton(320, 282, 150, 34, "VOLVER", COLOR_ROJO_DEL);
  tft.setTextDatum(TL_DATUM);

  while (true) {
    int x, y;
    if (!leerToque(x, y)) continue;
    for (int i = 0; i < n; i++)
      if (dentro(x, y, 10, 36 + i * 40, 460, 36)) return i;
    if (dentro(x, y, 10, 282, 150, 34))  return -1;
    if (dentro(x, y, 165, 282, 150, 34)) return -2;
    if (dentro(x, y, 320, 282, 150, 34)) return -3;
  }
}

String BronquitoUI::pedirCodigo(const String *lineas, int nLineas, const String &prefijo) {
  hasDot = false;
  String digitos = "";

  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(BRONQUITO_BROWN, COLOR_FONDO);
  tft.setTextSize(2);
  for (int i = 0; i < nLineas && i < 3; i++) tft.drawString(lineas[i], 10, 8 + i * 24);

  const char *teclas[4][3] = {{"1", "2", "3"}, {"4", "5", "6"}, {"7", "8", "9"}, {"<-", "0", "OK"}};
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      uint16_t color = BRONQUITO_BROWN;
      if (r == 3 && c == 0) color = COLOR_ROJO_DEL;
      if (r == 3 && c == 2) color = COLOR_VERDE_OK;
      dibujarBoton(95 + c * 100, 134 + r * 44, 90, 40, teclas[r][c], color);
    }
  }
  dibujarBoton(10, 266, 75, 40, "VOLVER", COLOR_TEXTO_GRIS, 1);

  auto mostrarCampo = [&]() {
    tft.fillRoundRect(10, 84, 460, 40, 6, COLOR_CAMPO_BG);
    tft.drawRoundRect(10, 84, 460, 40, 6, BRONQUITO_CYAN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(BRONQUITO_BROWN, COLOR_CAMPO_BG);
    tft.setTextSize(3);
    tft.drawString(prefijo + digitos + "_", 240, 104);
  };
  mostrarCampo();

  while (true) {
    int x, y;
    if (!leerToque(x, y)) continue;

    if (dentro(x, y, 10, 266, 75, 40)) {          // VOLVER
      tft.setTextDatum(TL_DATUM);
      return "";
    }
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 3; c++) {
        if (!dentro(x, y, 95 + c * 100, 134 + r * 44, 90, 40)) continue;
        if (r < 3 || c == 1) {                    // dígitos
          if (digitos.length() < 3) digitos += teclas[r][c];
        } else if (c == 0) {                      // borrar
          if (digitos.length() > 0) digitos.remove(digitos.length() - 1);
        } else if (digitos.length() > 0) {        // OK
          tft.setTextDatum(TL_DATUM);
          return digitos;
        }
      }
    }
    mostrarCampo();
  }
}

// Menú del botón de conexión. Devuelve:
// 2 = sincronizar, 0 = cambiar red, 1 = cambiar compu, -1 = volver
int BronquitoUI::elegirConexion(const String& ssid) {
  hasDot = false;
  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(BRONQUITO_BROWN, COLOR_FONDO);
  tft.setTextSize(2);
  tft.drawString("Conectado a:", 240, 22);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.setTextSize(ssid.length() <= 14 ? 3 : 2);
  tft.drawString(ssid.substring(0, 28), 240, 56);

  dibujarBoton(90, 90,  300, 46, "SINCRONIZAR",   COLOR_VERDE_OK);
  dibujarBoton(90, 146, 300, 46, "CAMBIAR RED",   BRONQUITO_CYAN);
  dibujarBoton(90, 202, 300, 46, "CAMBIAR COMPU", BRONQUITO_ORANGE);
  dibujarBoton(90, 258, 300, 46, "VOLVER",        COLOR_TEXTO_GRIS);
  tft.setTextDatum(TL_DATUM);

  while (true) {
    int x, y;
    if (!leerToque(x, y)) continue;
    if (dentro(x, y, 90, 90,  300, 46)) return 2;
    if (dentro(x, y, 90, 146, 300, 46)) return 0;
    if (dentro(x, y, 90, 202, 300, 46)) return 1;
    if (dentro(x, y, 90, 258, 300, 46)) return -1;
  }
}

// Pantalla de confirmación: devuelve true si se toca ACEPTAR, false si RECHAZAR
bool BronquitoUI::pedirConfirmacion(const String &titulo, const String &l1, const String &l2) {
  hasDot = false;
  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.drawString(titulo.substring(0, 30), 240, 50);
  tft.setTextColor(BRONQUITO_BROWN, COLOR_FONDO);
  tft.drawString(l1.substring(0, 36), 240, 115);
  tft.setTextColor(COLOR_TEXTO_GRIS, COLOR_FONDO);
  tft.drawString(l2.substring(0, 36), 240, 150);
  dibujarBoton(40, 220, 190, 60, "RECHAZAR", COLOR_ROJO_DEL);
  dibujarBoton(250, 220, 190, 60, "ACEPTAR", COLOR_VERDE_OK);
  tft.setTextDatum(TL_DATUM);

  while (true) {
    int x, y;
    if (!leerToque(x, y)) continue;
    if (dentro(x, y, 40, 220, 190, 60))  return false;
    if (dentro(x, y, 250, 220, 190, 60)) return true;
  }
}

// Teclado numérico para el PIN (4 dígitos, se muestra con asteriscos). "" si se toca VOLVER.
String BronquitoUI::pedirPin(const String &titulo, const String &subtitulo) {
  hasDot = false;
  String pin = "";

  tft.fillScreen(COLOR_FONDO);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(BRONQUITO_CYAN, COLOR_FONDO);
  tft.drawString(titulo.substring(0, 36), 10, 8);
  tft.setTextColor(BRONQUITO_BROWN, COLOR_FONDO);
  tft.drawString(subtitulo.substring(0, 36), 10, 36);

  const char *teclas[4][3] = {{"1", "2", "3"}, {"4", "5", "6"}, {"7", "8", "9"}, {"<-", "0", "OK"}};
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      uint16_t color = BRONQUITO_BROWN;
      if (r == 3 && c == 0) color = COLOR_ROJO_DEL;
      if (r == 3 && c == 2) color = COLOR_VERDE_OK;
      dibujarBoton(95 + c * 100, 134 + r * 44, 90, 40, teclas[r][c], color);
    }
  }
  dibujarBoton(10, 266, 75, 40, "VOLVER", COLOR_TEXTO_GRIS, 1);

  auto mostrarCampo = [&]() {
    String estrellas = "";
    for (unsigned int i = 0; i < pin.length(); i++) estrellas += "*";
    tft.fillRoundRect(10, 84, 460, 40, 6, COLOR_CAMPO_BG);
    tft.drawRoundRect(10, 84, 460, 40, 6, BRONQUITO_CYAN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(BRONQUITO_BROWN, COLOR_CAMPO_BG);
    tft.setTextSize(3);
    tft.drawString(estrellas + "_", 240, 104);
  };
  mostrarCampo();

  while (true) {
    int x, y;
    if (!leerToque(x, y)) continue;

    if (dentro(x, y, 10, 266, 75, 40)) { tft.setTextDatum(TL_DATUM); return ""; }   // VOLVER
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 3; c++) {
        if (!dentro(x, y, 95 + c * 100, 134 + r * 44, 90, 40)) continue;
        if (r < 3 || c == 1) {                        // dígitos
          if (pin.length() < 4) pin += teclas[r][c];
        } else if (c == 0) {                          // borrar
          if (pin.length() > 0) pin.remove(pin.length() - 1);
        } else if (pin.length() == 4) {               // OK solo con los 4 dígitos
          tft.setTextDatum(TL_DATUM);
          return pin;
        }
      }
    }
    mostrarCampo();
  }
}
