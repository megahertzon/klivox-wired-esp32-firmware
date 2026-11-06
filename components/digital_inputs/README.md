# Entradas digitales: contactos NC y pulsadores NO

Componente C reutilizable para ESP-IDF y FreeRTOS. Configura hasta cinco entradas
con pull-up, nivel activo y antirrebote independientes. No depende de Wi-Fi,
Zigbee, IR ni actuadores. La asignación y los nombres de Klivox están en
`main/app_inputs.c`, fuera del driver. Esta tarjeta configura tres entradas;
el driver conserva capacidad para hasta cinco en otras placas. GPIO21 y GPIO22
pertenecen al LED RGB, sin entradas ni eventos de subir/bajar temperatura.

## Cableado predeterminado

Con el equipo apagado, conectar cada contacto seco o pulsador entre su GPIO y
GND común. Usar una resistencia externa individual de **10 kΩ entre cada GPIO
y 3V3**. Se habilitan también los pull-ups internos en los GPIO que los admiten.
No conectar tensiones externas de 5/12 V a estas entradas.

| Índice | Entrada | GPIO | Estado activo |
| --- | --- | --- | --- |
| 0 | Contacto NC 1, puerta/ventana | 32 | HIGH: circuito abierto |
| 1 | Contacto NC 2, puerta/ventana | 26 | HIGH: circuito abierto |
| 2 | Pulsador NO encendido/apagado | 19 | LOW: pulsado |

```text
3V3 ---[10 kΩ]---+--- GPIO
                |
           contacto / pulsador
                |
GND ------------+
```

Para cada sensor NC, comprobar continuidad con la puerta/ventana cerrada:
debe cerrar el circuito a GND (LOW) y abrirlo al abrir la puerta (HIGH).
El nombre NC del fabricante puede referirse a otra posición del imán; confirmar
el comportamiento real antes de instalar. Un cable cortado o un contacto NC
sin conectar también se registra abierto: no se distinguen esas causas.
El pulsador NO queda HIGH en reposo y une GPIO con GND al pulsarlo.
Los números son GPIO, no posiciones físicas del conector de la placa.

GPIO18 sigue reservado para DS18B20, GPIO27 para VS1838B y GPIO25 para buzzer.
Cambiar los tres GPIO y los tiempos desde **Klivox digital inputs** en
`idf.py menuconfig`. La aplicación rechaza conflictos con esos componentes;
el driver rechaza GPIO inválidos o duplicados antes de configurarlos. Al
reasignar pines, comprobar además las conexiones y reservas de la placa concreta.
Los GPIO34–39 del ESP32 clásico requieren pull-up externo.

## Estados y eventos

1. `digital_inputs_init(config)` copia la configuración e inicializa o reutiliza
   el event loop predeterminado. Llamar desde la tarea de arranque una sola vez.
2. Registrar consumidores con `esp_event_handler_register` para
   `DIGITAL_INPUTS_EVENT` / `DIGITAL_INPUTS_EVENT_CHANGED`.
3. `digital_inputs_start()` crea una tarea. El arranque de las interrupciones
   ocurre dentro de ella: un error se registra y aparece en `get_state`.
4. `digital_inputs_get_state(index, &state)` devuelve una instantánea sin esperar:
   `ESP_ERR_INVALID_STATE` antes de estabilizar, `ESP_ERR_TIMEOUT` si está ocupada
   o el error de arranque. Solo escribe la salida en `ESP_OK`.

Cada evento lleva una copia de `digital_input_state_t`: índice, GPIO, nivel
físico `level_high`, estado lógico `active`, `initial`, contador de cambios y
tiempo de confirmación desde el arranque en microsegundos. `active=true` indica
contacto abierto para NC o pulsador presionado para NO. La primera observación
estable tiene `initial=true` y contador cero; las siguientes incrementan el
contador. Los punteros de datos del evento solo son válidos en su manejador.

Para interpretar una pulsación usar el índice de botón y
`state->active && !state->initial`. Así un botón mantenido al encender no produce
una orden; habrá que liberarlo y pulsarlo de nuevo. También se notifican las
liberaciones. No hay repetición automática al mantenerlo presionado.

El callback opcional `on_change` recibe los mismos estados en la tarea de
entradas; el evento se publica antes de llamarlo. Copiar los datos y mantener
cortos ambos tipos de callbacks. Delegar operaciones de red o actuadores a sus
tareas. La publicación al event loop no espera: si su cola está llena se avisa
del descarte; la instantánea sigue actualizada para reconciliar estados. Para
comandos de botón que deban entregarse con garantía, el consumidor necesitará
una cola y política de desbordamiento propias.

`main/app_inputs.c` aplica la asignación anterior y registra en log los estados
y `ACCION SOLICITADA: POWER_TOGGLE` al pulsar. Por ahora
no transmite IR, no modifica temperaturas ni supone que el aire esté encendido.
En `main.c` se pueden registrar consumidores entre `app_inputs_init()` y
`app_inputs_start()`. Los índices de aplicación están en `app_inputs.h`.

## Antirrebote y coexistencia

Ventana predeterminada de **50 ms**, configurable entre 10 y 1000 ms por
entrada. Cada flanco reinicia solo el plazo de esa entrada. La ISR únicamente
marca qué canal cambió y despierta la tarea. La tarea duerme hasta el siguiente
flanco o vencimiento; cuando todos están estables no hace polling periódico.
Al arrancar también confirma y publica el estado inicial de cada entrada.

En ESP32 de dos núcleos, instala los manejadores desde una tarea en **CPU1**,
igual que IR. Ambos reutilizan el servicio GPIO global sin desinstalarlo;
esto conserva la afinidad de las interrupciones IR y evita trasladarlas a
CPU0 al instalar el primer manejador. En targets de un núcleo usa CPU0.
Si IR ya instaló el servicio, el SDK puede imprimir `GPIO isr service already
installed`: se acepta `ESP_ERR_INVALID_STATE` y se reutiliza el servicio.
La ISR de este módulo usa flags=0, igual que la biblioteca IR: puede posponerse
durante operaciones con caché deshabilitada. No ofrece captura de pulsos
rápidos ni garantiza observar pulsaciones durante esas pausas.
El ciclo de vida es una inicialización y arranque por boot, sin API de parada.

Para reutilizarlo en Zigbee, copiar `components/digital_inputs`, declarar
`digital_inputs` como dependencia y configurar GPIO libres de esa placa.
No copiar automáticamente el mapa del ESP32 clásico ni asumir la misma afinidad
de CPU. El consumidor Zigbee debe respetar el contexto y bloqueo de su pila.

## Pruebas físicas pendientes

- Abrir/cerrar cada NC: HIGH abierto, LOW cerrado; sin cambios duplicados tras
  rebotes. Desconectar el cable: abierto. Arrancar abierto y cerrado.
- Pulsar/liberar cada NO: LOW pulsado y HIGH liberado, una solicitud por
  pulsación estable. Mantenerlo pulsado: sin repetición; arrancar pulsado:
  estado inicial sin solicitud de acción.
- Cambiar dos entradas a la vez y provocar rebote en una: la otra debe seguir
  confirmándose de forma independiente.
- Confirmar log de instalación GPIO en core=1, recepción HAIER_AC160 y pitido
  corto al mismo tiempo que contactos, botones y temperatura cada 5 segundos.
- Confirmar Wi-Fi con credenciales persistentes tras cortar alimentación y,
  en una unidad sin configurar, coexistencia durante aprovisionamiento BLE.

Compilar verifica integración y recursos; estas pruebas requieren hardware.
