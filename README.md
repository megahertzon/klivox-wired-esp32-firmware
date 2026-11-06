# Klivox Wired — conectividad, temperatura y recepción IR

Firmware C para ESP32 clásico, ESP-IDF 5.5.6 y FreeRTOS. La refactorización
conserva el Provisioning Manager oficial y no incorpora un cliente MQTT.
Incluye adquisición periódica de temperatura con un DS18B20 externo.
También captura y decodifica controles IR mediante un VS1838B, como el proyecto
`on_off_light`, con detección general y salida RAW para señales desconocidas.
Un buzzer activo emite un pulso corto al capturar una trama IR no truncada.

## Archivos

- `main/main.c`: inicialización general; inicia conectividad y retorna sin esperar IP.
  También inicia el módulo de temperatura, incluso si falla la conectividad.
- `main/app_feedback.c`: conecta las capturas IR con el aviso sonoro usando las
  APIs públicas, sin acoplar el driver IR al buzzer.
- `main/CMakeLists.txt`: compila `main.c` y `app_feedback.c`, y declara sus
  dependencias de `wifi_manager`, `temperature_manager`, `ir_receiver` y `buzzer`.
- `main/idf_component.yml`: declara la versión de ESP-IDF del proyecto
  (`>=5.5.6,<5.6`). `dependencies.lock` fija las dependencias resueltas.
- `components/wifi_manager/wifi_manager.c`: NVS, netif, event loop, Wi-Fi Station,
  aprovisionamiento BLE, eventos y temporizador de reconexión.
- `components/wifi_manager/include/wifi_manager.h`: API y eventos públicos.
- `components/wifi_manager/Kconfig`: selección de transporte y seguridad,
  reintentos del alta inicial, QR y tiempos de reconexión.
- `components/wifi_manager/idf_component.yml`: dependencia QR y versión de SDK;
  su CMake declara las dependencias públicas y privadas del componente.
- `sdkconfig`: conserva la seguridad y partición actuales; incorpora los dos
  parámetros de reconexión y la configuración del sensor al regenerarse en el build.
- `components/temperature_manager/`: componente reutilizable que encapsula el
  bus 1-Wire, descubrimiento DS18B20, lectura periódica, log y consulta del último
  resultado. Incluye su propia configuración y dependencias.
- `components/ir_receiver/`: receptor IR con API C, implementación C++, callback
  y consulta de la última captura. Usa `jorgecis/irremoteidf` 1.0.2.
- `components/buzzer/`: buzzer activo mediante GPIO y temporizador de apagado,
  con solicitudes de pulso asíncronas y sin dependencias externas.
- `sdkconfig.defaults`: parámetros base del ESP32 y ajuste de IRAM para RMT;
  los valores de un `sdkconfig` existente tienen prioridad sobre estos defaults.

La lógica original de `main/app_main.c` se separó de la inicialización general
y ahora reside en `components/wifi_manager`. La cabecera conserva su nombre,
por lo que los consumidores siguen incluyendo `wifi_manager.h`.
`main/main_backup.c`, que ya existía, queda intacto y fuera de la compilación.
La copia original anterior a esta refactorización está en
`.refactor-backup/wifi-manager-original/`, incluyendo configuración y particiones.

## Uso y eventos para futuros componentes

Wi-Fi/BLE ahora es un componente independiente, igual que los sensores. Para
reutilizarlo, copiar la carpeta `components/wifi_manager` completa y agregar
`wifi_manager` a `PRIV_REQUIRES` del consumidor. El traslado conserva la API,
la configuración de seguridad y los datos persistentes. Su menú está en
`Component config → Klivox Wi-Fi manager configuration`.
Ver [documentación del componente](components/wifi_manager/README.md).

```c
#include "wifi_manager.h"

static void connectivity_event(void *ctx, esp_event_base_t base,
                               int32_t id, void *data)
{
    switch (id) {
    case WIFI_MANAGER_EVENT_CONNECTED: {
        const wifi_manager_connected_event_t *event = data;
        (void)event->ip;
        /* Notificar a la tarea del futuro cliente MQTT: IPv4 disponible. */
        break;
    }
    case WIFI_MANAGER_EVENT_DISCONNECTED:
        /* Notificar pérdida de conectividad; no borrar configuración. */
        break;
    default:
        break;
    }
}

/* Desde la tarea de inicialización, comprobando cada resultado: */
/* wifi_manager_init();
 * esp_event_handler_register(WIFI_MANAGER_EVENT, ESP_EVENT_ANY_ID,
 *                            connectivity_event, NULL);
 * wifi_manager_start();
 */
```

Registrar los consumidores después de `wifi_manager_init()` y antes de
`wifi_manager_start()`. Los callbacks se ejecutan en el event loop predeterminado:
deben ser cortos y delegar operaciones de red a sus respectivas tareas.

| Evento de `WIFI_MANAGER_EVENT` | Datos |
| --- | --- |
| `WIFI_MANAGER_EVENT_CONNECTED` | `wifi_manager_connected_event_t`: dirección IPv4 |
| `WIFI_MANAGER_EVENT_DISCONNECTED` | `wifi_manager_disconnected_event_t`: razón Wi-Fi; 0 si perdió IP o se detuvo Station |
| `WIFI_MANAGER_EVENT_PROVISIONING_STARTED` | Sin datos |
| `WIFI_MANAGER_EVENT_PROVISIONING_SUCCESS` | Sin datos; credenciales validadas con IP |
| `WIFI_MANAGER_EVENT_PROVISIONING_FAILED` | `wifi_manager_provisioning_failed_event_t`: motivo y error |

`wifi_manager_is_connected()` informa si hay IPv4, no solamente asociación al AP.
`wifi_manager_is_provisioned()` indica que se encontraron credenciales guardadas
al arrancar o que el aprovisionamiento inicial ya fue validado. No garantiza que
el router esté disponible. Ambas consultas son instantáneas y usan un event group
de FreeRTOS. Los datos de cada evento solo son válidos dentro del callback.

Los avisos públicos no bloquean si la cola está llena: se registra el descarte y
los consumidores pueden reconciliar su estado con las consultas públicas. Un
evento interno de reconexión que no cabe en la cola se reintenta un segundo
después. Una renovación de IP puede volver a emitir `CONNECTED`; los futuros
consumidores deben tolerarlo.

## Conexión, persistencia y recursos

1. `init` inicializa NVS y el controlador, y consulta el estado mediante
   `wifi_prov_mgr_is_provisioned()`; no borra NVS ni modifica las credenciales.
2. Con credenciales guardadas, `start` libera el manager y sus recursos BTDM e
   inicia Station sin anunciar un servicio BLE de aprovisionamiento.
3. Sin credenciales, utiliza el flujo original: nombre `PROV_XXXXXX`, UUID BLE,
   endpoint `custom-data`, Security 2, salt/verifier y QR del ejemplo.
4. El manager oficial controla los intentos de conexión durante la validación
   inicial. Se mantienen cinco intentos y la opción existente que permite al
   teléfono enviar credenciales corregidas después de un fallo. Ese reinicio
   descarta exclusivamente las credenciales nuevas rechazadas, no las de una
   instalación que ya fue validada. No se ejecuta ante desconexiones normales.
5. Después de `WIFI_PROV_END`, se desinicializa el manager y se libera BTDM.
   El temporizador del módulo toma el control de las reconexiones: 1, 2, 4, 8,
   16, 32 y 60 segundos, manteniendo 60 como máximo. Los tiempos son esperas
   entre intentos, adicionales al tiempo que tarde cada intento del controlador.
   Obtener IP reinicia la espera a un segundo; asociarse cancela el temporizador
   mientras DHCP obtiene una dirección.
6. Si se pierde conexión después de validar credenciales pero antes de que BLE
   termine, nunca se borran esas credenciales. Si el manager reporta un fallo en
   esa ventana, se solicita su cierre y la reconexión continúa después de `END`.

Los tiempos se ajustan con `CONFIG_KLIVOX_WIFI_RETRY_INITIAL_MS` y
`CONFIG_KLIVOX_WIFI_RETRY_MAX_MS` en `idf.py menuconfig`.

No hay espera de conexión en `app_main`, bucles de polling, reinicio automático
del ESP32 ni recuperación destructiva de NVS. Si NVS no puede inicializarse,
el módulo devuelve el error y deja los datos intactos. Los recursos parcialmente
adquiridos durante una inicialización fallida se liberan; los recursos globales
compartidos NVS/netif no se desinicializan.

`init` y `start` deben llamarse desde una sola tarea de arranque. Las llamadas
repetidas devuelven su resultado original. Un error de arranque es terminal para
esa ejecución, porque la limpieza del SDK puede haber liberado irreversiblemente
memoria BLE. No se ofrece todavía una API de stop/reset/reaprovisionamiento;
esa función futura requiere diseñar explícitamente el ciclo de vida Bluetooth.

## Seguridad de desarrollo y producción

La configuración activa sigue siendo BLE + Security 2 + Development. Se mantienen
sin cambios el usuario `wifiprov`, PoP `abcd1234`, salt, verifier y UUID originales.
No se registra la contraseña de la red Wi-Fi ni el contenido arbitrario recibido
por `custom-data`. El QR y su URL continúan mostrando el PoP de desarrollo como
antes; ese PoP es distinto de la contraseña del router.

La rama `EXAMPLE_PROV_SEC2_PROD_MODE` continúa separada y sin implementar:
devuelve `ESP_ERR_NOT_SUPPORTED` hasta incorporar material SRP único por dispositivo
desde fábrica. No cambia silenciosamente a las credenciales de desarrollo.

## Compilar y validar en hardware

Desde una terminal ESP-IDF 5.5.6, en esta carpeta:

```powershell
idf.py build
```

No se necesita modificar el SDK. La tabla debe seguir siendo
`Single factory app (large), no OTA` (`partitions_singleapp_large.csv`).
La compilación no verifica por sí misma el funcionamiento de radio o NVS en el ESP32.

Validación inicial de la refactorización Wi-Fi, el 5 de noviembre de 2026:
`idf.py build` terminó con código
0 en ESP-IDF 5.5.6. El binario ocupa 1.308.736 bytes y deja 15 % libre en la
partición actual. Se compararon los bytes de salt, verifier, UUID y tabla de
particiones con la copia original; coinciden. En `sdkconfig` solo se agregaron
los dos parámetros de backoff. El log de la comprobación final está en
`build/wifi_manager_build_validation.log`. No se ejecutó flash ni borrado;
las pruebas de la tabla siguiente quedan pendientes en hardware.

La carga del firmware o cualquier borrado del dispositivo requieren autorización
previa. Una vez autorizada la carga y con el monitor serie disponible:

| Prueba | Procedimiento | Resultado esperado |
| --- | --- | --- |
| Persistencia existente | Usar el ESP32 que ya tiene la red configurada | `Saved credentials found`, sin iniciar servicio BLE, seguido de `Connected with IP address` |
| Corte de energía | Desconectar alimentación y volver a conectar | Reconexión a la misma red sin usar la app y sin borrar credenciales |
| Router ausente | Apagar temporalmente el AP y encenderlo después | Esperas progresivas en logs, reconexión al volver el AP, sin reiniciar ni abrir BLE |
| Reinicio sin router | Arrancar con el AP apagado; restaurarlo después | Mantiene estado provisionado y reintenta hasta recuperar IP |
| Aprovisionamiento inicial | Usar una unidad sin credenciales; conectar con ESP BLE Provisioning y el QR existente | Sesión segura, recepción de credenciales sin mostrarlas, éxito, IP y liberación BTDM tras finalizar |
| Credenciales iniciales incorrectas | En esa unidad, enviar primero una contraseña incorrecta; luego corregirla desde la app | Evento de fallo inicial y posibilidad de corregir sin borrar toda la memoria |
| Desconexión durante cierre BLE | Tras el primer éxito, apagar el AP antes del cierre automático; esperar el cierre y restaurar el AP | Conserva credenciales, libera aprovisionamiento y recupera IP; comprobar también tras cortar alimentación |
| Estado público | Consumir eventos desde otro módulo y consultar ambas funciones | Connected solo con IP; disconnected quita ese estado pero conserva provisioned |
| Privacidad de logs | Revisar la sesión de aprovisionamiento y reconexión | La contraseña real de la red y los datos de `custom-data` no aparecen |

Para probar el alta inicial, usar una segunda unidad sin configurar evita borrar
las credenciales de la unidad actual. No se agrega un borrado automático para
facilitar esta prueba.

## DS18B20: conexión y lectura periódica

Conectar UN sensor con alimentación externa de tres hilos, con el equipo apagado:

| DS18B20 | ESP32 Klivox Wired |
| --- | --- |
| GND (pin 1 en TO-92) | GND |
| DQ / DATA (pin 2 en TO-92) | GPIO18 / IO18 |
| VDD (pin 3 en TO-92) | 3V3 / 3,3 V |
| Resistencia externa de 4,7 kΩ | Entre DQ y 3V3 |

```text
ESP32 3V3 ----+---------------- VDD DS18B20
              |
            [4,7 kΩ]
              |
ESP32 GPIO18 -+---------------- DQ DS18B20
ESP32 GND -------------------- GND DS18B20
```

La resistencia es un pull-up entre datos y alimentación, no va en serie con DQ.
Si el módulo ya incorpora ese pull-up a 3,3 V, no hace falta duplicarlo. No
conectar DQ ni el pull-up a 5 V. No se implementa alimentación parásita.
En sondas encapsuladas con cable, confirmar las funciones de los conductores
con el fabricante: los colores no están garantizados.

GPIO18 no se utiliza en el código de conectividad actual y es un GPIO de
entrada/salida del ESP32. Confirmar que esté disponible en la placa concreta;
los números GPIO no son la posición física del pin en el conector.
Referencia eléctrica: [datasheet del DS18B20](https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf).

En `idf.py menuconfig` → **Klivox temperature sensor**:

- `CONFIG_KLIVOX_TEMPERATURE_GPIO`: GPIO de datos; predeterminado **18**.
- `CONFIG_KLIVOX_TEMPERATURE_PERIOD_SECONDS`: período de muestreo; predeterminado
  **5 segundos**, configurable entre 1 y 3600 segundos.

La primera conversión comienza en la tarea del sensor al arrancar. Una salida
esperada, ilustrativa y todavía no medida en el hardware, es:

```text
I (...) temperature_manager: 1-Wire ready on GPIO18; sampling period 5000 ms
I (...) temperature_manager: DS18B20 detected on GPIO18, ROM=...
I (...) temperature_manager: DS18B20: 26.50 C
I (...) temperature_manager: DS18B20: 26.56 C
```

### Configuración de memoria en el ESP32 clásico

Para que Wi-Fi, BLE, DS18B20 y recepción IR quepan en la IRAM del ESP32 clásico,
se colocan en flash el ISR TX y los encoders de RMT:
`CONFIG_RMT_TX_ISR_HANDLER_IN_IRAM=n` y `CONFIG_RMT_ENCODER_FUNC_IN_IRAM=n`.
El ISR RX permanece en IRAM. Se mantienen desactivados los modos RMT TX/RX
cache-safe de la configuración previa. No se cambió la configuración Wi-Fi/BLE,
la tabla de particiones ni el SDK. El margen de IRAM es distinto del espacio
libre en flash y debe revisarse al ampliar el firmware. No se ha validado
todavía la integración de estos componentes en otro SoC.
La compilación con IR deja **396 bytes libres en IRAM**; la partición de
aplicación conserva **55.248 bytes libres (aproximadamente 4 %)** con el buzzer.

Este ajuste se aplica al `sdkconfig` local y se incluye en `sdkconfig.defaults`
para nuevas configuraciones. Si se conserva un `sdkconfig` anterior, desactivar
**Component config → ESP-Driver:RMT Configurations → Place RMT TX ISR handler
in IRAM to reduce latency** y **Place RMT encoder function in IRAM** desde
`idf.py menuconfig`, porque los defaults no sobrescriben valores ya existentes.
El ajuste pertenece al proyecto Wired;
revisar los recursos de la placa Zigbee antes de aplicar su configuración.

Como los modos cache-safe están desactivados, las interrupciones RMT pueden
posponerse durante escrituras de flash. Se debe comprobar la lectura simultánea
con aprovisionamiento y guardado de credenciales; los fallos detectados por el
controlador se invalidan y reintentan, sin usar una lectura errónea.

### Adquisición y errores

La conversión cede CPU en la tarea del sensor durante unos 800 ms; el período
normal se mide entre inicios de conversión. El controlador verifica CRC y su
patrón de arranque sin inicializar. El módulo también rechaza resultados fuera
del rango del sensor, sin publicar valores ficticios ante errores. No escribe
la EEPROM del sensor ni NVS.

Ante un sensor ausente o un fallo, se emite un aviso, se invalida el resultado
consultable y se repite el descubrimiento en el siguiente ciclo. Eso permite
recuperarse al restablecer una conexión o sustituir el sensor. La dirección ROM
identifica qué sensor produjo cada lectura. Solo se admite un DS18B20 en este
bus dedicado; si se conectan varios, se utiliza el primero descubierto.

### API y reutilización en Zigbee

`components/temperature_manager` depende únicamente de FreeRTOS, utilidades de
ESP-IDF y los controladores oficiales `espressif/ds18b20` **0.3.1** y
`espressif/onewire_bus` **1.1.1**. El controlador DS18B20 elegido no incorpora la
integración adicional de Sensor Hub. Las versiones están fijadas en el manifiesto
del componente y en `dependencies.lock`.

La API pública de `temperature_manager.h` contiene:

- `temperature_manager_init(config)`: configura el bus; copia GPIO, período y
  callback opcional. Llamar desde una única tarea de inicialización.
- `temperature_manager_start()`: inicia una única tarea de lectura y log.
- `temperature_manager_get_latest(out)`: consulta inmediata y protegida entre
  tareas. Devuelve `ESP_OK` y escribe la lectura solo si el último intento fue
  válido; antes del primer resultado devuelve `ESP_ERR_INVALID_STATE` y tras
  un fallo devuelve ese error. La lectura contiene °C, ROM y `timestamp_us`.
- `config.on_sample`: callback opcional con estado y lectura por cada intento;
  recibe `NULL` en lugar de lectura si falla. Se ejecuta en la tarea del sensor:
  copiar los datos y enviar una notificación o mensaje a la tarea consumidora.

No se necesita conexión Wi-Fi, BLE ni un event loop para obtener temperatura.
`main.c` intenta iniciar el sensor incluso si falla la inicialización de red.
El bus y la tarea viven durante toda la ejecución; no hay API de parada todavía.

Para reutilizarlo en el firmware Zigbee, copiar la carpeta completa
`components/temperature_manager`, agregar `temperature_manager` a
`PRIV_REQUIRES` del componente consumidor, elegir un GPIO libre en esa placa y
llamar a `init`/`start` con su configuración. Se necesita un SoC con RMT y canales
TX/RX disponibles; no asumir que GPIO18 es apropiado para la placa Zigbee.
El transporte Zigbee debe consumir la consulta o el callback respetando el
contexto y los bloqueos de su propia pila. No se modificó el proyecto Zigbee en
esta implementación.

### Verificación en hardware del sensor

Una vez autorizada la carga del firmware:

1. Cablear alimentación, GND, GPIO18 y el pull-up; arrancar y comprobar la ROM
   detectada y una temperatura razonable aproximadamente cada cinco segundos.
2. Calentar suavemente la sonda con la mano y comprobar el cambio en el log.
3. Interrumpir/restablecer DQ sin provocar cortocircuitos: deben aparecer errores,
   nunca temperaturas inventadas, y después recuperarse la lectura.
4. Arrancar sin sensor y conectarlo después correctamente: debe detectarlo en
   un ciclo posterior, sin reiniciar ni detener la conectividad.
5. Comprobar que Wi-Fi sigue conectando con las credenciales guardadas después
   de cortar alimentación y que la lectura continúa si el AP está ausente.
6. En una unidad sin credenciales, aprovisionar BLE mientras se adquiere
   temperatura y comprobar la recuperación de cualquier fallo transitorio.

La compilación se registra en `build/temperature_manager_build.log`; estas
pruebas físicas requieren el sensor conectado y no se sustituyen por el build.

Validación de esta incorporación: `idf.py build` finalizó con código 0 en
ESP-IDF 5.5.6, sin advertencias del compilador ni del enlazador. El binario mide
1.340.224 bytes y deja un 13 % libre en la partición de aplicación. Se comprobó
que la tabla binaria de particiones coincide con la anterior. No se ejecutó
flash ni borrado del dispositivo; las pruebas físicas anteriores están pendientes.

## Recepción IR con VS1838B

Conectar **OUT a GPIO27**, **VCC a 3V3** y **GND a GND común**, confirmando el
pinout del receptor/módulo. GPIO18 sigue reservado para el DS18B20. En
`menuconfig → Klivox IR receiver` se ajustan el GPIO, buffer de captura (4096),
silencio de fin de mensaje (50 ms) y log RAW (activado).

La implementación usa la misma biblioteca y el mismo flujo de decodificación
de `on_off_light`; se conserva la detección general sin seleccionar una marca.
No se añade transmisión IR ni automatización. Los radios y ambos sensores
continúan en tareas separadas. Los buffers IR se reservan antes de Wi-Fi/BLE;
un conflicto entre los GPIO de IR y DS18B20 se rechaza sin reconfigurar el pin.

La API C permite iniciar el módulo, consultar la última captura y recibir un
callback con metadatos y vista temporal de RAW. Detalles de memoria, límites de
la biblioteca, portabilidad y pruebas en
[components/ir_receiver/README.md](components/ir_receiver/README.md).

Validación de IR previa al buzzer: `idf.py build` finalizó con código 0 en ESP-IDF 5.5.6. El
binario ocupa **1.479.760 bytes**. No hubo advertencias del compilador/enlazador;
el comprobador de tamaños advierte que la partición está cerca de llenarse (4 %
libre). La tabla binaria de particiones sigue idéntica. Log en
`build/ir_receiver_build.log`. No se realizó flash ni borrado; quedan pendientes
las pruebas de recepción real y coexistencia con BLE/Wi-Fi y DS18B20.

## Aviso sonoro al capturar IR

`components/buzzer` controla un buzzer activo mediante transistor NPN desde
**GPIO25**, con un pulso nominal de **80 ms**. La conexión con el receptor está
en `main/app_feedback.c` y usa su callback público; el componente IR conserva
su independencia y la captura en CPU1.

Una captura con tiempos y sin `overflow` solicita un pitido, también si el
protocolo es desconocido. Si ya está sonando se omite el nuevo pedido, sin
extenderlo ni encolarlo. Un temporizador apaga la salida, sin esperar en la
tarea IR y sin generar sonidos al inicializar. Una confirmación sonora indica
recepción, no que el aire haya ejecutado el comando.

En `menuconfig → Component config → Klivox active buzzer` se cambian GPIO y
duración. La aplicación comprueba conflictos con GPIO18 (DS18B20) y GPIO27 (IR)
antes de configurar la salida. Ver el circuito con NPN, resistencia de base,
pull-down y alimentación externa en
[components/buzzer/README.md](components/buzzer/README.md).

Validación del buzzer: `idf.py build` finalizó con código 0 en ESP-IDF 5.5.6,
sin advertencias de C/enlazado. El firmware ocupa 1.480.752 bytes; quedan 55.248
bytes en la partición de aplicación (4 % aproximado) y 396 bytes de IRAM. Se
mantiene el aviso del comprobador de tamaños por el escaso margen. Se verificó
que las implementaciones de Wi-Fi, IR y DS18B20 y la tabla binaria de particiones
siguen idénticas; solo se añadieron dos valores efectivos de configuración,
GPIO25 y pulso de 80 ms. El log está en `build/buzzer_build.log`. No se ejecutó
flash ni borrado; el sonido y la coexistencia física con sensores/radios quedan
pendientes de probar con el circuito conectado.
