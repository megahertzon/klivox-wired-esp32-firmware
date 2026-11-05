# Klivox Wired — gestión Wi-Fi y aprovisionamiento

Firmware C para ESP32 clásico, ESP-IDF 5.5.6 y FreeRTOS. La refactorización
conserva el Provisioning Manager oficial y no incorpora un cliente MQTT.

## Archivos

- `main/main.c`: inicialización general; inicia conectividad y retorna sin esperar IP.
- `main/wifi_manager.c`: NVS, netif, event loop, Wi-Fi Station, aprovisionamiento,
  eventos y temporizador de reconexión.
- `main/wifi_manager.h`: API, eventos y tipos públicos de los mensajes.
- `main/CMakeLists.txt`: compila exclusivamente `main.c` y `wifi_manager.c`;
  declara las dependencias de ESP-IDF y la biblioteca QR ya existente.
- `main/Kconfig.projbuild`: conserva BLE y Security 2; agrega los tiempos de
  reconexión. Elimina los modos del ejemplo que borraban credenciales en cada
  arranque o repetían el aprovisionamiento periódicamente.
- `main/idf_component.yml`: conserva QR y declara ESP-IDF `>=5.5.6,<5.6`.
  `dependencies.lock` se actualiza al resolver ese requisito, sin agregar paquetes.
- `sdkconfig`: conserva la seguridad y partición actuales; incorpora los dos
  parámetros nuevos de reconexión al regenerarse durante el build.

`main/app_main.c` se convirtió en los tres archivos de código indicados.
`main/main_backup.c`, que ya existía, queda intacto y fuera de la compilación.
La copia original anterior a esta refactorización está en
`.refactor-backup/wifi-manager-original/`, incluyendo configuración y particiones.

## Uso y eventos para futuros componentes

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

Validación realizada el 5 de noviembre de 2026: `idf.py build` terminó con código
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
