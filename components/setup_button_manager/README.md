# SETUP button manager

Componente ESP-IDF para un pulsador activo LOW con pull-up interno. En Klivox
Wired se usa GPIO23, 50 ms de antirrebote y 5000 ms de pulsación continua.
No depende de Wi-Fi, NVS ni buzzer: notifica mediante un callback de aplicación.

Agregar `setup_button_manager` a `PRIV_REQUIRES`, incluir
`setup_button_manager.h` y llamar desde una tarea de arranque:

```c
const setup_button_manager_config_t config = {
    .gpio_num = KLIVOX_SETUP_GPIO,
    .hold_ms = KLIVOX_SETUP_HOLD_MS,
    .debounce_ms = KLIVOX_SETUP_DEBOUNCE_MS,
    .on_long_press = on_setup,
};
esp_err_t err = setup_button_manager_init(&config);
```

El consumidor valida conflictos de GPIO antes de iniciar. El componente copia
la configuración y valida entrada con pull-up, callback y tiempos. La tarea de
prioridad 1 duerme hasta un flanco o plazo; la ISR solo guarda nivel/tiempo y
notifica. Comparte el servicio GPIO con IR y entradas, sin desinstalarlo. En
ESP32 dual core lo instala desde CPU1. Los recursos viven durante este arranque.

Un evento por pulsación; mantener 15 s no repite. Rearma tras 50 ms de liberación
estable. El callback se ejecuta en la tarea del botón y debe regresar pronto,
sin esperar sonido, Wi-Fi o BLE. En Wired, `main/app_setup.c` conecta el callback
con el feedback y la API asíncrona del gestor Wi-Fi. Ver pruebas en README raíz.
