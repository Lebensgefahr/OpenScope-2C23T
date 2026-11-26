/* Only the cadence of the hardware autoset differs from the tested
 * pretrigger parent. The original half-period and gain rules remain. */
#define main stock_scope_measurements_main
#include "scope_measurements.c"
#undef main

uint8_t scope_hw_set_trigger(uint8_t channel, uint8_t level, uint8_t falling) {
    (void)channel; (void)level; (void)falling;
    return 0;
}

static void feed(uint16_t period, uint8_t low, uint8_t high) {
    auto_fill_channel(0, period, low, high);
    auto_fill_channel(1, 2, 128, 128);
    auto_frame();
}

static void test_time_decision_waits_for_fresh_frames(void) {
    setup();
    scope_auto_setup_request(1);
    assert(ui.scope_timebase == 0 && scope_auto_time_active);
    assert(configure_count == 1 && arm_count == 1);
    feed(25, 100, 150);
    assert(scope_hw_auto_settle_frames == 1);
    assert(!scope_auto_time_service());
    assert(scope_hw_auto_settle_frames == 1);
    feed(25, 100, 150);
    assert(ui.scope_timebase == 0 && configure_count == 1);
    feed(25, 100, 150);
    assert(!scope_auto_time_active && scope_auto_channel_mask == 1);
    /* Preserve the parent's half-period rule and final slower step. */
    assert(ui.scope_timebase == 1 && configure_count == 2);
    for (unsigned i = 0; i < 5; ++i) {
        feed(25, 100, 150);
        assert(scope_auto_channel_mask == 1);
    }
    feed(25, 100, 150);
    assert(!scope_auto_channel_mask);
}

static void test_range_settles_before_next_step(void) {
    setup();
    scope_auto_setup_request(1);
    feed(25, 0, 255);
    feed(25, 0, 255);
    assert(scope_vdiv_ch[0] == SCOPE_AUTO_VDIV_START);
    feed(25, 0, 255);
    assert(scope_vdiv_ch[0] == SCOPE_AUTO_VDIV_START + 1u);
    assert(scope_auto_time_active && ui.scope_timebase == 0);
    feed(25, 0, 255);
    assert(scope_hw_auto_settle_frames == 1);
    scope_auto_cancel();
    assert(!scope_hw_auto_settle_frames);
}

int main(void) {
    test_time_decision_waits_for_fresh_frames();
    test_range_settles_before_next_step();
    puts("hardware autoset cadence tests passed");
    return 0;
}
