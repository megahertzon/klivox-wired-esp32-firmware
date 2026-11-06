idf_component_register(SRCS "digital_inputs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event
                       PRIV_REQUIRES esp_driver_gpio esp_timer freertos)
