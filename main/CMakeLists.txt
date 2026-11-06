idf_component_register(SRCS "main.c" "app_feedback.c"
                       PRIV_REQUIRES wifi_manager temperature_manager ir_receiver buzzer
                       INCLUDE_DIRS ".")
