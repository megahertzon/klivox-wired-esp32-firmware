# Receptor IR de aire acondicionado

Componente ESP-IDF con API C y decodificador interno C++. Reutiliza
`jorgecis/irremoteidf` **1.0.2**, la misma versión utilizada por
`on_off_light/main/ir/ir_ac_decoder.cpp`. No depende de Wi-Fi, BLE ni Zigbee.
El proyecto de referencia no se modifica.

## Conexión del VS1838B

Con el equipo apagado:

| Receptor | ESP32 Klivox Wired |
| --- | --- |
| OUT / señal | GPIO27 / IO27 |
| VCC | 3V3 |
| GND | GND común |

GPIO18 sigue dedicado al DS18B20. Confirmar el pinout del receptor o módulo por
sus marcas y su ficha: el orden físico puede variar entre módulos. El receptor
es de 38 kHz, con salida activa en bajo. Alimentarlo a 3,3 V para que OUT no
aplique 5 V al ESP32. No lleva el pull-up de 4,7 kΩ del DS18B20.
Puede añadirse desacoplo de 100 nF entre VCC y GND, cerca del receptor.
Ficha: [VS1838B distribuida por Elecrow](https://www.elecrow.com/download/Infrared%20receiver%20vs1838b.pdf).

El VS1838B ya entrega la envolvente demodulada. La recepción usa interrupciones
GPIO y un temporizador `esp_timer`, no canales RMT ni un ADC. El DS18B20 conserva
su bus RMT. Solo se admite un receptor: IRremoteIDF comparte su estado de captura
de forma global dentro del proceso.
En el ESP32 de dos núcleos se fija la tarea de captura al núcleo 1; en SoC de
un solo núcleo se utiliza el 0. La tarea instala la interrupción GPIO mediante
`enableIRIn()`, por lo que su afinidad evita instalarla en el núcleo 0, ocupado
por Wi-Fi/Bluetooth en Wired. Si otra aplicación ya instaló el servicio GPIO,
su afinidad previa se conserva: debe coordinar ese servicio al integrar el módulo.
Se habilita pull-up interno en GPIO27 para estabilizar la entrada si falta el
receptor. En GPIO de solo entrada del ESP32 se deja deshabilitado; confirmar el
sesgo eléctrico externo si se elige uno de esos pines.

## Configuración

En `idf.py menuconfig` → **Klivox IR receiver**:

- `KLIVOX_IR_RX_GPIO`: **27** por defecto; elegir un pin libre en otra placa.
- `KLIVOX_IR_RX_BUFFER_SIZE`: **4096** entradas por buffer, igual que la referencia.
- `KLIVOX_IR_RX_TIMEOUT_MS`: **50 ms** de silencio para cerrar el mensaje.
- `KLIVOX_IR_DUMP_RAW`: activado por defecto; desactivar para reducir el log.

En **Component config → IRremote (infrared send/receive)** se conserva habilitada
la detección general de protocolos. No se instancia ni se utiliza un transmisor.
No hay cambios automáticos de potencia, temperatura o modo del aire.

La biblioteca mantiene dos buffers de captura: aproximadamente **16 KiB** en
total con 4096 entradas. La tarea usa **8 KiB** de stack. El módulo comprueba un
presupuesto conservador de heap y bloque contiguo antes de construir el receptor.
`main.c` reserva esos buffers antes de iniciar los radios y comienza la captura
después de inicializar conectividad y temperatura.

La comprobación reduce el riesgo de agotar memoria, pero no garantiza todas las
asignaciones posteriores. La biblioteca original reinicia ante ciertos fallos
de asignación en sus constructores y usa comprobaciones fatales en parte del
arranque GPIO/timer. No se modifican sus fuentes. Su `enableIRIn()` no devuelve
estado: `ir_receiver_start()` confirma la creación de la tarea, y los fallos del
arranque interno se reportan por el log de la biblioteca. Esto debe comprobarse
en hardware, especialmente durante el intercambio Security 2 de BLE.

## Capturas y log

Como en `on_off_light`, se usa `save_buffer=true` para decodificar una instantánea
mientras continúa la captura. La tarea consulta `decode()` cada 10 ms, cediendo
CPU con `vTaskDelay`, porque esa es la API de recepción de IRremoteIDF. No espera
flancos activamente ni realiza el procesamiento en `app_main` o en la ISR.

Una salida ilustrativa, no una lectura medida, sería:

```text
I (...) ir_receiver: OUT -> GPIO27, capture core=1. Esperando el control remoto...
I (...) ir_receiver: CAPTURA #1: GREE, 64 bits, ... tiempos, repeat=0, overflow=0
I (...) ir_receiver: Protocol  : GREE
I (...) ir_receiver: Aire acondicionado: ...
I (...) ir_receiver: RAW_US #1 [0]: 9000,4500,560,...
```

Se imprime el resumen de la biblioteca, la descripción del aire cuando existe,
los bytes de estados AC y opcionalmente los tiempos RAW. Estos se expresan en
microsegundos, sin el hueco inicial sintético: índice par = marca activa en bajo;
índice impar = espacio. Se transmiten en líneas de hasta 16 duraciones, con
número de captura e índice, sin crear un texto RAW enorme en el heap.

`UNKNOWN` significa que no se reconoció el protocolo; su hash no se expone como
si fuera un comando real. Una captura con `overflow` queda marcada como truncada
y no se publica como protocolo/estado conocido para automatización. Aumentar el
buffer solo si el heap lo permite; ajustar el silencio si un comando se divide.

El doble buffer no evita todas las pérdidas: el log RAW largo o un callback
bloqueante pueden hacer perder mensajes posteriores. Pulsar un botón a la vez
durante el diagnóstico. No se guarda ninguna captura automáticamente en NVS.

## API pública

`include/ir_receiver.h` no expone tipos C++ ni tipos de la biblioteca:

- `ir_receiver_init(config)`: copia configuración y reserva el receptor.
  Llamar una sola vez desde la tarea de arranque, antes de Wi-Fi/BLE.
- `ir_receiver_start()`: crea la tarea una vez; repetir un inicio exitoso es seguro.
- `ir_receiver_get_latest(out)`: copia la última captura sin esperar. Devuelve
  `ESP_ERR_INVALID_STATE` si aún no existe una captura y `ESP_ERR_TIMEOUT` si
  el mutex está ocupado. Solo escribe la salida en `ESP_OK`.
- `config.on_frame`: callback opcional en la tarea IR. Recibe metadatos y una
  vista temporal de RAW; copiar los datos necesarios antes de retornar.

La instantánea contiene protocolo, bits, repetición, overflow, bytes AC o valor/
dirección/comando según el protocolo, descripción y marca de tiempo monotónica
al decodificar. `description_truncated` indica si el texto no cupo; los bytes
del estado se conservan por separado. `ESP_OK` en la consulta significa que hay
una captura, no que sea un protocolo AC reconocido.

Los tiempos de la vista RAW se convierten con
`(uint32_t)raw->ticks[i] * raw->tick_us`; el factor es 2 µs en esta versión.
La vista y el puntero de captura solo viven durante el callback. La instantánea
consultable no contiene esos punteros, evitando referencias a buffers reutilizados.

Para una futura integración MQTT/Zigbee, copiar o encolar la captura desde el
callback y procesarla en la tarea correspondiente. No llamar directamente a una
pila Zigbee que requiera su propio contexto/bloqueo. Una recepción no confirma
que el aire haya recibido la señal ni su estado físico; algunos protocolos
envían cambios parciales o acciones de alternancia. No se crea un estado remoto
autoritativo a partir del último texto decodificado.

El componente vive durante todo el arranque; no incorpora parada ni varios
receptores simultáneos. Para reutilizarlo, copiar esta carpeta completa y agregar
`ir_receiver` a `PRIV_REQUIRES` del consumidor, seleccionando GPIO y tamaño de
buffers para la placa destino. `main.c` comprueba además que IR y DS18B20 no
compartan GPIO; otras aplicaciones deben comprobar sus propios conflictos.
Al integrarlo en `on_off_light`, sustituir la implementación y la llamada a
`ir_ac_decoder_start()` anteriores: no iniciar ambos receptores, porque la
biblioteca comparte buffers, temporizador y estado de captura globales.

## Compilación y pruebas

El CMake del proyecto compila únicamente IRremoteIDF con `-Os` para mantener la
detección general compacta. No se editan `managed_components/` ni archivos del
SDK. El log de compilación queda en `build/ir_receiver_build.log`.
En Wired también se colocan en flash el ISR TX y los encoders del RMT usado
por DS18B20 para resolver el límite de IRAM combinado. RX conserva su ISR en
IRAM. Estos ajustes están en `sdkconfig.defaults`; un `sdkconfig` anterior debe
desactivar `RMT_TX_ISR_HANDLER_IN_IRAM` y `RMT_ENCODER_FUNC_IN_IRAM` manualmente
en `menuconfig`, porque los defaults no sustituyen valores existentes. No es
una exigencia propia del decodificador IR para todas las placas.

Validación de IR previa al buzzer con ESP-IDF 5.5.6: `idf.py build` terminó con código
0, sin advertencias del compilador/enlazador. El binario mide 1.479.760 bytes;
quedan 56.240 bytes en la partición de aplicación (4 % aproximado) y 396 bytes
en la región IRAM. El comprobador de tamaños avisa del escaso margen de la
partición. Se conservó idéntica la tabla binaria de particiones. Las siguientes
pruebas físicas todavía no se han realizado.

Una vez autorizada la carga del firmware:

1. Comprobar el mensaje de espera en GPIO27 y la temperatura del DS18B20 cada 5 s.
2. Apuntar el control y pulsar encendido, modo, temperatura y ventilador por
   separado. Comparar protocolo, bytes y descripción entre pulsaciones.
3. Probar mensajes desconocidos y capturas largas: conservar RAW; comprobar que
   el estado se marca desconocido/truncado cuando corresponda.
4. Desactivar RAW y comprobar que continúan los resúmenes y la consulta/callback.
5. Comprobar captura con Wi-Fi conectado y durante caída/restauración del AP.
6. En una unidad sin credenciales, comprobar recepción y heap disponible durante
   el aprovisionamiento BLE Security 2; confirmar persistencia Wi-Fi tras reiniciar.

Compilar no verifica señales físicas, precisión de tiempos con radios activos ni
compatibilidad con el control concreto. No se ejecuta flash ni borrado como parte
de la implementación.

### Diagnóstico de la captura UNKNOWN de 325 tiempos

El RAW aportado en la prueba contiene marcas normales de aproximadamente 584 µs
y dos marcas anómalas de 336 y 90 µs. Los intervalos anteriores se alargan en la
misma proporción: `1884 + 336 = 1636 + 584 = 2220 µs`, y `1014 + 90 ≈ 520 + 584`.
Es compatible con retrasos al atender flancos, no con falta de un decodificador:
se verificó que las fuentes y opciones de protocolos coinciden con `on_off_light`.

Un análisis offline de intervalos entre flancos ascendentes produjo la candidata
`A6 92 07 00 1C 69 11 0B 2F 00 00 00 05 14 B5 00 00 00 00 B5`, compatible con
HAIER_AC160 y sus dos checksums. Esto sustenta la hipótesis de latencia, pero
no convierte la captura original en una lectura válida del firmware. Los
«163 bits» de UNKNOWN son el contador aproximado del hash de la biblioteca;
no una longitud de protocolo ya reconocida.

Se cambió la tarea sin afinidad por una tarea fijada a CPU1 en el ESP32 clásico.
No se ampliaron tolerancias, se alteraron tiempos RAW ni se deshabilitaron
checksums. La compilación verifica el cambio; confirmar en hardware que aparece
`capture core=1` y que el mismo botón produce HAIER_AC160 repetidamente, con
Wi-Fi/BLE activos y DS18B20 funcionando. La mejora física aún no está verificada.
