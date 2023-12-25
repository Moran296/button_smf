#include <zephyr/logging/log.h>
#include "button_smf.h"

LOG_MODULE_REGISTER(button_smf, CONFIG_BUTTON_SMF_LOG_LEVEL);

enum button_state
{
    BUTTON_STATE_IDLE,
#if IS_ENABLED(CONFIG_BUTTON_SMF_DOUBLE_CLICK)
    BUTTON_STATE_DOUBLE_CLICK,
#endif
    BUTTON_STATE_PRESSED
};

enum button_event
{
    BUTTON_EVENT_PRESS = BIT(0),
    BUTTON_EVENT_RELEASE = BIT(1),
    BUTTON_EVENT_TIMEOUT = BIT(2),
};

static void idle_entry(void *o);
static void idle_run(void *o);
static void idle_exit(void *o);
#if IS_ENABLED(CONFIG_BUTTON_SMF_DOUBLE_CLICK)
static void double_click_pend_entry(void *o);
static void double_click_pend_run(void *o);
static void double_click_pend_exit(void *o);
#endif
static void pressed_entry(void *o);
static void pressed_run(void *o);
static void pressed_exit(void *o);

const struct smf_state button_states[] = {
    [BUTTON_STATE_IDLE] = SMF_CREATE_STATE(idle_entry, idle_run, idle_exit),
#if IS_ENABLED(CONFIG_BUTTON_SMF_DOUBLE_CLICK)
    [BUTTON_STATE_DOUBLE_CLICK] = SMF_CREATE_STATE(double_click_pend_entry, double_click_pend_run, double_click_pend_exit),
#endif
    [BUTTON_STATE_PRESSED] = SMF_CREATE_STATE(pressed_entry, pressed_run, pressed_exit)};

static void execute_button_press_cb(struct button_smf_data_t *button_smf)
{
    uint8_t i = button_smf->next_press_cb_index;
    k_timeout_t current_cb_time = button_smf->button_pressed_cb[i].cb_time;

    // run all callbacks with the same timeout
    for (; i < CONFIG_BUTTON_SMF_MAX_PRESSED_CALLBACKS; i++)
    {
        bool is_cb_time = K_TIMEOUT_EQ(button_smf->button_pressed_cb[i].cb_time, current_cb_time);
        bool is_cb_exists = button_smf->button_pressed_cb[i].cb != NULL;

        if (is_cb_time && is_cb_exists)
        {
            LOG_DBG("Executing button pressed callback: %d", i);
            button_smf->button_pressed_cb[i].cb(button_smf->user_data);
        }
        else
        {
            break;
        }
    }

    button_smf->next_press_cb_index = i;
}

static void execute_button_release_cb(struct button_smf_data_t *button_smf)
{
    k_ticks_t now = k_uptime_ticks();
    k_ticks_t press_duration = now - button_smf->button_press_time;

    struct button_callback_t *button_released_cb = NULL;

    // find the callback with the biggest timeout that is smaller or equal to press_duration
    for (int i = 0; i < CONFIG_BUTTON_SMF_MAX_RELEASE_CALLBACKS; i++)
    {
        // break when no more callbacks (as array is sorted)
        bool is_cb_exists = button_smf->button_released_cb[i].cb != NULL;
        if (!is_cb_exists)
        {
            break;
        }

        bool is_timeout_passed = button_smf->button_released_cb[i].cb_time.ticks <= press_duration;
        if (is_timeout_passed)
        {
            button_released_cb = &button_smf->button_released_cb[i];
        }
    }

    if (button_released_cb != NULL)
    {
        LOG_DBG("Executing button released callback");
        button_released_cb->cb(button_smf->user_data);
    }
}

static void reschedule_long_press_work(struct button_smf_data_t *button_smf)
{
    int i = button_smf->next_press_cb_index;

    if (i >= CONFIG_BUTTON_SMF_MAX_PRESSED_CALLBACKS)
    {
        return;
    }

    if (button_smf->button_pressed_cb[i].cb == NULL)
    {
        return;
    }

    if (K_TIMEOUT_EQ(button_smf->button_pressed_cb[i].cb_time, K_NO_WAIT))
    {
        return;
    }

    // time to next callback is the delta between the cb timeout and time that already passed since press
    k_ticks_t time_since_press = k_uptime_ticks() - button_smf->button_press_time;
    // TODO: check for overflow
    k_ticks_t time_to_next_cb = button_smf->button_pressed_cb[i].cb_time.ticks - time_since_press;

    LOG_DBG("Rescheduling long press work for %lld ticks", time_to_next_cb);
    k_work_reschedule(&button_smf->button_long_press_work, K_TICKS(time_to_next_cb));
}

static void idle_entry(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;

    button_smf->next_press_cb_index = 0;
    button_smf->button_press_time = 0;
}

static void idle_run(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;
    uint32_t events = button_smf->button_event.events;

    k_event_clear(&button_smf->button_event, 0xFF);
    if (events & BUTTON_EVENT_PRESS)
    {
#if IS_ENABLED(CONFIG_BUTTON_SMF_DOUBLE_CLICK)
        // go to double click pending state only if double click callback is registered
        enum button_state state = button_smf->double_click_cb == NULL ? BUTTON_STATE_PRESSED : BUTTON_STATE_DOUBLE_CLICK;
        smf_set_state(SMF_CTX(button_smf), &button_states[state]);
#else
        smf_set_state(SMF_CTX(button_smf), &button_states[BUTTON_STATE_PRESSED]);
#endif
    }
}

static void idle_exit(void *o)
{
    // pass
}

static void pressed_entry(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;

    button_smf->button_press_time = k_uptime_ticks();
    button_smf->next_press_cb_index = 0;

    execute_button_press_cb(button_smf);
    reschedule_long_press_work(button_smf);
}

static void pressed_run(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;

    uint32_t events = button_smf->button_event.events;
    k_event_clear(&button_smf->button_event, 0xFF);

    if (events & BUTTON_EVENT_RELEASE)
    {
        smf_set_state(SMF_CTX(button_smf), &button_states[BUTTON_STATE_IDLE]);
    }
    else if (events & BUTTON_EVENT_TIMEOUT)
    {
        execute_button_press_cb(button_smf);
        reschedule_long_press_work(button_smf);
    }
    else
    {
        LOG_WRN("Unknown button event for pressed state: %d", button_smf->button_event.events);
    }
}

static void pressed_exit(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;

    k_work_cancel_delayable(&button_smf->button_long_press_work);

    execute_button_release_cb(button_smf);
}

#if IS_ENABLED(CONFIG_BUTTON_SMF_DOUBLE_CLICK)
static void double_click_pend_entry(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;
    k_work_reschedule(&button_smf->button_long_press_work, K_MSEC(CONFIG_BUTTON_SMF_DOUBLE_CLICK_DELAY_MS));
}

static void double_click_pend_run(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;
    bool state;

    uint32_t events = button_smf->button_event.events;
    k_event_clear(&button_smf->button_event, 0xFF);
    if (events & BUTTON_EVENT_PRESS)
    {
        LOG_DBG("double click -> second press detected, running cb");
        button_smf->double_click_cb(button_smf->user_data);
        smf_set_state(SMF_CTX(button_smf), &button_states[BUTTON_STATE_IDLE]);
    }
    if (events & BUTTON_EVENT_RELEASE)
    {
        LOG_DBG("double click -> release detected, waiting for second press/timeout");
    }
    else if (events & BUTTON_EVENT_TIMEOUT)
    {
        smf_set_state(SMF_CTX(button_smf), &button_states[BUTTON_STATE_PRESSED]);
        state = gpio_pin_get_dt(button_smf->config.button_gpio);
        if (!state)
        {
            // if was released and not double clicked, go to press and then handle release
            k_event_set(&button_smf->button_event, BUTTON_EVENT_RELEASE);
            smf_run_state(SMF_CTX(button_smf));
        }
    }
}

static void double_click_pend_exit(void *o)
{
    struct button_smf_data_t *button_smf = (struct button_smf_data_t *)o;
    k_work_cancel_delayable(&button_smf->button_long_press_work);
}
#endif

static void button_long_press_work_cb(struct k_work *work)
{
    int ret;
    struct button_smf_data_t *button_smf = CONTAINER_OF(work, struct button_smf_data_t, button_long_press_work);

    k_mutex_lock(&button_smf->lock, K_FOREVER);

    k_event_set(&button_smf->button_event, BUTTON_EVENT_TIMEOUT);

    ret = smf_run_state(SMF_CTX(button_smf));
    if (ret)
    {
        LOG_ERR("SMF run state error: %d", ret);
    }

    k_mutex_unlock(&button_smf->lock);
}

static void button_debounce_work_cb(struct k_work *work)
{
    int state;
    int ret;
    struct button_smf_data_t *button_smf = CONTAINER_OF(work, struct button_smf_data_t, button_debounce_work);

    k_mutex_lock(&button_smf->lock, K_FOREVER);

    state = gpio_pin_get_dt(button_smf->config.button_gpio);
    LOG_DBG("button %s", state ? "pressed" : "released");
    k_event_set(&button_smf->button_event, state ? BUTTON_EVENT_PRESS : BUTTON_EVENT_RELEASE);

    ret = smf_run_state(SMF_CTX(button_smf));
    if (ret)
    {
        LOG_ERR("SMF run state error: %d", ret);
    }

    k_mutex_unlock(&button_smf->lock);
}

static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // reschedule debounce work for CONFIG_BUTTON_DEBOUNCE_TIME_MS
    struct button_smf_data_t *button_smf = CONTAINER_OF(cb, struct button_smf_data_t, button_cb_data);
    k_work_reschedule(&button_smf->button_debounce_work, K_MSEC(CONFIG_BUTTON_SMF_DEBOUNCE_TIME_MS));
}

static int insert_sorted(struct button_callback_t *button_cb, size_t len, k_timeout_t cb_time, button_callback_cb_t cb)
{
    int i;

    // check if already full
    if (button_cb[len - 1].cb != NULL)
    {
        return -ENOMEM;
    }

    for (i = 0; i < len; i++)
    {
        if (button_cb[i].cb == NULL)
        {
            button_cb[i].cb_time = cb_time;
            button_cb[i].cb = cb;
            return 0;
        }

        if (button_cb[i].cb_time.ticks > cb_time.ticks)
        {
            // shift all callbacks to the right
            for (int j = len - 1; j > i; j--)
            {
                button_cb[j] = button_cb[j - 1];
            }

            button_cb[i].cb_time = cb_time;
            button_cb[i].cb = cb;
            return 0;
        }
    }

    return -ENOMEM;
}

// ============================= API ================================

int button_smf_init(struct button_smf_data_t *button_smf, const struct gpio_dt_spec *button_gpio, void *user_data)
{
    int ret;

    button_smf->user_data = user_data;
    button_smf->config.button_gpio = button_gpio;

    memset(button_smf->button_pressed_cb, 0, sizeof(button_smf->button_pressed_cb));
    memset(button_smf->button_released_cb, 0, sizeof(button_smf->button_released_cb));

    if (!gpio_is_ready_dt(button_gpio))
    {
        LOG_ERR("Button GPIO device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(button_gpio, GPIO_INPUT);
    if (ret < 0)
    {
        LOG_ERR("Button GPIO could not be configured");
        return ret;
    }

    k_event_init(&button_smf->button_event);
    k_mutex_init(&button_smf->lock);
    k_work_init_delayable(&button_smf->button_debounce_work, button_debounce_work_cb);
    k_work_init_delayable(&button_smf->button_long_press_work, button_long_press_work_cb);

    ret = gpio_pin_interrupt_configure_dt(button_gpio, GPIO_INT_EDGE_BOTH);
    if (ret < 0)
    {
        LOG_ERR("Button GPIO interrupt could not be configured");
        return ret;
    }

    gpio_init_callback(&button_smf->button_cb_data, button_isr, BIT(button_gpio->pin));
    ret = gpio_add_callback(button_gpio->port, &button_smf->button_cb_data);
    if (ret < 0)
    {
        LOG_ERR("Button GPIO callback could not be added");
        return ret;
    }

    // TODO: What if already pressed?
    smf_set_initial(SMF_CTX(button_smf), &button_states[BUTTON_STATE_IDLE]);

    return 0;
}

int button_smf_register_callback(struct button_smf_data_t *button_smf,
                                 enum button_smf_event_type event,
                                 button_callback_cb_t cb,
                                 k_timeout_t timeout)
{
    int ret = 0;

    if (!button_smf || !cb)
    {
        return -EINVAL;
    }

    k_mutex_lock(&button_smf->lock, K_FOREVER);

    if (event == BUTTON_SMF_PRESS)
    {
        ret = insert_sorted(button_smf->button_pressed_cb, CONFIG_BUTTON_SMF_MAX_PRESSED_CALLBACKS, timeout, cb);
    }
    else if (event == BUTTON_SMF_RELEASE)
    {
        ret = insert_sorted(button_smf->button_released_cb, CONFIG_BUTTON_SMF_MAX_RELEASE_CALLBACKS, timeout, cb);
    }
    else
    {
        ret = -EINVAL;
    }

    k_mutex_unlock(&button_smf->lock);
    return ret;
}
