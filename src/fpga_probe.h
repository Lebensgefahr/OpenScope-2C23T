#pragma once
#include <stdint.h>

enum { FPGA_PROBE_OK, FPGA_PROBE_NO_FPGA, FPGA_PROBE_TIMEOUT,
       FPGA_PROBE_READ_ERROR, FPGA_PROBE_WRONG_STATE };
typedef struct {
    uint32_t status, timebase, ready_before, ready_after, observed_low;
    uint32_t latch_cycles, wait_cycles, read_cycles;
    uint32_t ch1_min, ch1_max, ch2_min, ch2_max;
} fpga_probe_result_t;
// Call only in scope STOP, after normal channel/timebase setup.
// Reuses caller's 4096-byte capture buffer; leaves displayed trace intact.
void fpga_probe_run(uint8_t *samples, fpga_probe_result_t *r);
void ui_scope_fpga_probe(fpga_probe_result_t *r);
