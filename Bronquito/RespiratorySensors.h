#ifndef RESPIRATORY_SENSORS_H
#define RESPIRATORY_SENSORS_H

#include <Arduino.h>

// Métricas clínicas calculadas para una maniobra de Espirometría
struct Metricasespirometria {
  float FVC;            // Capacidad Vital Forzada (L)
  float FEV1;           // Volumen Espiratorio Forzado en el 1er segundo (L)
  float PEF;            // Flujo Espiratorio Pico (L/min)
  float Tiffeneau;      // Relación FEV1/FVC (%)
  float FIVC;           // Capacidad Vital Inspiratoria Forzada (L)
  float FIV1;           // Volumen Inspiratorio Forzado en el 1er segundo (L)
  float BEV;            // Volumen de Extrapolación Retrospectiva (L)
  float t_exhalacion;   // Tiempo total de exhalación (s)
  int codigoValidacion; // 0 = Éxito, 1..6 = Código de error 
};

// Métricas de un intento individual de PIM
struct MetricasPIM {
  float pimPico_cmH2O;      // Presión máxima instantánea alcanzada
  float pimSostenida_cmH2O; // Promedio en la ventana continua de 1.0s (70 muestras)
  float flujoFugaMax_Lmin;  // Flujo registrado para verificar oclusión de vía aérea
  float duracion_s;         // Tiempo total sostenido
  int codigoValidacion;     // 0 = Válida, 1 = Fuga/Sin oclusión, 2 = Duración <1.5s, 6 = Cancelado
};

// Historial y resultado de la sesión de PIM (hasta 8 intentos)
struct SesionPIM {
  MetricasPIM intentos[8];
  int totalIntentosRealizados;
  int indicesValidosReproducibles[3]; // Posiciones de los 3 intentos dentro del 10% de variación
  float pimReportada_cmH2O;          // El mayor valor de las 3 mediciones reproducibles
  bool sesionExitosa;                // true si se alcanzaron las 3 válidas
};

class RespiratorySensors {
private:
  uint8_t _pinFlujo;
  uint8_t _pinPIM;

  // Constantes de conversión deducidas del hardware
  const float FACTOR_VENTURI_FLUJO   = 461.10f; // Q [L/min] = 461.10 * sqrt(V_neto)
  const float FACTOR_PSA071040_CMH2O = 126.05f; // P [cmH2O] = V_diff * 126.05

  // Offsets de tara y variables ambientales
  float _vZeroFlujo;
  float _vZeroPIM;
  float _factorBTPS;
  float _tempAmb;
  float _humedadRel;

  // Buffers de adquisición estáticos (15s a 100 Hz / 5s a 70 Hz)
  static const int MAX_MUESTRAS_ESPIRO = 1500;
  static const int MAX_MUESTRAS_PIM    = 350;

  float _bufferFlujo[MAX_MUESTRAS_ESPIRO];   // Flujo en L/s (BTPS)
  float _bufferVolumen[MAX_MUESTRAS_ESPIRO]; // Volumen en L (BTPS)
  float _bufferPIM[MAX_MUESTRAS_ESPIRO];     // Presión en cmH2O
  int _totalMuestras;

  int _idxInicioEspiracion;
  int _idxFinEspiracion;

  hw_timer_t *_timer;

public:
  RespiratorySensors(uint8_t pinFlujo = 36, uint8_t pinPIM = 39);

  void begin();
  void setCondicionesAmbientales(float factorBTPS, float tempC, float humPct);
  void configurarTimerFrecuencia(uint32_t freqHz);
  bool realizarTara();

  // 1. Muestreo y Adquisición por Timer
  bool adquirirDatosEspirometria(volatile bool &cancelFlag);
  bool adquirirDatosPIM(volatile bool &cancelFlag);

  // 2. Procesamiento Matemático
  Metricasespirometria calcularMetricasEspirometria();
  MetricasPIM calcularMetricasPIM();

  // 3. Algoritmos de Validación Clínica
  int validarEspirometria(const Metricasespirometria &m);
  int validarIntentoPIM(const MetricasPIM &m);
  bool evaluarReproducibilidadPIM(SesionPIM &sesion);

  // 4. Preparar archivos para guardar en MICRO SD (JSON y CSV para otra librería de Storage)
  String prepararJSONResumenEspiro(const Metricasespirometria &m, const String &idEval, const String &fechaHora);
  String prepararJSONResumenPIM(const SesionPIM &sesion, const String &idEval, const String &fechaHora);
  String prepararCSVCurvaEspiro();
  String prepararCSVCurvaPIM();
  static String obtenerTextoErrorEspiro(int codigoError);
  static String obtenerTextoErrorPIM(int codigoError);
  int getTotalMuestras() const { return _totalMuestras; }
};

#endif