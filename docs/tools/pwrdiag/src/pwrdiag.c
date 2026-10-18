/* Power-supply diagnostics for nRF52840 (ZMK test build) */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <nrf.h>

LOG_MODULE_REGISTER(pwrdiag, LOG_LEVEL_INF);

static uint32_t resetreas_at_boot;

static int pwrdiag_capture(void)
{
    resetreas_at_boot = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = 0xFFFFFFFF; /* clear so the next boot shows only the new cause */
    return 0;
}
SYS_INIT(pwrdiag_capture, PRE_KERNEL_1, 0);

static const char *regout0_str(uint32_t v)
{
    switch (v & 7) {
    case 0: return "1.8V"; case 1: return "2.1V"; case 2: return "2.4V";
    case 3: return "2.7V"; case 4: return "3.0V"; case 5: return "3.3V";
    default: return "default(1.8V)";
    }
}

static void pwrdiag_thread(void)
{
    k_sleep(K_SECONDS(3));
    for (;;) {
        uint32_t rr = resetreas_at_boot;
        LOG_INF("=== PWRDIAG uptime=%llds ===", k_uptime_get() / 1000);
        LOG_INF("MAINREGSTATUS=%u (%s)", NRF_POWER->MAINREGSTATUS,
                NRF_POWER->MAINREGSTATUS ? "HIGH-voltage mode: VDDH supplied, REG0 in use"
                                         : "NORMAL mode: VDD supplied directly, REG0 bypassed");
        LOG_INF("DCDCEN (REG1)=%u  DCDCEN0 (REG0)=%u", NRF_POWER->DCDCEN, NRF_POWER->DCDCEN0);
        LOG_INF("UICR REGOUT0=0x%08x -> VDD %s", NRF_UICR->REGOUT0, regout0_str(NRF_UICR->REGOUT0));
        LOG_INF("USBREGSTATUS=0x%x (bit0 VBUS detected)", NRF_POWER->USBREGSTATUS);
        LOG_INF("RESETREAS at boot=0x%08x%s%s%s%s%s%s", rr,
                rr == 0 ? " [power-on/brown-out]" : "",
                (rr & POWER_RESETREAS_RESETPIN_Msk) ? " [reset pin]" : "",
                (rr & POWER_RESETREAS_DOG_Msk) ? " [watchdog]" : "",
                (rr & POWER_RESETREAS_SREQ_Msk) ? " [soft reset]" : "",
                (rr & POWER_RESETREAS_LOCKUP_Msk) ? " [CPU lockup]" : "",
                (rr & POWER_RESETREAS_OFF_Msk) ? " [wake from System OFF]" : "");
        k_sleep(K_SECONDS(10));
    }
}
K_THREAD_DEFINE(pwrdiag_tid, 1024, pwrdiag_thread, NULL, NULL, NULL, 7, 0, 0);
