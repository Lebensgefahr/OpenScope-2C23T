#include "board.h"
#include "app_config.h"
#include "dmm.h"
#include "display.h"
#include "fw_update.h"
#include "settings.h"
#include "siggen.h"
#include "ui.h"
#include "usb_msc.h"

#include <stdint.h>

static uint16_t diode_beep_hold_ms;
static uint16_t live_beep_hold_ms;
static uint16_t live_beep_phase_ms;
static uint16_t ui_beep_ms;
static uint8_t diode_beep_ready;
static uint8_t diode_beep_was_enabled;

enum {
    DIODE_BEEP_HOLD_MS = 30,
    KEY_BEEP_MS = 28,
    SETTINGS_BEEP_PREVIEW_MS = 90,
    LIVE_BEEP_HOLD_MS = 450,
    LIVE_BEEP_PERIOD_MS = 180,
    LIVE_BEEP_ON_MS = 45,
    LIVE_BEEP_VOLUME_PERCENT = 70,
    STARTUP_BEEP_MS = 55,
    SHUTDOWN_BEEP_MS = 70,
    STARTUP_INPUT_SETTLE_MS = 200,
};

static void shutdown_now(uint8_t beep) {
    board_dmm_beep_irq_arm(0);
    ui_note_runtime_settings();
    siggen_shutdown();
    if (beep) {
        board_buzzer_set(1);
        delay_ms(SHUTDOWN_BEEP_MS);
    }
    board_buzzer_set(0);
    settings_flush();
    board_power_off();
}

static void dmm_beep_service(uint8_t elapsed_ms) {
    uint8_t dmm_beep_seen;
    uint8_t buzzer_on = 0;
    uint8_t force_full = 0;
    uint8_t fixed_volume = 0;
    uint8_t diode_mode = ui_diode_beep_enabled();
    uint8_t live_mode = ui_live_beep_enabled();

    if (diode_mode && !diode_beep_was_enabled) {
        diode_beep_ready = 0;
        diode_beep_hold_ms = 0;
        board_dmm_beep_irq_arm(0);
        (void)board_dmm_beep_edge_seen();
    } else if (!diode_mode) {
        diode_beep_ready = 0;
    }
    diode_beep_was_enabled = diode_mode;

    if (ui_beep_ms && !diode_mode && !live_mode) {
        board_dmm_beep_irq_arm(0);
        if (elapsed_ms >= ui_beep_ms) {
            ui_beep_ms = 0;
            board_buzzer_set(0);
        } else {
            ui_beep_ms = (uint16_t)(ui_beep_ms - elapsed_ms);
            board_buzzer_set(1);
        }
        return;
    }

    if (diode_mode) {
        board_dmm_beep_irq_force_full(1);
        if (!diode_beep_ready && dmm_reading_is_real()) {
            diode_beep_ready = 1;
            (void)board_dmm_beep_edge_seen();
        }
        if (diode_beep_ready) {
#if HW_TARGET_HW40
            board_dmm_beep_irq_arm(0);
            dmm_beep_seen = dmm_diode_continuity_active();
#else
            board_dmm_beep_irq_arm(1);
            dmm_beep_seen = (uint8_t)(board_dmm_beep_active() || board_dmm_beep_edge_seen());
#endif
        } else {
            board_dmm_beep_irq_arm(0);
            (void)board_dmm_beep_edge_seen();
            dmm_beep_seen = 0;
        }
        if (dmm_beep_seen) {
            diode_beep_hold_ms = DIODE_BEEP_HOLD_MS;
        } else if (diode_beep_hold_ms > elapsed_ms) {
            diode_beep_hold_ms = (uint16_t)(diode_beep_hold_ms - elapsed_ms);
        } else {
            diode_beep_hold_ms = 0;
        }
        live_beep_hold_ms = 0;
        live_beep_phase_ms = 0;
        buzzer_on = diode_beep_hold_ms ? 1u : 0u;
        force_full = 1;
    } else if (live_mode) {
        board_dmm_beep_irq_force_full(0);
        board_dmm_beep_irq_arm(0);
        (void)board_dmm_beep_edge_seen();
        dmm_beep_seen = dmm_live_wire_active();
        if (dmm_beep_seen) {
            if (!live_beep_hold_ms) {
                live_beep_phase_ms = 0;
            }
            live_beep_hold_ms = LIVE_BEEP_HOLD_MS;
        } else if (live_beep_hold_ms > elapsed_ms) {
            live_beep_hold_ms = (uint16_t)(live_beep_hold_ms - elapsed_ms);
        } else {
            live_beep_hold_ms = 0;
        }

        if (live_beep_hold_ms) {
            live_beep_phase_ms = (uint16_t)(live_beep_phase_ms + elapsed_ms);
            if (live_beep_phase_ms >= LIVE_BEEP_PERIOD_MS) {
                live_beep_phase_ms = (uint16_t)(live_beep_phase_ms % LIVE_BEEP_PERIOD_MS);
            }
        } else {
            live_beep_phase_ms = 0;
        }
        diode_beep_hold_ms = 0;
        buzzer_on = (live_beep_hold_ms && live_beep_phase_ms < LIVE_BEEP_ON_MS) ? 1u : 0u;
        fixed_volume = LIVE_BEEP_VOLUME_PERCENT;
    } else {
        board_dmm_beep_irq_arm(0);
        diode_beep_hold_ms = 0;
        live_beep_hold_ms = 0;
        live_beep_phase_ms = 0;
        (void)board_dmm_beep_edge_seen();
    }

    ui_set_live_wire_detected((uint8_t)(live_mode && live_beep_hold_ms));

    if (ui_beep_ms && !buzzer_on) {
        if (elapsed_ms >= ui_beep_ms) {
            ui_beep_ms = 0;
            board_buzzer_set(0);
        } else {
            ui_beep_ms = (uint16_t)(ui_beep_ms - elapsed_ms);
            board_buzzer_set(1);
        }
    } else if (force_full) {
        if (buzzer_on) {
            ui_beep_ms = 0;
        }
        board_buzzer_set_full(buzzer_on);
    } else if (fixed_volume) {
        if (buzzer_on) {
            ui_beep_ms = 0;
        }
        board_buzzer_set_percent(buzzer_on, fixed_volume);
    } else if (ui_beep_ms) {
        if (elapsed_ms >= ui_beep_ms) {
            ui_beep_ms = 0;
            board_buzzer_set(0);
        } else {
            ui_beep_ms = (uint16_t)(ui_beep_ms - elapsed_ms);
            board_buzzer_set(1);
        }
    } else {
        board_buzzer_set(buzzer_on);
    }
}

#define SCHED_CYCLES_PER_MS 144000u

/* Main-loop profile: CPU cycles per complete loop. */
static volatile uint32_t main_prof_sum[8];
static uint32_t main_prof_n;
static void main_prof_report(void) {
    static uint32_t report_at;
    uint32_t now = board_cycle_counter_read();
    if (!main_prof_n) report_at = now;
    ++main_prof_n;
    if ((uint32_t)(now - report_at) < 500u * SCHED_CYCLES_PER_MS) return;
    if (usb_cdc_is_configured()) {
        char out[128];
        char rev[10];
        uint16_t pos = 0;
        out[pos++] = '$'; out[pos++] = 'M'; out[pos++] = 'A';
        out[pos++] = 'I'; out[pos++] = 'N';
        for (unsigned i = 0; i < 8u; ++i) {
            uint32_t v = main_prof_sum[i] / main_prof_n;
            unsigned n = 0;
            out[pos++] = ',';
            do { rev[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
            while (n) out[pos++] = rev[--n];
        }
        out[pos++] = '\r'; out[pos++] = '\n';
        usb_cdc_send_bytes((const uint8_t *)out, pos);
    }
    for (unsigned i = 0; i < 8u; ++i) main_prof_sum[i] = 0;
    main_prof_n = 0;
}
int main(void) {
    uint16_t input_settle_ms = STARTUP_INPUT_SETTLE_MS;

    board_init();
    battery_init();
    input_init();
    dmm_init();
    board_buzzer_init();

    lcd_init();
    usb_msc_init();
    ui_init();
    lcd_display_on();
    delay_ms(50);
    board_backlight_set(1);
    load_counter_init();
    ui_beep_ms = STARTUP_BEEP_MS;

    /* Cooperative scheduler, using the free-running DWT cycle counter. */
    uint32_t last_tick = board_cycle_counter_read();
    uint32_t last_end = last_tick;
    uint32_t key_ms = 20u;
    uint32_t dmm_ms = 0u;
    uint32_t ui_pending_ms = 0u;
    while (1) {
        uint32_t mp_start = board_cycle_counter_read();
        uint32_t elapsed_ms = (mp_start - last_tick) / SCHED_CYCLES_PER_MS;
        if (!elapsed_ms) continue;
        last_tick += elapsed_ms * SCHED_CYCLES_PER_MS;
        uint32_t idle_ticks = mp_start - last_end;
        uint32_t mp_t = mp_start;

        /* Advance real-time timers without replaying old input samples. */
        key_ms += elapsed_ms;
        dmm_ms += elapsed_ms;
        ui_pending_ms += elapsed_ms;
        if (input_settle_ms > elapsed_ms) {
            input_settle_ms = (uint16_t)(input_settle_ms - elapsed_ms);
        } else {
            input_settle_ms = 0;
        }
        if (dmm_ms >= 20u) {
            dmm_tick(dmm_ms);
            dmm_ms = 0;
        }
        if (dmm_poll()) {
            dmm_beep_service(0);
            ui_dmm_measurement_updated();
            ui_dmm_send_usb_telemetry();
        }
        main_prof_sum[0] += board_cycle_counter_read() - mp_t;

        mp_t = board_cycle_counter_read();
        usb_msc_poll();
        main_prof_sum[3] += board_cycle_counter_read() - mp_t;

        mp_t = board_cycle_counter_read();
        dmm_beep_service((uint8_t)(elapsed_ms > 255u ? 255u : elapsed_ms));
        main_prof_sum[5] += board_cycle_counter_read() - mp_t;

        mp_t = board_cycle_counter_read();
        uint32_t events = 0;
        if (key_ms >= 20u) {
            key_ms %= 20u;
            events = input_pressed_events();
        }
        if (events && !input_settle_ms) {
            if (events & KEY_POWER) shutdown_now(1);
            ui_handle_keys(events);
            if (events & KEY_REPEAT) {
                ui_beep_ms = 0;
            } else if (ui_consume_beep_preview()) {
                ui_beep_ms = SETTINGS_BEEP_PREVIEW_MS;
            } else {
                ui_beep_ms = KEY_BEEP_MS;
            }
            dmm_beep_service(0);
        } else {
            ui_tick(ui_pending_ms);
            ui_pending_ms = 0;
        }
        main_prof_sum[1] += board_cycle_counter_read() - mp_t;

        mp_t = board_cycle_counter_read();
        if (ui_auto_sleep_due()) shutdown_now(0);
        fw_update_service();
        main_prof_sum[2] += board_cycle_counter_read() - mp_t;

        mp_t = board_cycle_counter_read();
        ui_set_load_sample(mp_t - mp_start, idle_ticks);
        main_prof_sum[6] += board_cycle_counter_read() - mp_t;
        /* Column 4 remains zero: no artificial blocking delay. */
        main_prof_sum[7] += board_cycle_counter_read() - mp_start;
        main_prof_report();
        last_end = board_cycle_counter_read();
    }
}
