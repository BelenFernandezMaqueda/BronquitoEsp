#ifndef RESPIRATORY_SENSORS_H
#define RESPIRATORY_SENSORS_H

#include <Arduino.h>
#include <vector>

struct MuestraEspiro {
  float tiempo_s;
  float vFlujo_V;
  float vPIM_V;
  float flujo_Lmin;
  float presion_cmH2O;
  float volumen_L;
  int fase;
};

class RespiratorySensors {
public:
  RespiratorySensors(int pinFlujo = 36, int pinPIM = 39);

  void begin();
  void realizarTara();
  void iniciarManiobra();
  MuestraEspiro procesarMuestra();
  String obtenerLineaCSV(const MuestraEspiro& m) const;

  float getVZeroFlujo() const { return vZeroFlujo; }
  float getVZeroPIM() const { return vZeroPIM; }

  // Buffer de muestras guardadas en RAM
  std::vector<MuestraEspiro> bufferMuestras;

private:
  const int _pinFlujo;
  const int _pinPIM;

  // Factores de conversión y calibración
  const float K_FLUJO       = 461.10f;
  const float K_PIM         = 114.18f;
  const float ALPHA_FILTRO  = 0.30f;
  const float ZONA_MUERTA_V = 0.002f;
  const float UMBRAL_FASE   = 0.2f;
  const float ALPHA_BASE    = 0.02f;
  const int   N_FIN_EVENTO  = 5;
  const float DT            = 0.010f; // 100 Hz

  float vZeroFlujo;
  float vZeroPIM;
  float vFlujoFiltrado;
  float vPIMFiltrado;

  float volumen_L;
  int fase;
  bool enEvento;
  int muestrasSinFlujo;
  float presionBase;
  float volumenPendiente;
  uint32_t tInicio_us;

  float leerV(int pin) const;
};

#endif