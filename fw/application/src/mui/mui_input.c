#include "mui_input.h"

#include "app_timer.h"
#include "mui_core.h"
#include "mui_event.h"
#include "bsp_btn.h"
#include "nrf_drv_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_pwr_mgmt.h"

#include "cache.h"

#define BACK_BUTTON_DEBOUNCE_MS 30

static const uint8_t m_back_button_pins[BACK_BUTTON_CANDIDATE_PINS_COUNT] = BACK_BUTTON_CANDIDATE_PINS;
static bool m_back_button_stable_pressed[BACK_BUTTON_CANDIDATE_PINS_COUNT] = {0};
static uint32_t m_back_button_last_event_ticks[BACK_BUTTON_CANDIDATE_PINS_COUNT] = {0};
static bool m_back_debug_valid = false;
static uint8_t m_back_debug_pin = 0;
static bool m_back_debug_pressed = false;
static uint32_t m_back_debug_ticks = 0;

static bool back_button_is_pressed(uint8_t pin) { return nrf_gpio_pin_read(pin) == BACK_BUTTON_ACTIVE_STATE; }

bool mui_input_get_back_debug_text(char *text, size_t text_size) {
    if (!m_back_debug_valid || text == NULL || text_size == 0) {
        return false;
    }

    if (app_timer_cnt_diff_compute(app_timer_cnt_get(), m_back_debug_ticks) > APP_TIMER_TICKS(3000)) {
        return false;
    }

    snprintf(text, text_size, "BK P%02d %s", m_back_debug_pin, m_back_debug_pressed ? "DN" : "UP");
    return true;
}

static int32_t back_button_find_pin_index(uint8_t pin) {
    for (uint32_t i = 0; i < BACK_BUTTON_CANDIDATE_PINS_COUNT; i++) {
        if (m_back_button_pins[i] == pin) {
            return (int32_t)i;
        }
    }
    return -1;
}

static void mui_input_post_event(mui_input_event_t *p_input_event) {
    uint32_t arg = p_input_event->type;
    arg <<= 8;
    arg += p_input_event->key;
    mui_event_t mui_event = {.id = MUI_EVENT_ID_INPUT, .arg_int = arg};
    mui_post(mui(), &mui_event);
}

static void back_button_gpiote_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    UNUSED_PARAMETER(action);

    int32_t idx = back_button_find_pin_index((uint8_t)pin);
    if (idx < 0) {
        return;
    }

    uint32_t now_ticks = app_timer_cnt_get();
    if (app_timer_cnt_diff_compute(now_ticks, m_back_button_last_event_ticks[idx]) < APP_TIMER_TICKS(BACK_BUTTON_DEBOUNCE_MS)) {
        return;
    }

    m_back_button_last_event_ticks[idx] = now_ticks;

    bool pressed = back_button_is_pressed((uint8_t)pin);
    if (pressed == m_back_button_stable_pressed[idx]) {
        return;
    }

    m_back_button_stable_pressed[idx] = pressed;
    m_back_debug_valid = true;
    m_back_debug_pin = (uint8_t)pin;
    m_back_debug_pressed = pressed;
    m_back_debug_ticks = now_ticks;
    nrf_pwr_mgmt_feed();

    if (pressed) {
        NRF_LOG_DEBUG("Back key candidate pin %d pressed", pin);
        mui_input_event_t input_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_PRESS};
        mui_input_post_event(&input_event);
    } else {
        NRF_LOG_DEBUG("Back key candidate pin %d released", pin);
        mui_input_event_t release_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_RELEASE};
        mui_input_event_t short_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_SHORT};
        mui_input_post_event(&release_event);
        mui_input_post_event(&short_event);
    }
}


void mui_input_on_bsp_btn_event(uint8_t btn, bsp_btn_event_t evt) {
    switch (evt) {

    case BSP_BTN_EVENT_PRESSED: {
        NRF_LOG_DEBUG("Key %d pressed", btn);
        mui_input_event_t input_event = {.key = btn, .type = INPUT_TYPE_PRESS};
        mui_input_post_event(&input_event);
        break;
    }

    case BSP_BTN_EVENT_RELEASED: {
        NRF_LOG_DEBUG("Key %d released", btn);
        mui_input_event_t input_event = {.key = btn, .type = INPUT_TYPE_RELEASE};
        mui_input_post_event(&input_event);
        break;
    }

    case BSP_BTN_EVENT_SHORT: {
        NRF_LOG_DEBUG("Key %d short push", btn);
        mui_input_event_t input_event = {.key = btn,
                                         .type = INPUT_TYPE_SHORT};
        mui_input_post_event(&input_event);
        break;
    }

    case BSP_BTN_EVENT_LONG: {
        NRF_LOG_DEBUG("Key %d long push", btn);
        mui_input_event_t input_event = {.key = btn,
                                         .type = INPUT_TYPE_LONG};
        mui_input_post_event(&input_event);

        break;
    }

     case BSP_BTN_EVENT_REPEAT: {
        NRF_LOG_DEBUG("Key %d repeat push", btn);
        mui_input_event_t input_event = {.key = btn,
                                         .type = INPUT_TYPE_REPEAT};
        mui_input_post_event(&input_event);

        break;
    }

    default:
        break;
    }
}


void mui_input_init() {
    uint32_t now_ticks = app_timer_cnt_get();

    bsp_btn_init(mui_input_on_bsp_btn_event);
    if (!nrf_drv_gpiote_is_init()) {
        ret_code_t err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    for (uint32_t i = 0; i < BACK_BUTTON_CANDIDATE_PINS_COUNT; i++) {
        nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
        config.pull = BACK_BUTTON_PULL;

        ret_code_t err_code = nrf_drv_gpiote_in_init(m_back_button_pins[i], &config, back_button_gpiote_handler);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Back key pin %d init failed: %d", m_back_button_pins[i], err_code);
            continue;
        }

        m_back_button_stable_pressed[i] = back_button_is_pressed(m_back_button_pins[i]);
        m_back_button_last_event_ticks[i] = now_ticks;
        nrf_drv_gpiote_in_event_enable(m_back_button_pins[i], true);
    }
}

void mui_input_tick() {}
