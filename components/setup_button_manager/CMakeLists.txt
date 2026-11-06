idf_component_register(SRCS "setup_button_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio
                       PRIV_REQUIRES esp_timer freertos)
