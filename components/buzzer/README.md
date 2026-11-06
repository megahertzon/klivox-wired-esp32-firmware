# Buzzer activo

Componente C independiente de IR, Wi-Fi y Zigbee. Usa GPIO activo en alto y un
temporizador de apagado, siguiendo el circuito de `on_off_light`. No usa RMT,
LEDC, PWM ni una tarea adicional. Requiere un buzzer **activo**, con oscilador;
un buzzer pasivo necesita una señal alterna y no se controla con este módulo.

## Conexión en Klivox Wired

Con el equipo apagado, para un buzzer pequeño de unos 20 mA como el utilizado
en el proyecto de referencia:

| Conexión | Destino |
| --- | --- |
| GPIO25 / IO25 | Resistencia de 1 kΩ → base del transistor NPN |
| Emisor del NPN | GND común del ESP32 y de la fuente |
| Colector del NPN | Negativo del buzzer |
| Positivo del buzzer | Alimentación nominal del buzzer: +5 V si es de 5 V |
| Resistencia de 10 kΩ | Entre base y emisor/GND |

La alimentación del buzzer no sale del GPIO. No aplicar 5 V al GPIO25. Usar un
transistor NPN adecuado, como el 2N2222 del otro proyecto, identificando sus
terminales con la ficha del fabricante: el orden físico varía. Si el consumo
es mayor, dimensionar el transistor y su resistencia de base para esa carga.
Si es electromagnético, colocar un diodo en paralelo al buzzer: cátodo/raya
al positivo y ánodo al colector/negativo, como en el circuito de referencia.

La resistencia externa de 10 kΩ mantiene el NPN apagado antes de que el
firmware controle el pin. El software precarga LOW antes de habilitar la salida
y lo reafirma después. Inicializar no genera un pitido; el software por sí solo
no garantiza silencio durante ROM/bootloader/reset.

## Configuración

En `idf.py menuconfig` → **Component config → Klivox active buzzer**:

- `CONFIG_KLIVOX_BUZZER_GPIO`: **25** por defecto.
- `CONFIG_KLIVOX_BUZZER_PULSE_MS`: **80 ms** por defecto; rango 50–1000 ms.

La aplicación rechaza un GPIO compartido con IR (27) o DS18B20 (18), antes de
configurar la salida. El componente verifica que el pin admita salida; al
reutilizarlo, elegir un GPIO libre de la placa y comprobar los conflictos propios.

## API y ciclo de vida

- `buzzer_init(config)`: configura GPIO, semáforo y temporizador; llamar desde
  una sola tarea de inicio. Repetir con el mismo GPIO es seguro; cambiar el pin
  después de inicializar se rechaza.
- `buzzer_beep(duration_ms)`: solicita un pulso y retorna sin esperar. Solo para
  contexto de tarea. `ESP_ERR_TIMEOUT` indica que ya está sonando: el pedido se
  omite, sin encolarlo ni prolongar el pitido actual.

- `buzzer_beep_pattern(count, duration_ms, gap_ms)`: solicita 1–8 pitidos de
  50–1000 ms, con pausas de 50–1000 ms cuando hay más de uno. Retorna enseguida;
  mantiene reservado el buzzer durante toda la secuencia, incluidas las pausas.
  Si está ocupado devuelve `ESP_ERR_TIMEOUT`, sin encolar ni extender el sonido.

El callback de `esp_timer` alterna ON/OFF y solo libera el semáforo tras el último
pitido. `buzzer_beep()` conserva la semántica de un único pulso. Si falla el arranque del temporizador, se fuerza LOW antes de
devolver el error. Los recursos viven durante este arranque. No se modifican
NVS ni particiones, y no se introducen dependencias externas.

La duración es nominal: el despacho del temporizador puede retrasarse bajo
carga, por ejemplo durante operaciones de flash. El callback es corto y no
realiza esperas. La tarea IR no espera a que termine el sonido.

La API no es idéntica a la anterior: `buzzer_beep()` en `on_off_light` tenía
semántica síncrona; aquí es siempre asíncrona. Para migrar el componente,
adaptar los consumidores y retirar la implementación antigua; no compilar
ambas porque comparten nombres públicos.

## Integración IR

La política de la aplicación está en `main/app_feedback.c`, registrada mediante
`ir_receiver_config_t.on_frame`. Un resultado con tiempos RAW y sin `overflow`
solicita un pitido de 80 ms. Se incluyen protocolos desconocidos y repeticiones:
el aviso confirma recepción, no decodificación válida ni ejecución por el aire.
Los resultados truncados/inválidos marcados con `overflow` y los vacíos no pitan.

No se usa el límite de 1,5 s del radar del otro proyecto, porque aquí el evento
es una captura IR. Los pedidos que llegan mientras hay un pulso activo se omiten.
La inicialización del buzzer ocurre antes de los radios y de los sensores; si
falla, esos módulos continúan y el callback no solicita pitidos.

Para reutilizarlo, copiar esta carpeta a `components/`, agregar `buzzer` a
`PRIV_REQUIRES` del consumidor y utilizar `include/buzzer.h`. La política de
`app_feedback` pertenece a Wired y debe adaptarse a la aplicación destino.

## Validación física pendiente

Después de una carga autorizada:

1. Comprobar que arranca en silencio y aparece `Active buzzer ready on GPIO25`.
2. Capturar una trama HAIER_AC160: debe oírse un pitido corto y continuar el log
   y la recepción IR en CPU1, junto con las lecturas DS18B20 cada cinco segundos.
3. Probar varias pulsaciones/repeticiones: no deben acumular pitidos ni bloquear
   el decodificador. Una captura desconocida no truncada también solicita aviso.
4. Verificar que el buzzer se apaga incluso mientras se imprime el RAW y que la
   conectividad se mantiene. Observar duración con carga BLE/Wi-Fi si se requiere
   medir el pulso, porque no es una salida con temporización hardware estricta.

El log de compilación es `build/buzzer_build.log`. El build no verifica la carga
eléctrica ni el sonido; no se ejecuta flash ni borrado durante esta implementación.

Build verificado con ESP-IDF 5.5.6, código 0 y sin advertencias de C/enlazado.
Binario de 1.480.752 bytes, 55.248 bytes libres en la partición de aplicación
(4 % aproximado) y 396 bytes de IRAM. El comprobador mantiene el aviso de
partición casi llena. Las implementaciones de los sensores y Wi-Fi y la tabla
binaria de particiones se compararon sin cambios. La prueba física está pendiente.

## Confirmación SETUP

`main/app_feedback.c` programa dos pitidos de 100 ms con pausa de 100 ms al
confirmar la pulsación SETUP de 5 segundos. El componente buzzer desconoce ese
significado: solo ejecuta el patrón. Si un aviso IR ya suena, la aplicación
suspende nuevos avisos IR y reintenta con un temporizador hasta reservar la
pareja. Durante la pausa de la pareja, los avisos IR se rechazan por ocupado.
El reset Wi-Fi/BLE continúa sin esperar a que termine el sonido.

Probar en hardware una pulsación corta (sin sonido), 5 segundos (dos tonos),
15 segundos (solo una pareja) y una segunda pulsación tras liberar (otra pareja).
Repetir con tráfico IR para comprobar la separación de los dos tonos y que el
buzzer termina LOW. Verificar también el pitido IR único después de SETUP.
