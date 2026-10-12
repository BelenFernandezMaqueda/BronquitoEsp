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
enum EstadoConexion { CON_NINGUNA, CON_SOLO_WIFI, CON_WIFI_Y_COMPU };

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
  void setEstadoConexion(EstadoConexion e) { estadoCon = e; }
  int  elegirConexion(const String& ssid);

  // Mensaje centrado de dos líneas (ej: "Conectando a:", "MiRed")
  void mostrarMensaje(const String &l1, const String &l2 = "", uint16_t color = BRONQUITO_CYAN);

  // Lista de redes. Devuelve el índice elegido (0..n-1), o:
  //  -1 = reescanear, -2 = escribir el nombre a mano, -3 = volver
  int elegirRed(const String *nombres, const int *rssi, int n);

  // Teclado en pantalla (bloqueante): devuelve el texto cuando se toca OK
  String pedirTexto(const String &titulo, String inicial = "");

  // Teclado numérico (hasta 3 dígitos) con un mensaje arriba (hasta 3 líneas).
  // "prefijo" se muestra delante de lo que se escribe (ej: "10.200.0.").
  // Devuelve los dígitos al tocar OK, o "" si se toca VOLVER.
  String pedirCodigo(const String *lineas, int nLineas, const String &prefijo);

  bool   pedirConfirmacion(const String& titulo, const String& l1, const String& l2 = "");   // true = ACEPTAR
  String pedirPin(const String& titulo, const String& subtitulo = "");                       // 4 dígitos, "" si VOLVER

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
  EstadoConexion estadoCon;

  // Rastro del punto rojo de depuración
  int dotX, dotY;
  bool hasDot;

  // Auxiliares privados
  void updatePinDisplay();
  void redrawArea(int x, int y, int w, int h, SystemState currentState);
  void clearTouchDot(SystemState currentState);
  void drawTouchDot(int x, int y);

  //Para conexion wifi
  bool leerToque(int &x, int &y);
  void dibujarBoton(int x, int y, int w, int h, const String &texto, uint16_t color, int tam = 2);
  void dibujarCampoTexto(const String &texto);
  void dibujarTeclado(int capa);
};

#endif