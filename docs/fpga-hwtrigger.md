# Experimental hardware trigger (AG1KLPQ48, old hardware only)

Based on fix/stock-clock at de1a70a1b49b3f65c54157a391e71cb6012769af. No USB probe changes, bootloader changes, delay changes, or changes to HW4 FPGA payload.

Build each mode in its own directory (Make does not track compiler flag changes):

```sh
make BUILD=build/hwtrigger-rising FPGA_HWTRIGGER=1
make BUILD=build/hwtrigger-anyedge FPGA_HWTRIGGER=2
make BUILD=build/hwtrigger-stock FPGA_HWTRIGGER=0
python3 tests/fpga_hwtrigger_verify.py
```

Default mode is 1 (rising). 0 restores original old-HW bitstream and original UI trigger logic. HW4 always uses its original bitstream and UI logic. Do not use this experiment as a normal release.

The hardware trigger detects the raw MSB crossing of ADC A (FPGA pin 40, threshold 128). ADC A's physical CH1/CH2 identity and analog polarity are unconfirmed. Mode 1 detects raw 0->1, mode 2 either edge. The menu source/level/slope do not configure the new logic. No pretrigger and no AUTO timeout; flat input may never produce READY. The YT UI treats READY as a trigger and displays the beginning of the buffer, not a centered trigger. Calibration, roll, Bode and other modes are unverified.

Capture modifications: use free slices (6,7):01 and :04 to register the ADC MSB and latch its edge; reset the event on arm. Replace stop LUT (6,6):07 with 0x4545, A=arm, B=state_enable, C=triggered. It evaluates !arm && (state_enable || !triggered). Before the event, phase/address held at zero; after event, original full-buffer capture proceeds. This is not the separate ring-buffer/pretrigger reference design. No PLL/BRAM/SPI/generator configuration edits. CRC updated.

Routes:
- PIN40 -> IOTILE(6,9) InputMUX00 -> L(6,7) RMUX16 I18 -> IMUX04/16 I13.
- L(6,7) slice01 Q -> OMUX04 I1 -> IMUX17 I0.
- L(6,6) slice13 LUT -> OMUX41 I0 -> RMUX85 I1 -> L(6,7) RMUX65 I13 -> IMUX19 I21.
- L(6,7) slice04 Q -> OMUX12 I1 -> L(7,7) RMUX44 I0 -> L(7,6) RMUX84 I17 -> L(6,6) IMUX30 I10.
- L(6,6) slice01 LUT -> OMUX04 I0 -> IMUX29 I0.

Stock SHA256 7c9ad508c29df5f4df77fd7759fc7aa58eaebcc927843b294a5ac4dacce8edd5.
Rising SHA256 50ce417c1e68592a8dd9ededdbec9de3a7724e4d8ce2f53e8b28523cec3df723.
AnyEdge SHA256 4c3a70916aceb67e6a75be36bd995b9a39747e2de2051d3a4afc4de171ac2379.
Rising: 53 configuration bits; AnyEdge: 52; both 33 bytes including CRC.

First hardware check: use old HW, YT, AUTO, both channels connected to a repetitive square wave centered around the ADC midpoint. Confirm configuration succeeds and capture appears. Then test the rare pulse burst at 1 s repetition. Compare with mode 0. If the screen is blank, try the other channel/offset and AnyEdge. Restore stock mode if needed. No hardware or STA validation yet; software verification does not guarantee physical timing.
