idf_component_register(SRCS "main.c" "client.c" "connect.c"
                    INCLUDE_DIRS "."
                    REQUIRES lwip
                             esp_wifi
                             console
                             nvs_flash
                             )
