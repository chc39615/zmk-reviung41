/*
 * RGB battery guard
 *
 * On battery (USB not powered): RGB underglow is forced off and the
 * nice!nano LED power switch (EXT_POWER, P0.13) is cut, whatever the
 * keymap or saved settings say. RGB TOG on battery is reverted at once.
 *
 * On USB: hands-off. RGB TOG works normally. When USB is plugged back
 * in, the RGB on/off choice you had before unplugging is restored.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/ext_power.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/rgb_underglow.h>
#include <zmk/usb.h>

LOG_MODULE_REGISTER(rgb_battery_guard, CONFIG_ZMK_LOG_LEVEL);

static const struct device *ext_power = DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));

/* Treat boot as "was on USB" so booting on battery counts as an unplug. */
static bool was_usb_powered = true;
static bool rgb_wanted_on_usb;

static void guard_apply(struct k_work *work) {
    bool usb = zmk_usb_is_powered();
    bool rgb_on = false;

    zmk_rgb_underglow_get_state(&rgb_on);

    if (usb) {
        if (!was_usb_powered && rgb_wanted_on_usb && !rgb_on) {
            LOG_INF("USB connected: restoring RGB");
            zmk_rgb_underglow_on();
        }
    } else {
        if (was_usb_powered) {
            rgb_wanted_on_usb = rgb_on; /* remember the USB-side choice */
            LOG_INF("On battery: RGB off, LED power cut (was %s)", rgb_on ? "on" : "off");
        }
        if (rgb_on) {
            zmk_rgb_underglow_off(); /* also disables EXT_POWER */
        }
        if (device_is_ready(ext_power) && ext_power_get(ext_power) > 0) {
            ext_power_disable(ext_power);
        }
    }
    was_usb_powered = usb;
}

static K_WORK_DELAYABLE_DEFINE(guard_work, guard_apply);

static int guard_listener(const zmk_event_t *eh) {
    if (as_zmk_usb_conn_state_changed(eh)) {
        k_work_reschedule(&guard_work, K_MSEC(10));
    } else if (!zmk_usb_is_powered()) {
        /* any key on battery: re-check after the keymap has run (catches RGB TOG) */
        k_work_reschedule(&guard_work, K_MSEC(10));
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_battery_guard, guard_listener);
ZMK_SUBSCRIPTION(rgb_battery_guard, zmk_usb_conn_state_changed);
ZMK_SUBSCRIPTION(rgb_battery_guard, zmk_position_state_changed);

static int guard_init(void) {
    /* first check after settings_load() in main() has restored saved RGB/EXT_POWER state */
    k_work_reschedule(&guard_work, K_MSEC(CONFIG_ZMK_RGB_BATTERY_GUARD_BOOT_DELAY_MS));
    return 0;
}

SYS_INIT(guard_init, APPLICATION, 99);
