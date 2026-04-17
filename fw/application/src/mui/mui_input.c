#include "mui_input.h"

#include "app_timer.h"
#include "mui_core.h"
#include "mui_event.h"
#include "bsp_btn.h"
#include "nrf_gpio.h"
#include "nrf_log.h"

#include "cache.h"

#define BACK_BUTTON_DEBOUNCE_MS 30

static bool m_back_button_raw_pressed = false;
static bool m_back_button_stable_pressed = false;
static uint32_t m_back_button_last_change_ticks = 0;

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
    bsp_btn_init(mui_input_on_bsp_btn_event);
    nrf_gpio_cfg_input(BACK_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    m_back_button_raw_pressed = nrf_gpio_pin_read(BACK_BUTTON_PIN) == 0;
    m_back_button_stable_pressed = m_back_button_raw_pressed;
    m_back_button_last_change_ticks = app_timer_cnt_get();
}

void mui_input_tick() {
    uint32_t now_ticks = app_timer_cnt_get();
    bool pressed = nrf_gpio_pin_read(BACK_BUTTON_PIN) == 0;

    if (pressed != m_back_button_raw_pressed) {
        m_back_button_raw_pressed = pressed;
        m_back_button_last_change_ticks = now_ticks;
    }

    if (pressed != m_back_button_stable_pressed &&
        app_timer_cnt_diff_compute(now_ticks, m_back_button_last_change_ticks) >= APP_TIMER_TICKS(BACK_BUTTON_DEBOUNCE_MS)) {
        m_back_button_stable_pressed = pressed;

        if (pressed) {
            NRF_LOG_DEBUG("Back key pressed");
            mui_input_event_t input_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_PRESS};
            mui_input_post_event(&input_event);
        } else {
            NRF_LOG_DEBUG("Back key released");
            mui_input_event_t release_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_RELEASE};
            mui_input_event_t short_event = {.key = INPUT_KEY_BACK, .type = INPUT_TYPE_SHORT};
            mui_input_post_event(&release_event);
            mui_input_post_event(&short_event);
        }
    }
}
