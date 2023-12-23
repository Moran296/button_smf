#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "button_smf.h"

LOG_MODULE_REGISTER(main);

void button_pressed(void *user_data)
{
        LOG_INF("button pressed\n");
}
void button_released(void *user_data)
{
        LOG_INF("button released\n");
}

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
struct button_smf_t button_smf;

void fast_press(void *user_data)
{
        LOG_INF("fast press\n");
}

void long_press(void *user_data)
{
        LOG_INF("long press\n");
}

void longest_press(void *user_data)
{
        LOG_INF("longest press\n");
}

void fast_release(void *user_data)
{
        LOG_INF("fast release\n");
}

void long_release(void *user_data)
{
        LOG_INF("long release\n");
}

void longest_release(void *user_data)
{
        LOG_INF("longest release\n");
}

int main(void)
{
        int ret = button_smf_init(&button_smf, &button, NULL);
        if (ret != 0)
        {
                LOG_ERR("button_smf_init failed\n");
                return ret;
        }

        /*
                Register press callbacks.
                All callbacks will be called in order while button is pressed
                So if you press the button for 6 seconds, only fast_press and long_press will be called
        */
        button_smf_register_callback(&button_smf, BUTTON_SMF_PRESS, fast_press, K_NO_WAIT);
        button_smf_register_callback(&button_smf, BUTTON_SMF_PRESS, long_press, K_SECONDS(5));
        button_smf_register_callback(&button_smf, BUTTON_SMF_PRESS, longest_press, K_SECONDS(10));

        /*
                Register release callbacks.
                Only the last callback (before release time) will be called
                So if you press the button for 6 seconds, only long_release will be called
        */
        button_smf_register_callback(&button_smf, BUTTON_SMF_RELEASE, fast_release, K_NO_WAIT);
        button_smf_register_callback(&button_smf, BUTTON_SMF_RELEASE, long_release, K_SECONDS(5));
        button_smf_register_callback(&button_smf, BUTTON_SMF_RELEASE, longest_release, K_SECONDS(10));

        LOG_INF("Sample app started");
        return 0;
}
