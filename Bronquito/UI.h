#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "SystemState.h"

// --- PINES DEDICADOS DEL TOUCH (CANAL HSPI) ---
#define TOUCH_CS   27
#define TOUCH_CLK  25
#define TOUCH_DIN  32
#define TOUCH_DO   35

// --- PALETA DE COLORES BRONQUITO ---
#define COLOR_FONDO       0xFFFF
#define BRONQUITO_YELLOW  0xF6AC
#define BRONQUITO_ORANGE  0xF3C4
#define BRONQUITO_CYAN    0x0559
#define BRONQUITO_BROWN   0x3944
#define COLOR_CAMPO_BG    0xF7BE
#define COLOR_TEXTO_GRIS  0x7BEF
#define COLOR_VERDE_OK    0x05E0
#define COLOR_ROJO_DEL    0xE880
#define POLLO_PICO_CLARO  0xED0D
#define POLLO_PICO_OSCURO 0xDBE7
#define POLLO_OJO_ARO     0xCC6B
#define POLLO_OJO         0x5143
#define COLOR_TARJETA_BG  0xF7DE

#define DOT_R 4

class BronquitoUI {
public:
  BronquitoUI();
  void begin();

  // Métodos de dibujo de pantallas
  void drawLoginScreen();
  void drawPinScreen();
  void drawMainMenu();

  // Feedbacks visuales, estado de sincronización y DNI del usuario
  void showSyncFeedback();
  void showLoginFeedback();
  void showPinStatus(bool correct);
  void showErrorMessage(String title, String message);
  void setSynced(bool synced) { isSynced = synced; }
  void setDNI(String dni) { userDNI = dni; }

  // Escucha de eventos táctiles
  UIEvent checkTouch(SystemState currentState);

  // Gestión de PIN
  String getPinEntered() const { return pinEntered; }
  void clearPin() { pinEntered = ""; }

private:
  TFT_eSPI tft;
  SPIClass touchSPI;
  XPT2046_Touchscreen touch;

  String pinEntered;
  String userDNI;
  bool lastTouchState;
  bool isSynced;

  // Rastro del punto rojo de depuración
  int dotX, dotY;
  bool hasDot;

  // Auxiliares privados
  void updatePinDisplay();
  void redrawArea(int x, int y, int w, int h, SystemState currentState);
  void clearTouchDot(SystemState currentState);
  void drawTouchDot(int x, int y);
};

#endif