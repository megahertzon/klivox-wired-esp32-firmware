idf_component_register(SRCS "main.c" "wifi_manager.c"
                       REQUIRES esp_event esp_netif
                       PRIV_REQUIRES esp_wifi esp_timer nvs_flash
                                     wifi_provisioning protocomm espressif__qrcode
                                     temperature_manager
                       INCLUDE_DIRS ".")
