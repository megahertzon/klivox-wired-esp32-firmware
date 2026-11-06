idf_component_register(SRCS "app_status.c" "main.c" "app_feedback.c" "app_inputs.c" "app_setup.c"
                       PRIV_REQUIRES wifi_manager temperature_manager ir_receiver buzzer
                                     digital_inputs setup_button_manager status_led_manager esp_timer
                       INCLUDE_DIRS ".")
