#include "RespiratorySensors.h"

// Variable de bandera global para la ISR del Timer por Hardware
static volatile bool flagTimerSensors = false;

void IRAM_ATTR onTimerSensorsISR() {
  flagTimerSensors = true;
}

RespiratorySensors::RespiratorySensors(uint8_t pinFlujo, uint8_t pinPIM)
  : _pinFlujo(pinFlujo), _pinPIM(pinPIM), _vZeroFlujo(1.00f), _vZeroPIM(1.65f),
    _factorBTPS(1.00f), _tempAmb(20.0f), _humedadRel(50.0f),
    _totalMuestras(0), _idxInicioEspiracion(-1), _idxFinEspiracion(-1), _timer(NULL) {}

void RespiratorySensors::begin() {
  analogSetAttenuation(ADC_11db);
  _timer = timerBegin(1000000);
  timerAttachInterrupt(_timer, &onTimerSensorsISR);
  configurarTimerFrecuencia(100);
}

void RespiratorySensors::setCondicionesAmbientales(float factorBTPS, float tempC, float humPct) {
  _factorBTPS = factorBTPS;
  _tempAmb    = tempC;
  _humedadRel = humPct;
}

void RespiratorySensors::configurarTimerFrecuencia(uint32_t freqHz) {
  uint64_t alarmValMicros = 1000000 / freqHz;
  timerAlarm(_timer, alarmValMicros, true, 0);
}

bool RespiratorySensors::realizarTara() {
  long sumaFlujo = 0;
  long sumaPIM   = 0;
  const int MUESTRAS = 50;

  for (int i = 0; i < MUESTRAS; i++) {
    sumaFlujo += analogRead(_pinFlujo);
    sumaPIM   += analogRead(_pinPIM);
    delay(20);
  }

  _vZeroFlujo = ((float)sumaFlujo / MUESTRAS / 4095.0f) * 3.3f;
  _vZeroPIM   = ((float)sumaPIM   / MUESTRAS / 4095.0f) * 3.3f;
  return true;
}

// ==========================================
// 1. ADQUISICIÓN Y MUESTREO
// ==========================================
bool RespiratorySensors::adquirirDatosEspirometria(volatile bool &cancelFlag) {
  _totalMuestras = 0;
  _idxInicioEspiracion = -1;
  _idxFinEspiracion = -1;

  configurarTimerFrecuencia(100); // 100 Hz
  realizarTara();

  unsigned long tInicioEspera = millis();
  bool inicioDetectado = false;
  float volAcumuladoATPS = 0.0f;
  const float DT = 0.01f;

  while (_totalMuestras < MAX_MUESTRAS_ESPIRO) {
    if (cancelFlag) return false;

    if (flagTimerSensors) {
      flagTimerSensors = false;

      int rawFlujo = analogRead(_pinFlujo);
      int rawPIM   = analogRead(_pinPIM);

      float vFlujo = ((float)rawFlujo / 4095.0f) * 3.3f;
      float vPIM   = ((float)rawPIM   / 4095.0f) * 3.3f;

      float vNetoFlujo = vFlujo - _vZeroFlujo;
      if (vNetoFlujo < 0.005f) vNetoFlujo = 0.0f;

      float flujo_Ls = (FACTOR_VENTURI_FLUJO * sqrt(vNetoFlujo)) / 60.0f;

      if ((vPIM - _vZeroPIM) < 0.0f) {
        flujo_Ls = -flujo_Ls; // Inspiración
      }

      if (!inicioDetectado) {
        if (flujo_Ls > 0.1f) {
          inicioDetectado = true;
          _idxInicioEspiracion = _totalMuestras;
        } else if ((millis() - tInicioEspera) > 3000) {
          return false; // Error Criterio 1: >3s sin soplar
        }
      }

      _bufferFlujo[_totalMuestras] = flujo_Ls * _factorBTPS;

      volAcumuladoATPS += (flujo_Ls * DT);
      _bufferVolumen[_totalMuestras] = volAcumuladoATPS * _factorBTPS;

      _totalMuestras++;
    }
  }
  return true;
}

bool RespiratorySensors::adquirirDatosPIM(volatile bool &cancelFlag) {
  _totalMuestras = 0;

  configurarTimerFrecuencia(70); // 70 Hz
  realizarTara();

  while (_totalMuestras < MAX_MUESTRAS_PIM) {
    if (cancelFlag) return false;

    if (flagTimerSensors) {
      flagTimerSensors = false;

      int rawPIM   = analogRead(_pinPIM);
      int rawFlujo = analogRead(_pinFlujo);

      float vPIM   = ((float)rawPIM   / 4095.0f) * 3.3f;
      float vFlujo = ((float)rawFlujo / 4095.0f) * 3.3f;

      _bufferPIM[_totalMuestras] = (vPIM - _vZeroPIM) * FACTOR_PSA071040_CMH2O;

      float vNetoFlujo = vFlujo - _vZeroFlujo;
      if (vNetoFlujo < 0.005f) vNetoFlujo = 0.0f;
      _bufferFlujo[_totalMuestras] = FACTOR_VENTURI_FLUJO * sqrt(vNetoFlujo); // Flujo fuga L/min

      _totalMuestras++;
    }
  }
  return true;
}

// ==========================================
// 2. CÁLCULO DE PARÁMETROS
// ==========================================
Metricasespirometria RespiratorySensors::calcularMetricasEspirometria() {
  Metricasespirometria m = {0};

  if (_idxInicioEspiracion < 0) {
    m.codigoValidacion = 1;
    return m;
  }

  float maxFlujo = 0.0f;
  for (int i = _idxInicioEspiracion; i < _totalMuestras; i++) {
    if (_bufferFlujo[i] > maxFlujo) maxFlujo = _bufferFlujo[i];
  }
  m.PEF = maxFlujo * 60.0f;

  float maxVol = 0.0f;
  for (int i = _idxInicioEspiracion; i < _totalMuestras; i++) {
    if (_bufferVolumen[i] > maxVol) {
      maxVol = _bufferVolumen[i];
      _idxFinEspiracion = i;
    }
  }
  m.FVC = maxVol;

  int idxFEV1 = _idxInicioEspiracion + 100;
  m.FEV1 = (idxFEV1 < _totalMuestras) ? _bufferVolumen[idxFEV1] : m.FVC;

  if (m.FVC > 0.0f) m.Tiffeneau = (m.FEV1 / m.FVC) * 100.0f;

  float minVol = m.FVC;
  for (int i = _idxFinEspiracion; i < _totalMuestras; i++) {
    if (_bufferVolumen[i] < minVol) minVol = _bufferVolumen[i];
  }
  m.FIVC = m.FVC - minVol;

  int idxFIV1 = _idxFinEspiracion + 100;
  m.FIV1 = (idxFIV1 < _totalMuestras) ? (m.FVC - _bufferVolumen[idxFIV1]) : m.FIVC;

  m.t_exhalacion = (float)(_idxFinEspiracion - _idxInicioEspiracion) * 0.01f;
  m.codigoValidacion = validarEspirometria(m);

  return m;
}

MetricasPIM RespiratorySensors::calcularMetricasPIM() {
  MetricasPIM m = {0};

  float minPresionPico = 0.0f;
  float maxFlujoFuga   = 0.0f;

  for (int i = 0; i < _totalMuestras; i++) {
    if (_bufferPIM[i] < minPresionPico) minPresionPico = _bufferPIM[i];
    if (_bufferFlujo[i] > maxFlujoFuga) maxFlujoFuga = _bufferFlujo[i];
  }

  m.pimPico_cmH2O = minPresionPico;
  m.flujoFugaMax_Lmin = maxFlujoFuga;
  m.duracion_s = (float)_totalMuestras * (1.0f / 70.0f);

  const int MUESTRAS_1SEG = 70;
  float minSumaVentana = 0.0f;

  if (_totalMuestras >= MUESTRAS_1SEG) {
    for (int i = 0; i <= _totalMuestras - MUESTRAS_1SEG; i++) {
      float sumaAux = 0.0f;
      for (int j = i; j < i + MUESTRAS_1SEG; j++) {
        sumaAux += _bufferPIM[j];
      }
      if (i == 0 || sumaAux < minSumaVentana) {
        minSumaVentana = sumaAux;
      }
    }
    m.pimSostenida_cmH2O = minSumaVentana / (float)MUESTRAS_1SEG;
  } else {
    m.pimSostenida_cmH2O = m.pimPico_cmH2O;
  }

  m.codigoValidacion = validarIntentoPIM(m);
  return m;
}

// ==========================================
// 3. VALIDACIONES
// ==========================================
int RespiratorySensors::validarEspirometria(const Metricasespirometria &m) {
  if (m.t_exhalacion < 6.0f) return 3;
  if (m.FIVC > (m.FVC * 1.05f)) return 5;

  int idxUltimoSegundo = _idxFinEspiracion - 100;
  if (idxUltimoSegundo >= _idxInicioEspiracion) {
    float dVol = _bufferVolumen[_idxFinEspiracion] - _bufferVolumen[idxUltimoSegundo];
    if (dVol >= 0.025f) return 4;
  }

  return 0;
}

int RespiratorySensors::validarIntentoPIM(const MetricasPIM &m) {
  if (m.flujoFugaMax_Lmin > 2.0f) return 1;
  if (m.duracion_s < 1.5f) return 2;
  return 0;
}

bool RespiratorySensors::evaluarReproducibilidadPIM(SesionPIM &sesion) {
  int validos[8];
  int cantValidos = 0;

  for (int i = 0; i < sesion.totalIntentosRealizados; i++) {
    if (sesion.intentos[i].codigoValidacion == 0) {
      validos[cantValidos++] = i;
    }
  }

  if (cantValidos < 3) return false;

  for (int i = 0; i < cantValidos - 2; i++) {
    for (int j = i + 1; j < cantValidos - 1; j++) {
      for (int k = j + 1; k < cantValidos; k++) {
        float v1 = abs(sesion.intentos[validos[i]].pimSostenida_cmH2O);
        float v2 = abs(sesion.intentos[validos[j]].pimSostenida_cmH2O);
        float v3 = abs(sesion.intentos[validos[k]].pimSostenida_cmH2O);

        float maxVal = max(v1, max(v2, v3));
        float minVal = min(v1, min(v2, v3));

        if (((maxVal - minVal) / maxVal) <= 0.10f) {
          sesion.indicesValidosReproducibles[0] = validos[i];
          sesion.indicesValidosReproducibles[1] = validos[j];
          sesion.indicesValidosReproducibles[2] = validos[k];
          sesion.pimReportada_cmH2O = -maxVal;
          sesion.sesionExitosa = true;
          return true;
        }
      }
    }
  }

  sesion.sesionExitosa = false;
  return false;
}

// ==========================================
// 4. PREPARACIÓN DE TRAMAS (JSON Y CSV)
// ==========================================
String RespiratorySensors::prepararJSONResumenEspiro(const Metricasespirometria &m, const String &idEval, const String &fechaHora) {
  String json = "{\n";
  json += "  \"id_evaluacion\": \"" + idEval + "\",\n";
  json += "  \"tipo\": \"ESPIROMETRIA\",\n";
  json += "  \"fecha_hora\": \"" + fechaHora + "\",\n";
  json += "  \"sincronizado\": false,\n";

  if (m.codigoValidacion == 0) {
    json += "  \"fvc\": " + String(m.FVC, 2) + ",\n";
    json += "  \"fev1\": " + String(m.FEV1, 2) + ",\n";
    json += "  \"pef\": " + String(m.PEF, 1) + ",\n";
    json += "  \"fivc\": " + String(m.FIVC, 2) + ",\n";
    json += "  \"fiv1\": " + String(m.FIV1, 2) + ",\n";
  } else {
    json += "  \"fvc\": null,\n";
    json += "  \"fev1\": null,\n";
    json += "  \"pef\": null,\n";
    json += "  \"fivc\": null,\n";
    json += "  \"fiv1\": null,\n";
  }

  json += "  \"pim_sostenida_cmh2o\": null,\n";
  json += "  \"pem_sostenida_cmh2o\": null,\n";
  json += "  \"resistencia_sugerida_cmh2o\": null,\n";
  json += "  \"temperatura_c\": " + String(_tempAmb, 1) + ",\n";
  json += "  \"humedad_rel_pct\": " + String(_humedadRel, 1) + "\n";
  json += "}";
  return json;
}

String RespiratorySensors::prepararJSONResumenPIM(const SesionPIM &sesion, const String &idEval, const String &fechaHora) {
  String json = "{\n";
  json += "  \"id_evaluacion\": \"" + idEval + "\",\n";
  json += "  \"tipo\": \"PIM\",\n";
  json += "  \"fecha_hora\": \"" + fechaHora + "\",\n";
  json += "  \"sincronizado\": false,\n";
  json += "  \"fvc\": null,\n";
  json += "  \"fev1\": null,\n";
  json += "  \"pef\": null,\n";
  json += "  \"fivc\": null,\n";
  json += "  \"fiv1\": null,\n";

  if (sesion.sesionExitosa) {
    json += "  \"pim_sostenida_cmh2o\": " + String(abs(sesion.pimReportada_cmH2O), 1) + ",\n";
  } else {
    json += "  \"pim_sostenida_cmh2o\": null,\n";
  }

  json += "  \"pem_sostenida_cmh2o\": null,\n";
  json += "  \"resistencia_sugerida_cmh2o\": null,\n";
  json += "  \"temperatura_c\": " + String(_tempAmb, 1) + ",\n";
  json += "  \"humedad_rel_pct\": " + String(_humedadRel, 1) + "\n";
  json += "}";
  return json;
}

String RespiratorySensors::prepararCSVCurvaEspiro() {
  String csv = "Tiempo_s,Flujo_Ls,Volumen_L\n";
  for (int i = 0; i < _totalMuestras; i++) {
    csv += String(i * 0.01f, 2) + ",";
    csv += String(_bufferFlujo[i], 3) + ",";
    csv += String(_bufferVolumen[i], 3) + "\n";
  }
  return csv;
}

String RespiratorySensors::prepararCSVCurvaPIM() {
  String csv = "Tiempo_s,PIM_cmH2O,FlujoFuga_Lmin\n";
  const float DT = 1.0f / 70.0f;
  for (int i = 0; i < _totalMuestras; i++) {
    csv += String(i * DT, 3) + ",";
    csv += String(_bufferPIM[i], 2) + ",";
    csv += String(_bufferFlujo[i], 2) + "\n";
  }
  return csv;
}

String RespiratorySensors::obtenerTextoErrorEspiro(int codigoError) {
  switch (codigoError) {
    case 0:
      return "Maniobra valida";
    case 1:
      return "Tardo mas de 3s en comenzar a soplar";
    case 2:
      return "Inicio vacilante / BEV muy alto (>100mL)";
    case 3:
      return "Soplido muy corto (debe durar al menos 6s)";
    case 4:
      return "Sin meseta / Corto antes de tiempo";
    case 5:
      return "Inspiracion previa incompleta (FIVC < FVC)";
    case 6:
      return "Maniobra cancelada por el usuario";
    default:
      return "Error de maniobra desconocido";
  }
}

String RespiratorySensors::obtenerTextoErrorPIM(int codigoError) {
  switch (codigoError) {
    case 0:
      return "Intento valido";
    case 1:
      return "Fuga detectada (Asegure oclusion de la boquilla)";
    case 2:
      return "Esfuerzo insuficiente (duracion menor a 1.5s)";
    case 6:
      return "Intento cancelado por el usuario / Fatiga";
    default:
      return "Error en intento PIM desconocido";
  }
}