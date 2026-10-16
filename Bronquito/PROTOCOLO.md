# Comunicación ESP32 ↔ web (lado firmware)

Este documento explica **qué mensajes intercambian Bronquito (ESP32) y la web**, con el
formato exacto de cada uno, y **cómo mandar / recibir mensajes** desde el código del
firmware y desde la web. El contrato completo del lado servidor está en `DISPOSITIVO.md`
del repo de la web (`backend/app/routers/dispositivo.py`).

---

## 1. Reglas generales

- **La ESP siempre inicia.** Es un cliente HTTP; la web nunca la llama. Todo lo que la
  web quiere pedirle queda en una **cola de solicitudes** que la ESP retira cuando
  sincroniza.
- **No hay consultas periódicas ni heartbeat.** La ESP habla con la web solamente:
  1. una vez cuando el WiFi se conecta (si ya hay una compu guardada),
  2. cuando el paciente toca **SINCRONIZAR** en el menú del botón de conexión,
  3. cuando se conecta una compu nueva (se manda `identificar`),
  4. cuando hay que validar un PIN online,
  5. (más adelante) al terminar un ejercicio.
- **URL base:** `http://<ip-de-la-compu>:8000`, rutas **sin** `/api`.
  El puerto está en `PUERTO_SERVIDOR` (`BronquitoNet.h`).
- **Clave del dispositivo:** todos los pedidos llevan el header
  `X-Device-Key: <clave>`. La clave está en `Secretos.h` (`CLAVE_DISPOSITIVO`) y tiene
  que ser igual a `DEVICE_API_KEY` del `.env` / Docker de la web. Si no coincide, la web
  responde `401 clave_invalida`.
- **Formato:** body y respuestas en JSON (`Content-Type: application/json`).
  Ningún texto que viaja a la ESP lleva acentos ni ñ (la fuente de la pantalla no los
  dibuja). Las claves JSON tampoco.
- **Errores:** código HTTP real + cuerpo `{"ok": false, "error": "codigo_sin_acentos"}`.
- **El PIN nunca viaja del servidor a la ESP.** La ESP lo pide en pantalla y lo valida
  con `verificar-pin`.

### Encontrar la compu en la red local ("código")

La ESP completa sola el principio de la IP con el de su propia red (ej. `172.20.10.`)
y el paciente tipea sólo el **último número** (0 a 255), que el médico ve en la web en
**Conectar a dispositivo**. La IP queda guardada en la memoria interna (`Preferences`,
clave `ip`) y se reutiliza en las próximas sincronizaciones.

- Compu y ESP tienen que estar en la misma red (WiFi de casa o hotspot del celu; las
  redes de la facultad suelen bloquearlo).
- Del lado web, levantar con `./levantar.sh` (no `docker compose up`).
- No es un código de seguridad: la seguridad es `X-Device-Key`. En la nube desaparece.

### Errores comunes a todos los endpoints

| HTTP | `error` | Cuándo | Qué muestra la ESP |
|---|---|---|---|
| 401 | `clave_invalida` | Falta `X-Device-Key` o no coincide | `Clave del equipo invalida` |
| 503 | `dispositivo_no_configurado` | El servidor no tiene `DEVICE_API_KEY` | `Equipo no configurado en web` |
| 400 | `datos_invalidos` | JSON mal armado o tipos incorrectos | `Datos invalidos` |
| < 0 | — | No se pudo conectar (timeout, compu apagada, otra red) | `La compu no responde` |
| `SIN_RED` (-1000) | — | No hay WiFi o no hay compu guardada (no se mandó nada) | `Sin WiFi` / `Falta conectar la compu` |

Los textos salen de `BronquitoNetwork::textoError(int)`.

---

## 2. Los mensajes, uno por uno

### 2.1 `POST /dispositivo/identificar`

Primer mensaje de toda sincronización. Sirve para: avisarle a la web el estado de la
ESP, **poner en hora el reloj** y saber si hay solicitudes. También se usa para probar
la compu cuando se tipea el código (200 = compu y clave OK).

**ESP → web**

```json
{"vacia": false, "usuario_actual": "msalerno", "archivos_pendientes": 2}
```

| Campo | Tipo | Qué es | De dónde sale en el firmware |
|---|---|---|---|
| `vacia` | bool | `true` si no existe `/perfil.json` en la SD | `storage.getLastError() == ERR_FILE_NOT_FOUND` |
| `usuario_actual` | string o `null` | `usuario` del `perfil.json` | `storage.getDNI()` si `isSynced()`, si no `null` |
| `archivos_pendientes` | int | pruebas con `"sincronizado": false` | `storage.contarPendientes()` |

**web → ESP (200)**

```json
{
  "ok": true,
  "epoch": 1791817820,
  "offset_minutos": -180,
  "solicitudes_pendientes": 1,
  "usuario": "msalerno",
  "pin_version": "3f913f671e7c"
}
```

| Campo | Qué hace la ESP con él |
|---|---|
| `epoch` | segundos UTC. Se pone el reloj con `net.ponerHora(epoch, offset)` |
| `offset_minutos` | hora local = `epoch + offset_minutos * 60` (Argentina: `-180`) |
| `solicitudes_pendientes` | informativo; igual se pide la cola completa |
| `usuario` / `pin_version` | paciente asignado en la web (o `null`). Si es el **mismo** `usuario` que el de la SD pero `pin_version` cambió, la ESP pide el PIN nuevo en pantalla |

Se guarda en el struct `RespuestaIdentificar` (`ok, http, error, epoch, offsetMin,
solicitudes, usuario, pinVersion`).

> Sin hora válida (año < 2026, `HORA_MINIMA`) la ESP no deja empezar ENTRENAMIENTO ni
> CALIBRACION. La hora vive en RAM: el equipo queda en reposo sin apagarse.

### 2.2 `GET /dispositivo/solicitudes`

Retira la cola de pedidos de la web. Sin body.

**web → ESP (200)**, lista vacía si no hay nada:

```json
{"ok": true, "solicitudes": []}
```

Cada solicitud tiene `id` (int), `tipo` (string) y `payload` (objeto). Tipos:

#### RESET (formatear)

```json
{"ok": true, "solicitudes": [{"id": 3, "tipo": "RESET", "payload": {}}]}
```

La ESP muestra **ACEPTAR / RECHAZAR**. Si acepta, borra **solamente**:
`/perfil.json`, `/CONFIGURACION.JSON` y los archivos de `/EVALUACIONES/` y
`/ENTRENAMIENTOS/` (las carpetas quedan). No toca WiFi, IP de la compu ni la clave.
Si hay alguna prueba con `"sincronizado": false` **no borra nada** y responde
`archivos_pendientes`. Tras un RESET aceptado se cierra la sesión (vuelve al login).

#### CARGAR_PERFIL (asignar paciente)

```json
{
  "ok": true,
  "solicitudes": [
    {
      "id": 4,
      "tipo": "CARGAR_PERFIL",
      "payload": {
        "perfil": {
          "usuario": "therrera",
          "nombre": "Tomas",
          "apellido": "Herrera",
          "pin_version": "06384859c242",
          "sexo": "M",
          "altura": "178",
          "peso": "82.5",
          "fumador": "n",
          "nacimiento": "14/03/1998",
          "registro": "01/06/2026",
          "enfermedad": "EPOC,Asma"
        }
      }
    }
  ]
}
```

- **Todos los valores son texto** (si llega un número, el firmware lo guarda vacío).
- `usuario`: letras a-z, 4 a 25. Es el identificador del paciente (reemplaza al DNI).
- `sexo`: `F`, `M` o `X`. `fumador`: `s` / `n`. Fechas `dd/mm/aaaa`.
  `enfermedad`: separadas por coma, puede venir vacío.
- **Sin PIN.** Al aceptar, la ESP escribe `/perfil.json` (sin `pin`), y enseguida pide
  el PIN en pantalla ("Hola Tomas — Ingresa tu PIN para activar") y lo valida con
  `verificar-pin`. Si el paciente toca VOLVER, el perfil queda con el PIN pendiente y
  se valida online en el primer ingreso.

#### "Cambiar usuario" = RESET + CARGAR_PERFIL

No llegan juntos: la web crea el CARGAR_PERFIL recién cuando la ESP confirma el RESET
con `ok: true`. Por eso, después de cada tanda la ESP vuelve a pedir la cola hasta que
venga vacía (con un tope de 10 vueltas):

```
GET  /dispositivo/solicitudes               -> [{"id": 3, "tipo": "RESET", "payload": {}}]
POST /dispositivo/solicitudes/3/confirmar      {"ok": true}
GET  /dispositivo/solicitudes               -> [{"id": 4, "tipo": "CARGAR_PERFIL", ...}]
POST /dispositivo/solicitudes/4/confirmar      {"ok": true}
GET  /dispositivo/solicitudes               -> []
```

#### Otros tipos (RUTINA, etc.)

Todavía no están implementados: la ESP los confirma con
`{"ok": false, "detalle": "tipo_no_soportado"}`.

### 2.3 `POST /dispositivo/solicitudes/{id}/confirmar`

La respuesta de la ESP a cada solicitud. **Siempre** se manda, se acepte o no.

**ESP → web**

| Caso | Body |
|---|---|
| Aceptada y aplicada | `{"ok": true}` |
| El paciente tocó RECHAZAR | `{"ok": false, "detalle": "rechazado_por_usuario"}` |
| RESET con pruebas sin subir (no borró nada) | `{"ok": false, "detalle": "archivos_pendientes"}` |
| RESET que no pudo borrar algún archivo | `{"ok": false, "detalle": "error_formato"}` |
| CARGAR_PERFIL que no pudo escribir la SD | `{"ok": false, "detalle": "error_sd"}` |
| Tipo que la ESP no conoce | `{"ok": false, "detalle": "tipo_no_soportado"}` |

**web → ESP (200)**

```json
{"ok": true, "estado": "CONFIRMADA"}
```

`estado` puede ser `CONFIRMADA`, `RECHAZADA`, `ERROR` o `CANCELADA`.
404 `solicitud_inexistente` si el id no existe. Confirmar dos veces no tiene efecto.
Si se corta la luz con una solicitud en pantalla, la web la vuelve a mandar en la
próxima sincronización.

### 2.4 `POST /dispositivo/verificar-pin`

Valida el PIN que el paciente tipeó. Se usa en tres casos:
1. Al aceptar un CARGAR_PERFIL.
2. En el **primer ingreso** de un perfil que todavía no tiene PIN guardado
   (`storage.pinPendiente()`): el PIN de la pantalla de login se valida online.
3. Cuando `identificar` trae otra `pin_version` para el mismo usuario.

**ESP → web**

```json
{"usuario": "therrera", "pin": "5678"}
```

**web → ESP**

| HTTP | Respuesta | Qué hace la ESP |
|---|---|---|
| 200 | `{"ok": true, "pin_version": "06384859c242"}` | guarda `pin` y `pin_version` en `/perfil.json` (`storage.guardarPin`) y desde ahí valida **offline** |
| 401 | `{"ok": false, "error": "pin_incorrecto"}` | muestra "PIN incorrecto" y lo vuelve a pedir (sin límite de intentos) |
| 404 | `{"ok": false, "error": "sin_paciente_asignado"}` | muestra el error y sale |
| 404 | `{"ok": false, "error": "usuario_no_asignado"}` | muestra el error y sale |

El PIN no se imprime en el monitor serie (sólo se loguea `método ruta -> código`).

### 2.5 Frecuencia: `GET /dispositivo/frecuencia` + `POST /dispositivo/frecuencia/confirmar`

Cuándo: al tocar **ENTRENAMIENTO** (`actualizarFrecuencia()` en `Bronquito.ino`), si hay
WiFi, compu guardada y perfil cargado. Sin conexión sigue con la que ya tiene en la SD.
Después la pantalla de Entrenamiento muestra la que quedó en la SD (`storage.leerFrecuencia()`
+ `ui.mostrarFrecuencia()`).

**ESP → web:** `GET /dispositivo/frecuencia?usuario=therrera`

| HTTP | Respuesta | Qué hace la ESP |
|---|---|---|
| 200 | `{"ok": true, "frecuencia": {"id": 12, "sesiones_por_semana": 3, "dias_semana": [0, 2, 4], "vigente_desde": "2026-10-13"}}` | la guarda en `/CONFIGURACION.JSON` (clave `frecuencia`, `storage.guardarFrecuencia`) y confirma |
| 200 | `{"ok": true, "frecuencia": null}` | nada (el médico todavía no definió una) |
| 404 / error de red | — | nada, sigue con la que tiene |

**ESP → web:** `POST /dispositivo/frecuencia/confirmar` con `{"id_frecuencia": 12}` → `{"ok": true}`.
Con eso la web saca el punto amarillo de "¡No sincronizado con ESP!".

`dias_semana`: 0 = lunes ... 6 = domingo.

---

## 3. Flujo completo de una sincronización (`sincronizar()` en `Bronquito.ino`)

1. `POST /dispositivo/identificar` → si falla, muestra el motivo y termina.
   Si sale bien, pone el reloj en hora y el botón de conexión pasa a **verde**.
2. Si el `usuario` es el mismo de la SD y la `pin_version` cambió → pide el PIN nuevo.
3. `GET /dispositivo/solicitudes`.
4. Por cada solicitud, en orden: pantalla ACEPTAR / RECHAZAR → aplicarla →
   `POST /dispositivo/solicitudes/{id}/confirmar`.
5. Volver al paso 3 hasta que la lista venga vacía.
6. Relee la SD, actualiza el login y muestra "Sincronizado!".

Botón de conexión: **rojo** = sin WiFi, **amarillo** = WiFi pero la compu no respondió
(o no hay compu), **verde** = la compu respondió la última vez. Refleja lo último que
se supo: no se vuelve a consultar solo.

---

## 4. Cómo enviar y recibir mensajes

### 4.1 Desde la ESP (firmware)

Todo pasa por un único método de `BronquitoNetwork` (`BronquitoNet.cpp`):

```cpp
int llamarApi(const String& ip, const char* metodo, const String& ruta,
              const String& cuerpo, JsonDocument& resp);
```

- Arma `http://<ip>:8000<ruta>`, agrega `X-Device-Key` y, si hay cuerpo,
  `Content-Type: application/json`.
- Devuelve el **código HTTP** (200, 401, ...), un número **negativo** si no se pudo
  conectar, o `SIN_RED` si no hay WiFi o la IP está vacía.
- Si la respuesta trae JSON, queda parseada en `resp` (ArduinoJson v7).
- Timeouts: 3 s para conectar, 5 s para la respuesta.

**Enviar un mensaje nuevo (ejemplo):**

```cpp
JsonDocument cuerpo;
cuerpo["campo"] = "valor";
cuerpo["numero"] = 42;
String enviar;
serializeJson(cuerpo, enviar);

JsonDocument resp;
int http = net.llamarApi(net.servidorGuardado(), "POST", "/dispositivo/mi-ruta", enviar, resp);

if (http == 200 && (resp["ok"] | false)) {
  String algo = resp["algo"] | "";          // leer la respuesta
} else {
  ui.mostrarMensaje("No se pudo", net.textoError(http), COLOR_ROJO_DEL);
}
```

Para un GET sin cuerpo: `net.llamarApi(ip, "GET", "/dispositivo/...", "", resp)`.

**Recibir un tipo de solicitud nuevo:** la ESP no "escucha"; los pedidos de la web
llegan en la cola. Para soportar un tipo nuevo (ej. `RUTINA`) hay que agregar un bloque
en `atenderSolicitud()` (`Bronquito.ino`), antes del `tipo_no_soportado`:

```cpp
if (tipo == "RUTINA") {
  JsonObject datos = s["payload"];
  bool acepta = ui.pedirConfirmacion("Nueva rutina", "El medico cambio", "tu rutina");
  if (!acepta) { confirmarSolicitud(id, false, "rechazado_por_usuario"); return; }
  // ... aplicar (ej. escribir CONFIGURACION.JSON) ...
  confirmarSolicitud(id, true, "");
  return;
}
```

Siempre hay que terminar con `confirmarSolicitud(...)`; si no, la web la vuelve a
mandar en la próxima sincronización.

**Wrappers que ya existen:**

| Función | Mensaje |
|---|---|
| `net.identificar(ip, vacia, usuario, pendientes, r)` | `POST /dispositivo/identificar` |
| `net.verificarPin(usuario, pin)` → `RespuestaPin` | `POST /dispositivo/verificar-pin` |
| `confirmarSolicitud(id, ok, detalle)` (en el `.ino`) | `POST /dispositivo/solicitudes/{id}/confirmar` |
| `procesarSolicitudes()` (en el `.ino`) | `GET /dispositivo/solicitudes` en bucle |

### 4.2 Desde la web (para mandarle algo a la ESP)

La web no le habla a la ESP: crea una solicitud y espera a que la ESP sincronice.
Desde el panel del médico (o con curl, con el token del médico):

| Acción en la web | Endpoint del médico | Lo que recibe la ESP |
|---|---|---|
| Asignar paciente | `POST /medicos/me/dispositivo/asignar` `{"nombre_usuario": "msalerno"}` | `CARGAR_PERFIL` |
| Formatear | `POST /medicos/me/dispositivo/formatear` `{}` | `RESET` |
| Cambiar usuario | `POST /medicos/me/dispositivo/formatear` `{"nombre_usuario_reemplazo": "therrera"}` | `RESET` y después `CARGAR_PERFIL` |
| Cancelar un pedido | `POST /medicos/me/dispositivo/solicitudes/{id}/cancelar` | si la ESP contesta que no la aplicó, queda `CANCELADA`; si contesta `ok: true`, queda `CONFIRMADA` |
| Ver estado | `GET /medicos/me/dispositivo` | — |

Reglas: un pedido por vez (si hay uno esperando, 409); asignar sólo con el dispositivo
sin paciente. Después de crear el pedido, en la ESP tocar el botón de conexión →
**SINCRONIZAR** (o reconectar el WiFi) para que lo retire.

### 4.3 Probar sin la ESP (curl)

```bash
K=<DEVICE_API_KEY>          # la misma que CLAVE_DISPOSITIVO de Secretos.h
B=http://localhost:8000

curl -X POST $B/dispositivo/identificar -H "X-Device-Key: $K" -H 'Content-Type: application/json' \
  -d '{"vacia":true,"usuario_actual":null,"archivos_pendientes":0}'
curl $B/dispositivo/solicitudes -H "X-Device-Key: $K"
curl -X POST $B/dispositivo/solicitudes/1/confirmar -H "X-Device-Key: $K" \
  -H 'Content-Type: application/json' -d '{"ok":true}'
curl -X POST $B/dispositivo/verificar-pin -H "X-Device-Key: $K" \
  -H 'Content-Type: application/json' -d '{"usuario":"msalerno","pin":"1234"}'
```

Si `identificar` da 401 con `clave_invalida`, la red anda pero la clave de
`Secretos.h` no coincide con la del servidor.

---

## 5. Archivos de la SD involucrados

| Archivo | Contenido |
|---|---|
| `/perfil.json` | perfil del paciente (`usuario, nombre, apellido, pin_version, sexo, altura, peso, fumador, nacimiento, registro, enfermedad`) + `pin` una vez validado. Perfiles viejos con `dni` siguen funcionando |
| `/CONFIGURACION.JSON` | `frecuencia` (ver 2.5); la rutina se suma en la etapa siguiente |
| `/EVALUACIONES/`, `/ENTRENAMIENTOS/` | cada prueba = un `.json` de resumen con `"sincronizado": false/true` + un `.csv` |

El perfil se escribe a un `.tmp` y después se renombra, para no dejarlo a medias si se
corta la luz.

## 6. Comandos por el Monitor Serie (desarrollo)

| Comando | Qué hace |
|---|---|
| `FORMATEAR` | igual que un RESET (se niega si hay pruebas sin subir) |
| `FORMATEAR!` | formatea aunque haya pruebas sin subir |
| `OLVIDAR_RED` | borra red, clave e IP de la compu guardadas y desconecta el WiFi |

## 7. Pendiente (segunda etapa)

- Solicitud `RUTINA` (escribe `CONFIGURACION.JSON`).
- Subida de evaluaciones y entrenamientos (y marcar `"sincronizado": true`).
- Sincronizar al terminar un ejercicio.
