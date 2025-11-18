file(GLOB_RECURSE SRC_FILES "src/*.c")

idf_component_register(
    SRCS ${SRC_FILES}       # alle gefundenen .c-Dateien
    INCLUDE_DIRS "hdr"      # Header-Dateien liegen im Ordner hdr
    REQUIRES nvs_flash lvgl esp_lvgl_port esp_lcd esp_driver_rmt
)

