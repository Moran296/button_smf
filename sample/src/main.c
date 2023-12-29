#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "button_smf.h"

LOG_MODULE_REGISTER(main);

void double_click(const struct device *button_smf)
{
        LOG_INF("double click!!!\n");
}

void button_pressed(const struct device *button_smf)
{
        LOG_INF("button pressed\n");
}
void button_released(const struct device *button_smf)
{
        LOG_INF("button released\n");
}

void fast_press(const struct device *button_smf)
{
        LOG_INF("fast press\n");
}

void long_press(const struct device *button_smf)
{
        LOG_INF("long press\n");
}

void longest_press(const struct device *button_smf)
{
        LOG_INF("longest press\n");
}

void fast_release(const struct device *button_smf)
{
        LOG_INF("fast release\n");
}

void long_release(const struct device *button_smf)
{
        LOG_INF("long release\n");
}

void longest_release(const struct device *button_smf)
{
        LOG_INF("longest release\n");
}

static const struct device *button_smf = DEVICE_DT_GET_ONE(buttonsmf);

int main(void)
{
        /*
                Register press callbacks.
                All callbacks will be called in order while button is pressed
                So if you press the button for 6 seconds, only fast_press and long_press will be called
        */
        LOG_INF("device is %p, name is %s", button_smf, button_smf->name);

        button_smf_register_press_cb(button_smf, fast_press, K_NO_WAIT);
        button_smf_register_press_cb(button_smf, long_press, K_SECONDS(5));
        button_smf_register_press_cb(button_smf, longest_press, K_SECONDS(10));

        /*
                Register release callbacks.
                Only the last callback (before release time) will be called
                So if you press the button for 6 seconds, only long_release will be called
        */
        button_smf_register_release_cb(button_smf, fast_release, K_NO_WAIT);
        button_smf_register_release_cb(button_smf, long_release, K_SECONDS(5));
        button_smf_register_release_cb(button_smf, longest_release, K_SECONDS(10));

        button_smf_register_double_click_callback(button_smf, double_click);

        LOG_INF("Sample app started");
        return 0;
}
