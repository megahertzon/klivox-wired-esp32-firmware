# Status LED manager

Componente C independiente de Wi-Fi/MQTT. RGB de ánodo común a GPIO13, con una
resistencia por canal: rojo GPIO21, verde GPIO22, azul GPIO23. SETUP pasa a
GPIO33. No compartir estos pines. La aplicación verifica el mapa antes de tocar
el LED. Se reservan LEDC low-speed timer 0 y canales 0/1/2; los demás componentes
actuales no usan LEDC. Revisar esta reserva al agregar otro consumidor PWM.

GPIO13 se configura como salida HIGH constante después de dejar apagados los
tres canales PWM; permanece alto incluso en OFF o con brillo 0. Es configurable
mediante `KLIVOX_LED_ANODE_GPIO` y se valida que no comparta pin con RGB ni otros
periféricos. Mover el ánodo de 3V3 a GPIO13. Las resistencias deben limitar la
corriente instantánea total de los canales a la capacidad de salida del GPIO;
el brillo PWM bajo no limita por sí solo la corriente instantánea. Para alimentar
cargas mayores, usar una etapa externa dimensionada para esa corriente.
HIGH se garantiza solo después de la inicialización, no durante reset/bootloader.

`linker.lf` coloca en flash los caminos LEDC utilizados desde esta tarea,
incluidas cinco funciones HAL y los auxiliares internos del driver. Evita
exceder la IRAM disponible sin editar el SDK ni cambiar la ubicación del código
de radio/IR. Requiere `CONFIG_LEDC_CTRL_FUNC_IN_IRAM=n` (valor predeterminado).
Esta placa no instala el servicio de fade ni llama LEDC desde ISR o con caché
deshabilitada; si se incorpora cualquiera de esos usos, revisar esta regla de
enlace y el presupuesto IRAM antes de habilitarlo.

LEDC usa 5 kHz y 8 bits; duty físico = 255 - brillo lógico. Inicialmente se
precargan salidas HIGH antes de conectar PWM. Para garantizar apagado durante
reset/bootloader hacen falta pull-ups externos adecuados; el firmware solo
controla las salidas después de arrancar. Brillo predeterminado 16/255, ajustable
en `Component config → Klivox RGB status LED` y mediante la API.

| Estado | Color | Encendido/apagado |
| --- | --- | --- |
| BOOTING | Amarillo | 1000/1000 ms |
| BLE_PROVISIONING | Azul | 250/250 ms |
| WIFI_CONNECTING | Amarillo | 250/250 ms |
| MQTT_CONNECTING | Azul | 1000/1000 ms |
| ONLINE | Verde | Fijo |
| OFFLINE | Rojo | 1000/1000 ms |
| ERROR | Rojo | 250/250 ms |
| WIFI_CONNECTED | Cian | Fijo; solo IPv4, sin afirmar MQTT |
| OFF | Apagado | Continuo |

Agregar `status_led_manager` a `PRIV_REQUIRES` e incluir `status_led_manager.h`.
`init()` es idempotente tras completarse y devuelve el primer resultado; una
llamada concurrente durante el inicio devuelve INVALID_STATE. Los recursos
permanecen durante este arranque. Una sola tarea de 3072 bytes/ prioridad 1
escribe LEDC y duerme hasta notificaciones o el siguiente flanco, sin polling.
Las APIs son para tareas, seguras entre núcleos y sin esperar al patrón.
`get_state()` devuelve BOOTING antes del inicio; consultar el resultado de init.
`state_name(estado)` devuelve el nombre diagnóstico estático o `INVALID` si no
es válido. La aplicación registra ese estado después de cada intento de lectura
de temperatura mediante `on_sample`, sin cambiar el LED.

`set_state()` reemplaza el estado base; ERROR y BLE activan condiciones
superpuestas. `set_condition(estado, true/false)` activa/libera cualquier
condición con prioridad ERROR > BLE > OFFLINE > WIFI_CONNECTING >
MQTT_CONNECTING > WIFI_CONNECTED > ONLINE > BOOTING > OFF. OFFLINE gana el empate
con WIFI_CONNECTING. Cada condición tiene un dueño; no son contadores.
No basta con asignar ONLINE para liberar ERROR o BLE: liberarlos explícitamente.
`set_connectivity(base, provisioning)` actualiza base y condición BLE de forma
atómica, conservando las demás condiciones. Las condiciones manuales de
conectividad también deben liberarse explícitamente por su dueño.

`set_brightness(0..255)` controla brillo; 0 apaga físicamente manteniendo estado.
`set_online_visible(false)` y Kconfig `KLIVOX_LED_HIDE_ONLINE` apagan únicamente
ONLINE y conservan avisos. El estado provisional cian sigue visible.

`main/app_status.c` consume los eventos públicos de Wi-Fi, incluidas nuevas
notificaciones CONNECTING y PROVISIONING_ENDED, y consulta instantáneas para
evitar reproducir estados obsoletos de la cola. BLE se libera al terminar la
liberación real de recursos. Credenciales rechazadas conservan azul para permitir
corrección; fallos internos muestran ERROR, liberado al volver a iniciar BLE o
recibir IP. No se modifica la política de reconexión, NVS ni el callback SETUP.
Como los eventos Wi-Fi existentes son best effort, una cola saturada puede perder
una actualización; la próxima notificación vuelve a consultar el estado actual.
Al integrar MQTT, la aplicación deberá componer sus condiciones reales y liberar
MQTT_CONNECTING al recibir la conexión del broker; Wi-Fi con IP nunca activa ONLINE.

## Pruebas físicas

Con equipo apagado mover SETUP a GPIO33 y cablear el RGB con resistencias.
Conectar el ánodo común a GPIO13. Tras arrancar, medir HIGH en ese pin respecto
a GND; debe mantenerse alto tanto con OFF como durante los parpadeos.
Después de cargar el firmware mediante el procedimiento autorizado:

1. Reiniciar: amarillo lento BOOTING (puede ser breve durante el inicio).
2. Equipo sin credenciales o SETUP sostenido 5 s: azul rápido. Confirmar dos
   pitidos y que otras configuraciones NVS permanecen intactas.
3. Aprovisionar por BLE: azul mientras el SDK siga activo; cian fijo después
   de obtener IP y finalizar BLE. No debe aparecer verde.
4. Cortar alimentación y encender: credenciales persistentes, sin aprovisionar;
   amarillo rápido hasta IP y después cian.
5. Apagar el AP: rojo lento durante espera, amarillo rápido en intentos; al
   recuperar AP debe volver cian sin borrar credenciales.
6. Para estados futuros o de diagnóstico, desde una tarea de desarrollo usar
   `set_state(MQTT_CONNECTING)`, `set_state(ONLINE)`, `set_state(OFFLINE)` y
   `set_state(OFF)`. MQTT_CONNECTING es solo prueba visual, no un cliente MQTT.
   Hacerlo con conectividad sin iniciar para que eventos reales no sustituyan
   la selección. No se incluye un modo de prueba automático en producción.
7. Activar `set_condition(ERROR, true)` sobre BLE y cambiar la base: rojo rápido
   debe prevalecer. Liberar ERROR: azul rápido; liberar BLE: base más reciente.
   Repetir con condiciones WIFI_CONNECTING, MQTT_CONNECTING y ONLINE y liberarlas.
8. Comprobar brillo 0/16/32, colores y periodos con osciloscopio si está disponible.
   Ocultar ONLINE y confirmar que ERROR y BLE siguen visibles. Restaurar brillo 16.

Las constantes de enum en los ejemplos llevan el prefijo `STATUS_LED_`.
Compilar no verifica polaridad, luminosidad, cableado, radio ni persistencia real.
