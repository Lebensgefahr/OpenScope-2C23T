# Experimental adjustable FPGA trigger (old HW)

Parent: experiment/fpga-hwtrigger-chsel at 0e2b5a225133659fc841803ac499130b4470a45f, physically tested on CH1 and CH2. No modification of that branch. This branch adds the adjustable raw threshold and edge selection. Hardware test of the new variant is pending.

Build:
```sh
python3 tests/fpga_hwtrigger_verify.py
make clean
make RUST_LLD=/usr/bin/ld.lld release-lt-hw4 FPGA_HWTRIGGER=1 VERSION=HWTRIG-LEVEL
```

Mode 0 restores stock logic; mode 1 selects rising/falling from the existing UI; mode 2 detects either edge. HW4 is unchanged. CH1/CH2, level and polarity can now be set from the existing menu. The comparator defines high as ADC >= level, so rising crosses from < to >=; falling from >= to <. This differs by one ADC code from the original software rising comparison. Trigger level uses the raw UI value (normally limited to 12..243). No pretrigger or AUTO timeout yet. As before, only YT capture is the intended hardware test; roll, Bode, calibration and automatic setup remain unverified.

## Serial arm protocol

MCU sends a 10-bit word, MSB first: falling, level[7:0], channel (0=CH1, 1=CH2). PC0 high throughout, PC1 carries data, PC2 pulses high for each bit. No SPI clock/SS transactions during the word. Data is stable before each rising arm; synchronized edge pulse shifts one bit into the new ten-bit register. PC2 low intervals separate the clocks. The last bit uses the original long capture-arm interval, allowing the comparator history to settle. Previous selection latch keeps the final channel. Each rearm rewrites the complete word. Any transient capture state during configuration is reset by the final arm. Timing and generator SPI payloads are unchanged. Short GPIO settling loops are new only inside this experimental arm protocol.

## Physical placement

33 new LUTs/registers beyond the tested parent: two 8-bit unsigned comparator chains, ten shift registers, three synchronizer/edge-history registers plus strobe, combined channel/polarity select, two comparison filter registers. The filter retains the previous comparison when consecutive samples disagree, rejecting a single sampled glitch. This adds latency and may reject extremely short pulses. Comparator propagation delay and CDC have not been verified by STA; simulations use zero-delay LUTs.

ADC CH1 D0..7: pins 48,46,45,44,43,42,41,40. CH2 D0..7: 12,11,6,5,4,3,2,1. These bit positions were reconstructed from stock BRAM/serializer routing; the lower bits have not been individually probed on hardware.
Shift Q0..9: L(6,5) slices 13,12,14,3,7,11,10,6,8,5. Shift mask aaf0, feedback C, strobe D. Q1..8=level, Q9=falling, Q0 redundant channel bit.
Arm synchronizer L(6,5) slices 1,0,2 (aaaa); strobe slice 9 (2222).
CH1 compare LSB->MSB: L(6,7):12,2,8; L(5,7):14,11,15,10; L(6,7):15.
CH2 compare: L(2,5):4,2,14,3; L(1,5):14,2,3; L(2,5):10.
Both LSB mask bbbb, subsequent b2b2 (A=data, B=threshold, C=lower-bit comparison).
L(6,7):3 mask 35ca selects channel and XORs polarity. L(4,7):3 samples; L(4,7):14 mask e8e8 uses sample history + feedback. Filter Q routed to original trigger slices 1 and 4 in L(6,7).

669 config bit flips versus tested parent, same patch for both modes. 750/749 config bit changes versus stock, 442 differing bytes including CRC. CRC d304b77a/22b3b181. Original physical I/O configuration, PLL, BRAM configuration, capture address/stop logic, SPI/generator logic and bootloader unchanged. Routes added via unused RMUX/IMUX/OMUX or existing same-net routes, except four intentionally redirected trigger inputs. Some unused BRAM routing switches are used, not the memory configuration itself.

Verification: exact payload SHA256/CRC, decoded comparator/shift/filter/edge LUTs, all 256x256 comparisons on both channels, all 1024 parameter combinations, selected/unselected channel isolation for both slopes. Full ARM build and physical hardware timing not verified here.

## First physical test
Use mode 1, old hardware, repetitive 1 kHz square wave first. Move level between low and high plateaus, switch RISE/FALL, then move level outside the signal range (NORMAL or SINGLE should wait). Repeat on CH2, with the unselected input quiet. Then repeat the Arduino rare pulse burst. The beginning remains at the left edge because pretrigger is not implemented. If false or missed triggering occurs, return to tested parent and report channel, slope and level. Do not use for normal releases yet.


# Ring capture / pretrigger extension

Parent hardware-tested level branch: 105a70648dafea9926e5232494067685701aa662. New experiment retains programmable threshold, slope, and source. Mode 0 and HW4 remain stock. Build:
```sh
python3 tests/fpga_hwtrigger_verify.py
make clean
make RUST_LLD=/usr/bin/ld.lld release-lt-hw4 FPGA_HWTRIGGER=1 VERSION=HWTRIG-PRETRIGGER
```

Capture runs continuously into all 4096 byte addresses (2048 sample pairs). A prefill latch inhibits triggering until write address bit 11 first becomes high. At the trigger, eight high address bits are frozen. Four two-bit comparators check the opposite half of the ring, ignoring four low byte-address bits. A two-clock match pipeline and odd-address condition request stop. The existing READY state also holds the phase/address stop until arm. This yields approximately 1024 pairs before and 1024 after the trigger, not an exact 50/50 split. The zero-delay model places the event at sample 1022..1030 for the modeled clock ratios. Real propagation delay and the BRAM pipeline may shift this. The FPGA does not manufacture pretrigger samples.

Read counter tile (9,8) retains the increment carry chain and gains synchronous parallel load through the previously unused C inputs (C0=arm, zero during read; C1..11=frozen write address bits 1..11). TileSyncMUX00 keeps the existing reset route and TileSyncMUX01 selects load. PC1=1 during the framed PC2 strobe requests load; PC1=0 requests ordinary reset for other commands. MCU skips two byte frames, then reads the normal byte count with unchanged fast-range alignment. This is intended to return the ring in chronological order; actual sync-load behavior, serializer phase, boundary pair and clock timing need hardware verification. Existing SPI timing and generator payloads unchanged.

UI stores an absolute trigger sample instead of a window start in hardware YT mode; a local +/-32 pair search refines the nominal midpoint if a sampled crossing exists. If decimation misses the crossing, the midpoint is used rather than rejecting READY. MOVE recomputes the display window and preserves its lock in STOP/SINGLE. Multiple edges inside that small search interval may make refinement ambiguous.

570 configuration bit flips relative to the level branch. The reproducible byte XOR delta and LUT sites are in fpga-pretrigger-patch.json. No change to PLL, physical I/O direction, BRAM modes or bootloader. New routing is not STA-validated. The model exercises 12,288 stop positions per trigger mode and all 4096 read pointer alignments; not a hardware simulation. AUTO timeout remains unimplemented in this branch. Full ARM build has not been run here. First test: rare pulse burst at 200 us/div, then fast ranges, check both channels, STOP/SINGLE MOVE and a long stable baseline before the event. Compare the generator output against the tested parent. Reflash the parent if the ring ordering or READY is incorrect.

## Staged autoset cadence experiment

Parent: 744d9b73babd9378f83bd9b30c94bac8ceaa2abc (user confirmed correct frequency after AUTO). This branch intentionally retains the parent's gain, threshold, centering, half-period search and timebase selection algorithms. Only the decision cadence is changed: wait for three distinct captured frames per decision, discarding the first two for settling. This is not median filtering. No FPGA changes and no force capture or trigger AUTO timeout. The new test exercises decision cadence and original half-period behavior; actual analogue settling and physical capture still require hardware verification.
