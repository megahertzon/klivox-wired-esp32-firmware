idf_component_register(SRCS "main.c" "app_feedback.c" "app_inputs.c"
                       PRIV_REQUIRES wifi_manager temperature_manager ir_receiver buzzer
                                     digital_inputs
                       INCLUDE_DIRS ".")
