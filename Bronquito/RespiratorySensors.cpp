#include "RespiratorySensors.h"

RespiratorySensors::RespiratorySensors(int pinFlujo, int pinPIM)
  : _pinFlujo(pinFlujo), _pinPIM(pinPIM),
    vZeroFlujo(0.0f), vZeroPIM(0.0f),
    vFlujoFiltrado(0.0f), vPIMFiltrado(0.0f),
    volumen_L(0.0f), fase(0), enEvento(false),
    muestrasSinFlujo(0), presionBase(0.0f),
    volumenPendiente(0.0f), tInicio_us(0) {}

void RespiratorySensors::begin() {
  pinMode(_pinFlujo, INPUT);
  pinMode(_pinPIM, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

float RespiratorySensors::leerV(int pin) const {
  uint32_t suma = 0;
  const int N_PROM = 4;
  for (int i = 0; i < N_PROM; i++) {
    suma += analogReadMilliVolts(pin);
  }
  return suma / (1000.0f * N_PROM);
}

void RespiratorySensors::realizarTara() {
  const int MUESTRAS_TARA = 200;
  float sumaFlujo = 0.0f, sumaPIM = 0.0f;

  for (int i = 0; i < MUESTRAS_TARA; i++) {
    sumaFlujo += leerV(_pinFlujo);
    sumaPIM   += leerV(_pinPIM);
    delay(5);
  }

  vZeroFlujo = sumaFlujo / MUESTRAS_TARA;
  vZeroPIM   = sumaPIM   / MUESTRAS_TARA;

  vFlujoFiltrado = vZeroFlujo;
  vPIMFiltrado   = vZeroPIM;

  iniciarManiobra();
}

void RespiratorySensors::iniciarManiobra() {
  volumen_L        = 0.0f;
  fase             = 0;
  enEvento         = false;
  muestrasSinFlujo = 0;
  presionBase      = 0.0f;
  volumenPendiente = 0.0f;
  tInicio_us       = micros();

  bufferMuestras.clear();
  bufferMuestras.reserve(1500); // Espacio reservado para 15 segundos a 100 Hz
}

MuestraEspiro RespiratorySensors::procesarMuestra() {
  float vFlujoBruto = leerV(_pinFlujo);
  float vPIMBruto   = leerV(_pinPIM);

  vFlujoFiltrado = ALPHA_FILTRO * vFlujoBruto + (1.0f - ALPHA_FILTRO) * vFlujoFiltrado;
  vPIMFiltrado   = ALPHA_FILTRO * vPIMBruto   + (1.0f - ALPHA_FILTRO) * vPIMFiltrado;

  float presion_cmH2O = K_PIM * (vPIMFiltrado - vZeroPIM);
  float vNetoFlujo = vFlujoFiltrado - vZeroFlujo;
  float magnitud_Lmin = (vNetoFlujo > ZONA_MUERTA_V) ? K_FLUJO * sqrtf(vNetoFlujo) : 0.0f;

  float flujo_Lmin = 0.0f;

  if (magnitud_Lmin == 0.0f) {
    muestrasSinFlujo++;
    if (muestrasSinFlujo >= N_FIN_EVENTO) {
      enEvento = false;
      fase = 0;
      volumenPendiente = 0.0f;
    }
    if (!enEvento) {
      presionBase += ALPHA_BASE * (presion_cmH2O - presionBase);
    }
  } else {
    muestrasSinFlujo = 0;
    if (!enEvento) {
      enEvento = true;
      fase = 0;
      volumenPendiente = 0.0f;
    }

    if (fase == 0) {
      float dP = presion_cmH2O - presionBase;
      if (dP > UMBRAL_FASE)       fase =  1;
      else if (dP < -UMBRAL_FASE) fase = -1;

      if (fase == 0) {
        volumenPendiente += (magnitud_Lmin / 60.0f) * DT;
      } else {
        volumen_L += fase * volumenPendiente;
        volumenPendiente = 0.0f;
      }
    }

    if (fase != 0) {
      flujo_Lmin = fase * magnitud_Lmin;
    }
  }

  volumen_L += (flujo_Lmin / 60.0f) * DT;

  MuestraEspiro m;
  m.tiempo_s      = (micros() - tInicio_us) / 1e6f;
  m.vFlujo_V      = vFlujoFiltrado;
  m.vPIM_V        = vPIMFiltrado;
  m.flujo_Lmin    = flujo_Lmin;
  m.presion_cmH2O = presion_cmH2O;
  m.volumen_L     = volumen_L;
  m.fase          = (magnitud_Lmin > 0.0f) ? fase : 0;

  return m;
}

String RespiratorySensors::obtenerLineaCSV(const MuestraEspiro& m) const {
  char buf[128];
  snprintf(buf, sizeof(buf), "%.3f,%.4f,%.4f,%.2f,%.2f,%.4f,%d",
           m.tiempo_s, m.vFlujo_V, m.vPIM_V, m.flujo_Lmin, m.presion_cmH2O, m.volumen_L, m.fase);
  return String(buf);
}