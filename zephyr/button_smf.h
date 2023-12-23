#ifndef BUTTON_SMF_H
#define BUTTON_SMF_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/smf.h>

enum button_smf_event_type
{
    BUTTON_SMF_PRESS,
    BUTTON_SMF_RELEASE,
    // TODO: double click
};

typedef void (*button_callback_cb_t)(void *user_data);

struct button_callback_t
{
    k_timeout_t cb_time;
    button_callback_cb_t cb;
};

struct button_smf_t
{
    struct smf_ctx ctx;
    struct k_mutex lock;
    struct k_event button_event;
    struct k_work_delayable button_debounce_work;
    struct k_work_delayable button_long_press_work;
    struct gpio_callback button_cb_data;

    k_ticks_t button_press_time;
    uint8_t next_press_cb_index;

    struct button_callback_t button_pressed_cb[CONFIG_BUTTON_SMF_MAX_PRESSED_CALLBACKS];
    struct button_callback_t button_released_cb[CONFIG_BUTTON_SMF_MAX_RELEASE_CALLBACKS];

    void *user_data;
    const struct gpio_dt_spec *button_gpio;
};

int button_smf_init(struct button_smf_t *button_smf, const struct gpio_dt_spec *button_gpio, void *user_data);

int button_smf_register_callback(struct button_smf_t *button_smf,
                                 enum button_smf_event_type event,
                                 button_callback_cb_t cb,
                                 k_timeout_t timeout);

#endif // BUTTON_SMF_H