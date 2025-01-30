/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aw9364.h"

#include <lvgl.h>
#include <lvgl_input_device.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);
// static struct gpio_dt_spec stuff_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(stuffenable), gpios);

const struct device *bl_dev = DEVICE_DT_GET(DT_NODELABEL(backlight));

static uint32_t count = 0;

static void callback_delta(lv_event_t *e) {
    ARG_UNUSED(e);

    int32_t delta = (int) lv_event_get_user_data(e);

    count = count + delta;
}

void input_cb(struct input_event *evt, void *user_data) {
    LOG_INF("Input callback");

    if (evt->code == INPUT_KEY_UP) {
        count = count + 1;
    } else if (evt->code == INPUT_KEY_DOWN) {
        count -= 1;
    }

    return;
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(buttons)), input_cb, 0);

int main(void) {
    char count_str[11] = {0};
    const struct device *display_dev;
    lv_obj_t *hello_world_label;
    lv_obj_t *hello_world_label2;
    lv_obj_t *count_label;

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Device not ready, aborting test");
        return 0;
    }
    printk("device ready\n");
    k_msleep(10);

    // int gerr = gpio_pin_set_dt(&stuff_gpio, 1);
    // if (gerr != 0) {
    // printk("couldnt set stuff gpuio\n");
    // }
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(255, 255, 255), LV_PART_MAIN);

    if (IS_ENABLED(CONFIG_LV_Z_POINTER_INPUT)) {
        lv_obj_t *hello_world_button;

        hello_world_button = lv_button_create(lv_screen_active());
        lv_obj_align(hello_world_button, LV_ALIGN_TOP_RIGHT, 0, 10);
        lv_obj_add_event_cb(hello_world_button, callback_delta, LV_EVENT_CLICKED, (void *) 1);
        hello_world_label = lv_label_create(hello_world_button);

        lv_obj_t *hello_world_button2;

        hello_world_button2 = lv_button_create(lv_screen_active());
        lv_obj_align(hello_world_button2, LV_ALIGN_TOP_LEFT, 0, 10);
        lv_obj_add_event_cb(hello_world_button2, callback_delta, LV_EVENT_CLICKED, (void *) -1);
        hello_world_label2 = lv_label_create(hello_world_button2);

    } else {
        hello_world_label = lv_label_create(lv_screen_active());
    }

    lv_label_set_text(hello_world_label, "+1");
    lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);

    lv_label_set_text(hello_world_label2, "-1");
    lv_obj_align(hello_world_label2, LV_ALIGN_CENTER, 0, 0);

    count_label = lv_label_create(lv_screen_active());
    lv_obj_align(count_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_timer_handler();

    lv_obj_t *ta = lv_textarea_create(lv_screen_active());
    lv_textarea_set_one_line(ta, true);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 20);

    display_blanking_off(display_dev);

    int last_count = ~0;
    while (1) {
        if (count != last_count) {
            sprintf(count_str, "%d", count);
            lv_label_set_text(count_label, count_str);
            last_count = count;
        }
        lv_timer_handler();
        k_sleep(K_MSEC(10));
    }
}
