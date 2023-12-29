#ifndef BUTTON_SMF_H
#define BUTTON_SMF_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/smf.h>

typedef void (*button_callback_cb_t)(const struct device *);

int button_smf_register_press_cb(const struct device *dev,
                                 button_callback_cb_t cb,
                                 k_timeout_t timeout);

int button_smf_register_release_cb(const struct device *dev,
                                   button_callback_cb_t cb,
                                   k_timeout_t timeout);

int button_smf_register_double_click_callback(const struct device *dev,
                                              button_callback_cb_t cb);
#endif // BUTTON_SMF_H