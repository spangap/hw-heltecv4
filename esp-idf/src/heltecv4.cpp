/**
 * heltecv4.cpp — Heltec WiFi LoRa 32 (V4) board support, end to end.
 *
 * Single owner of all V4 hardware bring-up. See heltecv4.h for the API contract
 * and the board reference: https://heltec.org/project/wifi-lora-32-v4/. Layout:
 *
 *   1. Vext peripheral power rail + LoRa CS park.
 *      Always compiled. Driven from heltecv4Start() before spangapInit().
 *
 * The SX1262 lives on its own SPI bus (separate from the flash) and is powered
 * directly, not off Vext — so unlike the T-Deck there is no shared-bus SD probe
 * to race. We still park the radio's CS HIGH before loraInit() owns it, and
 * bring Vext up so any rail-powered peripheral added later is live at boot.
 */
#include "heltecv4.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* =========================================================================
 * 1. Vext peripheral power rail + LoRa CS park
 * ========================================================================= */

static void heltecv4PowerInit(void)
{
#if BOARD_VEXT_CTRL_PIN >= 0
    gpio_config_t vext = {};
    vext.pin_bit_mask = 1ULL << BOARD_VEXT_CTRL_PIN;
    vext.mode         = GPIO_MODE_OUTPUT;
    vext.pull_up_en   = GPIO_PULLUP_DISABLE;
    vext.pull_down_en = GPIO_PULLDOWN_DISABLE;
    vext.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&vext);
    gpio_set_level((gpio_num_t)BOARD_VEXT_CTRL_PIN, BOARD_VEXT_ON_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(100));   /* rail settle */
#endif
    /* Park the SX1262's CS HIGH (deselected) so it doesn't drive MISO before
     * loraInit() claims the pin. The LoRa radio CS pin comes from iface-lora's
     * Kconfig (CONFIG_LORA0_CS_PIN); defined only when iface-lora is staged. */
#if defined(CONFIG_LORA0_CS_PIN)
    gpio_config_t cs = {};
    cs.pin_bit_mask = 1ULL << CONFIG_LORA0_CS_PIN;
    cs.mode         = GPIO_MODE_OUTPUT;
    cs.pull_up_en   = GPIO_PULLUP_DISABLE;
    cs.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cs.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cs);
    gpio_set_level((gpio_num_t)CONFIG_LORA0_CS_PIN, 1);
#endif
}

/* =========================================================================
 * Public API — the always-on board bring-up (see heltecv4.h).
 * ========================================================================= */

void heltecv4Start(void) {
    heltecv4PowerInit();   /* Vext rail + LoRa CS park */
}
