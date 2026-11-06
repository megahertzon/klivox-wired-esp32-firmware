# Klivox Wired — conectividad, temperatura y recepción IR

Firmware C para ESP32 clásico, ESP-IDF 5.5.6 y FreeRTOS. La refactorización
conserva el Provisioning Manager oficial y no incorpora un cliente MQTT.
Incluye adquisición periódica de temperatura con un DS18B20 externo.
También captura y decodifica controles IR mediante un VS1838B, como el proyecto
`on_off_light`, con detección general y salida RAW para señales desconocidas.
Un buzzer activo emite un pulso corto al capturar una trama IR no truncada.
Incluye dos entradas NC de puerta/ventana y un pulsador NO con eventos.
Incluye un LED RGB de ánodo común mediante `components/status_led_manager/`,
con brillo bajo, prioridades liberables y actualización mediante eventos Wi-Fi.
Ver [API, patrones, cableado y pruebas físicas](components/status_led_manager/README.md).

La integración RGB está en `main/app_status.c/.h`; `main/main.c` inicia el
componente antes de los radios y registra el consumidor después de inicializar
Wi-Fi. Los GPIO RGB son 21/22/23 y SETUP se trasladó a 33. El evento CONNECTING,
el evento PROVISIONING_ENDED y la instantánea de aprovisionamiento se exponen
en la API pública Wi-Fi; el gestor LED no depende de ella directamente.

Validación RGB: `idf.py build` con ESP-IDF 5.5.6 completó el enlace y generó
`build/klivox-wired.bin` de 1.488.576 bytes, con 47.424 bytes libres en la
partición de aplicación (3 %). La IRAM queda con solo 32 bytes libres según
el mapa; cualquier ampliación requiere revisar ese presupuesto. El componente
incluye una regla de enlace para los caminos LEDC usados solo desde tarea,
sin editar el SDK. Registro: `build/log/idf_py_stdout_output_18040`.
No se realizó flash ni borrado; las pruebas físicas siguen pendientes.

## Archivos

- `main/main.c`: inicialización general; inicia conectividad y retorna sin esperar IP.
  También inicia el módulo de temperatura, incluso si falla la conectividad.
- `main/app_feedback.c`: conecta las capturas IR con el aviso sonoro usando las
  APIs públicas, sin acoplar el driver IR al buzzer.
- `main/app_inputs.c`: asigna GPIO y nombres a los contactos/pulsadores y registra
  sus cambios; `app_inputs.h` define los índices de la aplicación.
- `main/CMakeLists.txt`: compila `main.c`, `app_feedback.c` y `app_inputs.c`, con
  dependencias de conectividad, sensores, buzzer y `digital_inputs`.
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
- `components/digital_inputs/`: entradas con pull-up, antirrebote independiente,
  eventos ESP-IDF, callback y consulta del último estado estable.
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
| `WIFI_MANAGER_EVENT_CONNECTING` | Sin datos; comienza un intento Station |
| `WIFI_MANAGER_EVENT_PROVISIONING_ENDED` | Sin datos; recursos de aprovisionamiento liberados |

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
2. Con credenciales guardadas, `start` libera el manager e
   inicia Station sin anunciar un servicio BLE de aprovisionamiento.
3. Sin credenciales, utiliza el flujo original: nombre `PROV_XXXXXX`, UUID BLE,
   endpoint `custom-data`, Security 2 y salt/verifier del ejemplo.
4. El manager oficial controla los intentos de conexión durante la validación
   inicial. Se mantienen cinco intentos y la opción existente que permite al
   teléfono enviar credenciales corregidas después de un fallo. Ese reinicio
   descarta exclusivamente las credenciales nuevas rechazadas, no las de una
   instalación que ya fue validada. No se ejecuta ante desconexiones normales.
5. Después de `WIFI_PROV_END`, se desinicializan el manager y el transporte BLE.
   La memoria Bluetooth queda reutilizable para SETUP; no se libera irreversiblemente.
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
repetidas devuelven su resultado original. El trabajo del SDK se serializa en una
sola tarea `wifi_control`; `start` espera su resultado, sin esperar credenciales ni IP.
La API `wifi_manager_reset_credentials()` permite recuperar el aprovisionamiento
incluso si falló el arranque del servicio, después de una inicialización correcta.

## Seguridad de desarrollo y producción

La configuración activa sigue siendo BLE + Security 2 + Development. Se mantienen
sin cambios el usuario `wifiprov`, PoP `abcd1234`, salt, verifier y UUID originales.
No se registra la contraseña de la red Wi-Fi ni el contenido arbitrario recibido
por `custom-data`. El QR y su URL ya no se imprimen porque contienen el secreto
de aprovisionamiento, independientemente de `EXAMPLE_PROV_SHOW_QR`. La app puede
usar el QR de desarrollo previamente entregado o la entrada manual con los mismos
parámetros Security 2; no se cambian las credenciales de seguridad.

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
| Aprovisionamiento inicial | Usar una unidad sin credenciales; conectar con ESP BLE Provisioning y el QR existente o entrada manual | Sesión segura, recepción de credenciales sin mostrarlas, éxito, IP y cierre del transporte BLE tras finalizar |
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
I (...) KLIVOX_LED: State at temperature sample: WIFI_CONNECTED (sensor=ESP_OK)
I (...) temperature_manager: DS18B20: 26.56 C
```

Después de cada intento de lectura, `main/app_status.c` registra una instantánea
del estado efectivo del LED con el tag `KLIVOX_LED`, incluso si falla el sensor
(el campo `sensor` muestra el error). Usa el callback `on_sample`, sin acoplar
el componente de temperatura al LED ni modificar el patrón. Si el LED no pudo
inicializarse, muestra `NOT_INITIALIZED`.

### Configuración de memoria en el ESP32 clásico

Para que Wi-Fi, BLE, DS18B20 y recepción IR quepan en la IRAM del ESP32 clásico,
se colocan en flash el ISR TX y los encoders de RMT:
`CONFIG_RMT_TX_ISR_HANDLER_IN_IRAM=n` y `CONFIG_RMT_ENCODER_FUNC_IN_IRAM=n`.
El ISR RX permanece en IRAM. Se mantienen desactivados los modos RMT TX/RX
cache-safe de la configuración previa. No se cambió la configuración Wi-Fi/BLE,
la tabla de particiones ni el SDK. El margen de IRAM es distinto del espacio
libre en flash y debe revisarse al ampliar el firmware. No se ha validado
todavía la integración de estos componentes en otro SoC.
La compilación con IR, buzzer y entradas deja **396 bytes libres en IRAM**;
la partición de aplicación conserva **52.000 bytes libres (3 % según el
comprobador de tamaños)**.

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
duración. La aplicación comprueba conflictos con sensores y pulsadores
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

## Contactos NC y pulsadores NO

Cada contacto/pulsador se conecta entre su GPIO y **GND común**, con una
resistencia individual de **10 kΩ entre GPIO y 3V3**. Son entradas para contactos
secos. Confirmar que el sensor NC cierre el circuito con la puerta cerrada.

| Entrada | GPIO | Activo | Reposo |
| --- | --- | --- | --- |
| Contacto NC 1 | 32 | HIGH, abierto | LOW, cerrado |
| Contacto NC 2 | 26 | HIGH, abierto | LOW, cerrado |
| Pulsador NO encendido/apagado | 19 | LOW, pulsado | HIGH, liberado |

Un cable cortado o un NC sin conectar también se lee abierto. Los GPIO se
configuran en **Klivox digital inputs**; GPIO18, GPIO27 y GPIO25 conservan sus
funciones. Las tres entradas tienen antirrebote independiente de 50 ms.

`main/app_inputs.c` configura `digital_inputs` y registra apertura/cierre,
pulsación/liberación y la acción solicitada. Solo se preparan entradas y eventos:
no hay transmisión IR ni cambio del estado del aire. No hay repetición al
mantener un botón; un botón pulsado al arrancar publica estado inicial sin
solicitar una acción.

La API pública `digital_inputs.h` permite suscribirse a
`DIGITAL_INPUTS_EVENT_CHANGED` en `DIGITAL_INPUTS_EVENT`, consultar una instantánea
o recibir un callback en la tarea de entradas. Cada estado lleva índice, GPIO,
nivel, indicador activo, indicador inicial, contador y timestamp. Los eventos
iniciales no deben interpretarse como órdenes. Las publicaciones no esperan si
la cola está llena; se registra el descarte y la consulta permite reconciliar
el estado. La tarea y el servicio GPIO se instalan en CPU1, como IR.

Ver [API, cableado y pruebas físicas](components/digital_inputs/README.md).
La integración en Zigbee queda preparada mediante el componente independiente;
se deberá elegir el mapa de pines de esa placa.

GPIO21 y GPIO22 ahora pertenecen al LED RGB; se retiraron los pulsadores de
subir y bajar temperatura de la configuración y de los eventos de aplicación.

Validación de esta tarjeta: `idf.py build` finalizó con código 0 en ESP-IDF
5.5.6, sin advertencias de compilador/enlazador. Firmware de **1.484.000 bytes**,
con 52.000 bytes libres en la partición (3 % según el comprobador de tamaños)
y 396 bytes de IRAM. Se mantiene el aviso por el margen de flash. Se verificaron
idénticos los drivers de Wi-Fi, IR, temperatura y buzzer y la tabla binaria de
particiones; la configuración efectiva solo añade los cinco parámetros de las
tres entradas respecto de la versión anterior sin contactos/pulsadores.
Log: `build/digital_inputs_build.log`. No se realizó flash ni borrado;
quedan pendientes las pruebas físicas con GPIO32, GPIO26 y GPIO19.


## Botón oculto SETUP: restablecimiento Wi-Fi

Cablear un pulsador **normalmente abierto entre GPIO33 y GND**. Usa pull-up
interno y nivel activo LOW; no requiere resistencia externa. GPIO33 está libre
en la configuración actual (temperatura 18, IR 27, buzzer 25, contactos 32/26,
POWER 19 y RGB 21/22/23). Mover el cable de SETUP de GPIO23 a GPIO33.
Si se cambia ese mapa, la aplicación valida conflictos antes de iniciar el LED.

`components/setup_button_manager/` configura el GPIO y usa interrupciones para
despertar una tarea de prioridad 1 en CPU1, compartiendo el servicio GPIO con
IR y entradas. La ISR solo captura nivel/tiempo y notifica; la tarea duerme hasta
un flanco o vencimiento del antirrebote/umbral. No hay polling permanente.
Las constantes `KLIVOX_SETUP_GPIO`, `KLIVOX_SETUP_HOLD_MS` (5000) y
`KLIVOX_SETUP_DEBOUNCE_MS` (50) están en la cabecera. La medición empieza en el
último flanco LOW y exige un nivel continuo: cualquier rebote reinicia ese plazo.
Se dispara una vez por pulsación y solo se rearma tras 50 ms de liberación estable.
También funciona si se arranca con SETUP presionado.

`main.c` llama a `app_setup_init()`. La composición está en `main/app_setup.c`:
valida el mapa de la placa y registra un callback que programa el sonido y solicita
el reset Wi-Fi. El componente `setup_button_manager` recibe GPIO, tiempos y
callback en `setup_button_manager_config_t`; no depende de Wi-Fi, NVS ni buzzer.
Su API es `setup_button_manager_init(&config)`, y el callback debe regresar rápido.

Al confirmar los 5 segundos se programan **dos pitidos de 100 ms, con una pausa
de 100 ms**, en el buzzer existente de GPIO25. `app_feedback` da prioridad a SETUP:
si hay un pitido IR en curso, espera su final mediante reintentos con temporizador.
`buzzer_beep_pattern(2, 100, 100)` reserva también la pausa entre ambos tonos para
que no se mezclen otros pitidos IR. No hay `vTaskDelay` ni esperas del sonido en el
callback SETUP; el restablecimiento Wi-Fi/BLE avanza en paralelo. El aviso confirma
la pulsación prolongada, no el éxito final del aprovisionamiento. Si falla el buzzer,
el reset sigue funcionando y se registra el error.

`wifi_manager_reset_credentials()` es asíncrona: `ESP_OK` significa solicitud
aceptada. Admite llamadas desde tareas y callbacks de eventos, nunca desde ISR.
Rechaza solicitudes simultáneas con `ESP_ERR_INVALID_STATE` y devuelve errores
de cola. Observar `PROVISIONING_STARTED` o `PROVISIONING_FAILED` para el resultado;
las consultas de estado permiten reconciliar notificaciones públicas perdidas.

La solicitud desactiva el backoff, cierra el manager anterior en `wifi_control`,
detiene Station y drena los eventos antiguos antes de abrir otro servicio. Se
selecciona `WIFI_STORAGE_FLASH` y se guarda una configuración Station vacía con
`esp_wifi_set_config(WIFI_IF_STA, ...)`. Solo entonces se marca sin provisionar.
No se usa `wifi_prov_mgr_reset_provisioning()`: en ESP-IDF 5.5.6 llama a
`esp_wifi_restore()` y restablece más opciones Wi-Fi de las necesarias. No se
borran namespaces del producto ni la partición NVS. No se duplican netif, event
loop, controlador Wi-Fi ni handlers de aplicación. El nuevo manager conserva
nombre BLE, UUID, endpoint y Security 2; el SDK persiste las nuevas credenciales.

Para permitir ciclos repetidos se usa `WIFI_PROV_EVENT_HANDLER_NONE` en lugar
de `FREE_BTDM`. El servicio y la pila BLE se detienen/desinicializan al terminar;
se conserva la memoria que debe seguir disponible para reiniciarlos. Esto cambia
el uso de memoria respecto del firmware que liberaba BTDM permanentemente y
requiere comprobar el mínimo heap libre en ciclos reales junto con IR y sensores.

Un corte **antes de confirmar la escritura vacía** puede conservar la red anterior;
un corte **después** arranca sin SSID y vuelve a BLE. Si falla el reinicio de BLE,
se informa `PROVISIONING_FAILED` y se puede liberar/presionar SETUP nuevamente.
No se reinicia automáticamente el ESP32.

### Prueba física de aceptación (pendiente)

Después de cargar este firmware mediante un procedimiento autorizado:

1. Arrancar con la red A guardada y comprobar IP sin anuncio BLE.
2. Mantener SETUP al menos 5 segundos. Ver `KLIVOX_SETUP: SETUP button pressed`,
   `Long press detected (5000 ms)`, `Requesting Wi-Fi credentials reset`, y luego
   `KLIVOX_WIFI: Wi-Fi credentials cleared`, `Starting BLE provisioning` y
   `Waiting for Wi-Fi configuration`. La IP anterior debe dejar de estar disponible.
3. Confirmar dos pitidos cortos al alcanzar el umbral; mantener hasta 15 segundos:
   una sola pareja y una sola solicitud. Liberar y volver a mantener 5 segundos:
   una nueva pareja. Una pulsación corta no debe sonar ni borrar nada.
   Repetir mientras llegan tramas IR: los avisos IR no deben colarse entre los dos
   tonos SETUP. Con buzzer ausente o averiado, verificar que Wi-Fi/BLE siguen.
4. Desde ESP BLE Provisioning, conectar al mismo nombre `PROV_XXXXXX`, usar los
   parámetros Security 2 Development existentes y configurar la red B.
   Comprobar éxito, nueva IP y cierre del servicio BLE sin reinicio.
5. Cortar alimentación y volver a encender: debe reconectar a B, nunca a A,
   sin anunciar BLE. Apagar/restaurar el AP y comprobar el backoff habitual.
6. Repetir el ciclo A/B al menos 20 veces; observar mínimo heap libre, ausencia
   de watchdog, anuncios duplicados y errores de inicialización Bluetooth.
   Verificar que continúan capturas IR, temperatura, contactos y POWER.

| Caso adicional | Resultado esperado |
| --- | --- |
| Pulsación menor de 5 s o rebotes breves | Sin borrado ni cambio de servicio |
| Soltar cerca del umbral antes de 5 s | Sin evento; el plazo se reinicia al volver a pulsar |
| Pulsar mientras falta el AP / durante backoff | Se cancela la reconexión y vuelve BLE |
| Pulsar durante BLE, con/sin teléfono conectado o validando credenciales | Se cierra la sesión anterior y se abre una sola sesión nueva |
| Pulsar tras un fallo de contraseña nueva | Es posible volver a configurar otra red |
| Cortar energía tras `Wi-Fi credentials cleared` antes de enviar otra red | Al volver, BLE activo; no recupera la red anterior |
| Configuración del producto en otro namespace NVS | Valores iguales antes y después de varios ciclos SETUP |
| Logs completos de sesiones | Sin contraseña Wi-Fi, PoP, verifier ni URL/QR con secretos |

Compilar no confirma estas pruebas de radio, rebotes físicos ni persistencia ante
cortes; requieren el ESP32 y el celular. No se ejecuta flash ni borrado automático.

Validación de SETUP, 6 de noviembre de 2026: `idf.py build` terminó con código 0
con ESP-IDF 5.5.6. Binario de 1.476.720 bytes; 59.280 bytes (4 %) libres en la
partición de aplicación. Registro: `build/setup_button_build.log`. Se verificaron
sin cambios salt, verifier, credenciales Security 2 y UUID, además de `sdkconfig`
y las particiones. No se ejecutó flash ni borrado. Pruebas físicas pendientes.

La implementación SETUP se trasladó a `components/setup_button_manager/`.
La coordinación queda en `main/app_setup.c` y el sonido en `app_feedback` +
`components/buzzer`; `main.c` solo inicia los módulos. La validación de sonido
físico y coexistencia con IR/BLE está pendiente; no se realizó flash ni borrado.

Compilación del traslado a componente y los dos pitidos: `idf.py build` con
ESP-IDF 5.5.6 terminó con código 0. Registro: `build/setup_button_build.log`.
Se mantiene el aviso de partición casi llena (4 % libre). La compilación no
comprueba el sonido físico, los tiempos bajo carga ni el funcionamiento BLE.
