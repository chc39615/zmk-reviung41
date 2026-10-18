/*
 * LED chain diagnostic (replaces ZMK underglow in this test build)
 *
 * Loop:
 *  A) WALK : one LED at a time in DATA-CHAIN order, 1.5 s each, dim (~6%).
 *            LED #1 (first after the MCU) = RED, all others = GREEN.
 *            Between steps all LEDs are OFF for 0.3 s.
 *  B) ALL OFF 2 s  (any light here = an LED latching garbage)
 *  C) SOLID WHITE steps, 4 s each: 2% -> 10% -> 25% -> 50% -> 100%
 *  D) ALL OFF 2 s, then repeat.
 * Every frame is re-sent every 20 ms (like ZMK effects do), so a flaky
 * LED shows up as flicker instead of hiding behind one good frame.
 * Progress is printed on the USB log.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include <drivers/ext_power.h>

LOG_MODULE_REGISTER(ledtest, LOG_LEVEL_INF);

#define STRIP DT_CHOSEN(zmk_underglow)
#define N DT_PROP(STRIP, chain_length)

static const struct device *strip = DEVICE_DT_GET(STRIP);
static const struct device *ext_power = DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));
static struct led_rgb px[N];

static void fill(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < N; i++) {
        px[i].r = r; px[i].g = g; px[i].b = b;
    }
}

/* keep re-sending the same frame for ms milliseconds */
static void hold(int ms) {
    for (int t = 0; t < ms; t += 20) {
        if (device_is_ready(ext_power) && ext_power_get(ext_power) <= 0) {
            ext_power_enable(ext_power); /* settings may have restored "off" */
            k_msleep(50);
        }
        led_strip_update_rgb(strip, px, N);
        k_msleep(20);
    }
}

static void ledtest_thread(void) {
    k_sleep(K_SECONDS(3));
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip not ready");
        return;
    }
    static const uint8_t level_pct[] = {2, 10, 25, 50, 100};

    for (int cycle = 1;; cycle++) {
        LOG_INF("=== cycle %d: WALK (chain order, #1 = RED) ===", cycle);
        for (int i = 0; i < N; i++) {
            fill(0, 0, 0);
            hold(300);
            if (i == 0) {
                px[i].r = 16;
            } else {
                px[i].g = 16;
            }
            LOG_INF("WALK: LED #%d of %d lit", i + 1, N);
            hold(IS_ENABLED(CONFIG_LEDTEST_WALK_ONLY) ? 3000 : 1500);
        }
        fill(0, 0, 0);
        LOG_INF("ALL OFF (nothing should light)");
        hold(2000);
        for (int s = 0; !IS_ENABLED(CONFIG_LEDTEST_WALK_ONLY) && s < ARRAY_SIZE(level_pct); s++) {
            uint8_t v = (uint8_t)((255 * level_pct[s]) / 100);
            fill(v, v, v);
            LOG_INF("SOLID WHITE %d%% (value %d)", level_pct[s], v);
            hold(4000);
        }
        fill(0, 0, 0);
        LOG_INF("ALL OFF");
        hold(2000);
    }
}
K_THREAD_DEFINE(ledtest_tid, 2048, ledtest_thread, NULL, NULL, NULL, 7, 0, 0);
