# Wi-Fi Manager

Componente ESP-IDF que encapsula Wi-Fi Station y aprovisionamiento BLE usando
el Provisioning Manager oficial. Se extrajo de `main/` sin modificar la lógica,
la API, los identificadores de configuración ni los parámetros de seguridad.

```text
wifi_manager/
├── CMakeLists.txt
├── Kconfig
├── idf_component.yml
├── wifi_manager.c
└── include/
    └── wifi_manager.h
```

## Integración

Copiar la carpeta completa a `components/` de una aplicación ESP-IDF compatible
con `>=5.5.6,<5.6`. Agregar `wifi_manager` a `PRIV_REQUIRES` del componente que
lo utiliza e incluir `wifi_manager.h`. Las dependencias de red, FreeRTOS,
aprovisionamiento y QR se declaran aquí; el consumidor no debe duplicarlas.
`esp_event` y `esp_netif` son dependencias públicas porque sus tipos aparecen
en la cabecera.

Desde una sola tarea de arranque, comprobar el resultado de
`wifi_manager_init()` y luego de `wifi_manager_start()`. Los consumidores de
`WIFI_MANAGER_EVENT` deben registrar sus manejadores entre esas dos llamadas.
El arranque no espera a recibir credenciales ni una dirección IP.

La aplicación mantiene el orden general de inicialización: reserva primero
los buffers IR, inicia conectividad y los demás módulos, y finalmente comienza
la recepción IR. El movimiento del componente no cambia ese orden.

## API y eventos

- `wifi_manager_init()`: inicializa NVS, netif, el event loop predeterminado,
  el controlador Wi-Fi y el manager oficial; comprueba credenciales guardadas.
- `wifi_manager_start()`: conecta con las credenciales existentes o inicia el
  servicio de aprovisionamiento. No anuncia BLE cuando ya está provisionado.
- `wifi_manager_is_connected()`: instantánea de disponibilidad IPv4.
- `wifi_manager_is_provisioned()`: credenciales detectadas al arrancar o alta
  inicial validada; no depende de que el AP esté disponible en ese momento.

El event base público `WIFI_MANAGER_EVENT` ofrece `CONNECTED`, `DISCONNECTED`,
`PROVISIONING_STARTED`, `PROVISIONING_SUCCESS` y `PROVISIONING_FAILED` con el
prefijo `WIFI_MANAGER_EVENT_`. Tipos, datos y límites de los callbacks se
describen en `include/wifi_manager.h`. Los callbacks deben ser cortos y delegar
operaciones de red a otras tareas. Si la cola de eventos se llena, el aviso
puede descartarse; las consultas de estado permiten reconciliar al consumidor.

## Configuración y persistencia

En `idf.py menuconfig` → **Component config → Klivox Wi-Fi manager configuration**.
Se conservan los símbolos `EXAMPLE_PROV_*` y `KLIVOX_WIFI_RETRY_*` para que los
valores actuales de `sdkconfig` y `sdkconfig.defaults` sigan siendo válidos.

La configuración Wired conserva BLE, Security 2 Development, nombre de servicio,
UUID, salt/verifier, credenciales de desarrollo y QR existentes. La rama de
producción requiere material único por dispositivo y sigue sin implementar;
devuelve un error en lugar de usar silenciosamente el material de desarrollo.

El módulo no registra la contraseña de la red. El QR de desarrollo sigue
conteniendo el PoP de aprovisionamiento, que es distinto de esa contraseña.
No borra NVS ante errores de inicialización ni elimina credenciales por perder
temporalmente el AP. La corrección de credenciales nuevas rechazadas durante
el alta inicial conserva el comportamiento del ejemplo oficial.

Después de aprovisionar se libera el manager y la memoria Bluetooth. La
reconexión normal usa un temporizador con espera progresiva de 1 a 60 segundos.
No hay API de parada ni reprovisionamiento durante el mismo arranque: la
liberación de memoria BLE forma parte del ciclo de vida actual. Este módulo
es el propietario del controlador Wi-Fi; no inicializar otro driver/manager
de red sobre él desde otro componente.

## Validación de la extracción

Se compara el contenido de la implementación, cabecera y Kconfig con su origen,
los valores efectivos de `sdkconfig` y el binario de particiones. La compilación
del proyecto se registra en `build/wifi_component_build.log`.

`idf.py build` finalizó con código 0 en ESP-IDF 5.5.6, con el mismo tamaño de
firmware anterior: 1.479.760 bytes. Se confirmaron idénticos la implementación,
cabecera y Kconfig trasladados, los valores efectivos de configuración, el
binario de particiones y el código de inicialización, IR y DS18B20. El margen
sigue en 4 % aproximado de la partición de aplicación y 396 bytes de IRAM;
se mantiene el aviso del comprobador de tamaños sobre la partición casi llena.
No se cargó firmware ni se borró memoria para validar esta extracción.

Después de una carga autorizada, comprobar reconexión con credenciales guardadas,
persistencia tras cortar alimentación y recuperación al restaurar el AP. Para
revisar el alta BLE, usar una unidad sin credenciales y comprobar Security 2,
obtención de IP y liberación de Bluetooth. Comprobar también que siguen los logs
del DS18B20 y las capturas IR. La compilación no sustituye esas pruebas físicas.
