#include <stdlib.h>
/* Regression coverage with FPGA_HWTRIGGER=1, using the existing UI mocks. */
#define main stock_scope_measurements_main
#include "scope_measurements.c"
#undef main

uint8_t scope_hw_set_trigger(uint8_t channel, uint8_t level, uint8_t falling) {
    (void)channel; (void)level; (void)falling;
    return 0;
}

static void feed_hw_auto(uint8_t lo1, uint8_t hi1, uint8_t lo2, uint8_t hi2) {
    auto_fill_channel(0, 50, lo1, hi1);
    auto_fill_channel(1, 103, lo2, hi2);
    auto_frame();
}

static void test_frame_autoset(void) {
    setup();
    ui.running = 1;
    configured_timebase = ui.scope_timebase;
    configured_range[0] = scope_vdiv_ch[0];
    configured_range[1] = scope_vdiv_ch[1];
    ui.scope_vdiv = scope_safe_vdiv();
    scope_auto_setup_request(3);
    assert(scope_auto_time_active && !configure_count && !arm_count);
    feed_hw_auto(128, 208, 100, 140);
    feed_hw_auto(128, 208, 0, 255); /* one outlier */
    assert(!configure_count && scope_vdiv_ch[1] == 5);
    feed_hw_auto(128, 208, 100, 140);
    assert(scope_vdiv_ch[0] == 5 && scope_vdiv_ch[1] == 4);
    assert(configure_count == 1 && arm_count == 1);
    assert(scope_auto_time_active && ui.scope_timebase == 11);
    for (unsigned i = 0; i < 3; ++i)
        feed_hw_auto(128, 208, 72, 152);
    assert(!scope_auto_time_active && !scope_auto_channel_mask);
    assert(ui.scope_trigger_source == 1 && ui.scope_trigger_level == 168);
    assert(scope_vdiv_ch[0] == 5 && scope_vdiv_ch[1] == 4);
    assert(ui.scope_timebase == 10);
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
    ui.running = 1;
    scope_frame_valid = 0;
    scope_ch_pos[0] = 15;
    ui.scope_trigger_level = 177;
    scope_auto_setup_request(3);
    assert(scope_auto_time_active);
    scope_frame_age_ms = (uint16_t)scope_auto_frame_timeout_ms();
    scope_auto_timeout_service();
    assert(!scope_auto_time_active);
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
