# FPGA trigger experiment (older hardware only)

Baseline: fix/stock-clock at de1a70a1. Stock FPGA configuration is unchanged.
The existing DWT cycle counter runs at the stock 240 MHz clock.

## Measure the existing capture protocol

Build with `make BUILD=build/fpga-probe VERSION=fpga-probe FPGA_TRIGGER_LAB=1 all`.
Use only the normal application update procedure for the older hardware.
No bootloader changes.

Select scope, DC coupling, CH1, 20 or 50 us/div, STOP.
Send `FPGAPROBE\n` to the USB CDC port (one command at a time).
Repeat with Arduino STOP (0 V) and TEST (one burst per second).

Reply:
`$FPGA,status,timebase_index,ready_before,ready_after,observed_low,latch_cycles,wait_cycles,read_cycles,ch1_min,ch1_max,ch2_min,ch2_max`

status: 0 OK, 1 no FPGA, 2 timeout, 3 read error, 4 wrong mode/timebase.
Convert cycles to microseconds by dividing by 240. Latch duration includes the
existing 1 ms delay; wait starts at latch return. If observed_low is 0, a fresh
capture has not been established by observing READY alone. If it is 1 and
status is 0, the READY transition was observed. ADC extrema are raw bytes.
The experiment temporarily re-arms capture and overwrites only the capture
scratch buffer, not the displayed trace. It re-arms at the end for RUN.

A READY transition with a flat input demonstrates that the currently used
capture mode does not require a signal edge. It does not prove that the FPGA
cannot support an unused trigger mode.

## FPGA reverse engineering

Stock payload is 46,808 bytes, CRC32 b76f3d95, device ID 00120010.
Rodinia decodes the uncompressed tile data for this family. The package pin
coordinates alone are insufficient to identify board nets or drive pins safely.
No new configuration is flashed until board nets, clock constraints, memory
mapping and the transport protocol are verified. The diagnostic is opt-in
at build time and makes no changes to normal acquisition when not invoked.
