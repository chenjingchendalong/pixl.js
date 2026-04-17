#include "mui_input.h"

#include "app_timer.h"
#include "mui_core.h"
#include "mui_event.h"
#include "bsp_btn.h"
#include "nrf_gpio.h"
#include "nrf_log.h"

#include "cache.h"

#define BACK_BUTTON_DEBOUNCE_MS 30

static const uint8_t m_back_button_pins[BACK_BUTTON_CANDIDATE_PINS_COUNT] = BACK_BUTTON_CANDIDATE_PINS;
static bool m_back_button_raw_pressed[BACK_BUTTON_CANDIDATE_PINS_COUNT] = {0};
static bool m_back_button_stable_pressed[BACK_BUTTON_CANDIDATE_PINS_COUNT] = {0};
static uint32_t m_back_button_last_change_ticks[BACK_BUTTON_CANDIDATE_PINS_COUNT] = {0};

static bool back_button_is_pressed(uint8_t pin) { return nrf_gpio_pin_read(pin) == BACK_BUTTON_ACTIVE_STATE; }

static void mui_input_post_event(mui_input_event_t *p_input_event) {
    uint32_t arg = p_input_event->type;
    arg <<= 8;
    arg += p_input_event->key;
    mui_event_t mui_event = {.id = MUI_EVENT_ID_INPUT, .arg_int = arg};
    mui_post(mui(), &mui_event);
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
    for (uint32_t i = 0; i < BACK_BUTTON_CANDIDATE_PINS_COUNT; i++) {
        nrf_gpio_cfg_input(m_back_button_pins[i], BACK_BUTTON_PULL);
        m_back_button_raw_pressed[i] = back_button_is_pressed(m_back_button_pins[i]);
        m_back_button_stable_pressed[i] = m_back_button_raw_pressed[i];
        m_back_button_last_change_ticks[i] = now_ticks;
    }
}

void mui_input_tick() {
    uint32_t now_ticks = app_timer_cnt_get();
    for (uint32_t i = 0; i < BACK_BUTTON_CANDIDATE_PINS_COUNT; i++) {
        bool pressed = back_button_is_pressed(m_back_button_pins[i]);

        if (pressed != m_back_button_raw_pressed[i]) {
            m_back_button_raw_pressed[i] = pressed;
            m_back_button_last_change_ticks[i] = now_ticks;
        }

        if (pressed != m_back_button_stable_pressed[i] &&
            app_timer_cnt_diff_compute(now_ticks, m_back_button_last_change_ticks[i]) >=
                APP_TIMER_TICKS(BACK_BUTTON_DEBOUNCE_MS)) {
            m_back_button_stable_pressed[i] = pressed;

            if (pressed) {
                NRF_LOG_DEBUG("Back key candidate pin %d pressed", m_back_button_pins[i]);
                mui_input_event_t input_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_PRESS};
                mui_input_post_event(&input_event);
            } else {
                NRF_LOG_DEBUG("Back key candidate pin %d released", m_back_button_pins[i]);
                mui_input_event_t release_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_RELEASE};
                mui_input_event_t short_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_SHORT};
                mui_input_post_event(&release_event);
                mui_input_post_event(&short_event);
            }
        }
    }
}
