#include "fpga_probe.h"
#include "fpga.h"
#include "board.h"

enum { TIMEOUT_CYCLES = 240000000u / 4u, MAX_POLLS = 4000000u };
void fpga_probe_run(uint8_t *samples, fpga_probe_result_t *r) {
    uint32_t t, start, polls = 0;
    uint8_t ready;
    if (!fpga_ready()) { r->status = FPGA_PROBE_NO_FPGA; return; }
    r->ready_before = fpga_capture_ready();
    t = board_cycle_counter_read();
    fpga_capture_latch();
    start = board_cycle_counter_read();
    r->latch_cycles = start - t;
    ready = fpga_capture_ready();
    r->ready_after = ready;
    r->observed_low = !ready;
    while (!ready) {
        if (board_cycle_counter_read() - start >= TIMEOUT_CYCLES || ++polls >= MAX_POLLS) {
            r->status = FPGA_PROBE_TIMEOUT;
            break;
        }
        ready = fpga_capture_ready();
    }
    t = board_cycle_counter_read();
    r->wait_cycles = t - start;
    if (ready) {
        uint8_t ok = fpga_capture_read(samples, FPGA_SCOPE_BUFFER_BYTES);
        r->read_cycles = board_cycle_counter_read() - t;
        r->status = ok ? FPGA_PROBE_OK : FPGA_PROBE_READ_ERROR;
        if (ok) {
            r->ch1_min = r->ch2_min = 255u;
            r->ch1_max = r->ch2_max = 0u;
            for (uint16_t i = 0; i < FPGA_SCOPE_BUFFER_BYTES; i += 2u) {
                uint8_t a = samples[i], b = samples[i+1u];
                if (a < r->ch1_min) r->ch1_min = a;
                if (a > r->ch1_max) r->ch1_max = a;
                if (b < r->ch2_min) r->ch2_min = b;
                if (b > r->ch2_max) r->ch2_max = b;
            }
        }
    }
    fpga_capture_latch(); // Leave normal capture armed when RUN resumes.
}
