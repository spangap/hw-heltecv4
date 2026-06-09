# hw-heltecv4 board HAL — an ordinary spangap component (no longer a buildable).
#
# Compiles the Heltec WiFi LoRa 32 (V4) board code: the Vext peripheral power
# rail and the LoRa CS park (heltecv4.cpp). No on-device UI.
#
# app_main is NOT here — it's generated into the reticulous/reticulous buildable
# that adds this board with `--with`. This component just exports the board
# start: hook symbol (heltecv4Start) the generated dispatcher calls.
include(${CMAKE_CURRENT_LIST_DIR}/spangap_requires.cmake)
idf_component_register(
    SRCS
        "src/heltecv4.cpp"
        ${SPANGAP_CONDITIONAL_SRCS}
    INCLUDE_DIRS "include" ${SPANGAP_CONDITIONAL_INCLUDE_DIRS}
    REQUIRES
        ${SPANGAP_REQUIRES}
        driver
        esp_event
        esp_netif
        esp_wifi
        lwip
)
