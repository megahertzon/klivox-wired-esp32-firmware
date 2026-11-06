idf_component_register(SRCS "ir_receiver.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES jorgecis__irremoteidf esp_driver_gpio
                                     esp_timer freertos heap)
