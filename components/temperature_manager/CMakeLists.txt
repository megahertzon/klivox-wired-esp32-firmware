idf_component_register(SRCS "temperature_manager.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES espressif__ds18b20 espressif__onewire_bus
                                     esp_driver_gpio esp_timer freertos)
