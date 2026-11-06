idf_component_register(SRCS "status_led_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc freertos
                       LDFRAGMENTS "linker.lf")
