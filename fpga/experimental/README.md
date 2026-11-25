# Experimental FPGA capture core

This is an isolated RTL prototype, not a replacement bitstream.
The existing application continues to use its stock FPGA image.

The ring holds 2048 sample pairs by default. It waits indefinitely for a selected
channel edge after collecting 1024 samples of prehistory, records the triggering
sample and 1023 subsequent samples, then freezes the buffer and raises READY.

Run the supplied testbench with:
```sh
iverilog -g2012 -s capture_ring_tb -o /tmp/capture_tb capture_ring.v capture_ring_tb.v
vvp /tmp/capture_tb
```

The independent algorithm model passed local tests; RTL simulation, synthesis,
timing closure, board pin assignments and SPI/clock/reset integration remain
unverified. Do not generate or flash a bitstream using guessed pin assignments.

Board integration must supply the ADC sample clock/enable, synchronize ARM and
configuration, synchronize READY into the SPI domain, guarantee that start_addr
is stable before reading, and preserve the original generator and transport.
