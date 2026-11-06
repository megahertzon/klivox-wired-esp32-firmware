idf_component_register(SRCS "wifi_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event esp_netif
                       PRIV_REQUIRES esp_wifi esp_timer nvs_flash
                                     wifi_provisioning protocomm freertos
                                     espressif__qrcode)
