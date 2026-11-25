#include <stdlib.h>
/* Regression coverage with FPGA_HWTRIGGER=1, using the existing UI mocks. */
#define main stock_scope_measurements_main
#include "scope_measurements.c"
#undef main

uint8_t scope_hw_set_trigger(uint8_t channel, uint8_t level, uint8_t falling) {
    (void)channel; (void)level; (void)falling;
    return 0;
}

static void test_frame_autoset(void) {
    setup();
    auto_fill_channel(0, 100, 128, 208);
    auto_fill_channel(1, 103, 100, 140);
    scope_compute_stats();
    uint32_t freq = scope_estimate_freq_hz_window(0, 2, 2046, 128, 208);
    assert(freq != 0);
    scope_auto_setup_request(3);
    assert(ui.scope_trigger_source == 1 && ui.scope_trigger_level == 168);
    assert(scope_vdiv_ch[0] == 5 && scope_vdiv_ch[1] == 4);
    assert(!scope_auto_time_active && scope_auto_channel_mask == 3);
    assert(configure_count == 1 && arm_count == 1);
    assert(scope_timebase_unit_ns[ui.scope_timebase] >= 250000000u / freq);
    for (uint8_t ch = 0; ch < 2; ++ch) {
        uint8_t mid = ch == 0 ? 168 : 112;
        int16_t center = scope_channel_base_center_for(ch) + scope_ch_pos[ch];
        int16_t y = scope_scaled_sample_y(ch, mid, center,
            scope_visible_grid_height(), 0, 319);
        assert(abs(y - scope_visible_grid_mid_y()) <= 1);
    }
    auto_assert_applied();
}

static void test_no_frame_preserves_configuration(void) {
    setup();
    scope_frame_valid = 0;
    scope_ch_pos[0] = 15;
    ui.scope_trigger_level = 177;
    scope_auto_setup_request(3);
    assert(ui.scope_timebase == 11 && ui.scope_trigger_level == 177);
    assert(scope_vdiv_ch[0] == 5 && scope_vdiv_ch[1] == 5);
    assert(scope_ch_pos[0] == 15 && !scope_auto_channel_mask);
    assert(!configure_count && !arm_count);
}

static void test_widen_translates_trigger(void) {
    setup();
    scope_auto_channel_steps_left[0] = 3;
    scope_min_raw[0] = 30;
    scope_max_raw[0] = 250;
    assert(scope_auto_channel_tune(0));
    assert(scope_vdiv_ch[0] == 6 && ui.scope_trigger_level == 134);
}

int main(void) {
    test_frame_autoset();
    test_no_frame_preserves_configuration();
    test_widen_translates_trigger();
    puts("hardware AUTO setup tests passed");
    return 0;
}
