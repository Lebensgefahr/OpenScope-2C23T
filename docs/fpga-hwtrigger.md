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
