# Experimental two-channel hardware trigger (old HW)

Parent: experiment/fpga-hwtrigger at ddb6ba7c50a0541ee4a0e785c11b82dca20579ab, tested by the device owner on CH1 (rare pulse bursts at 10 s intervals). This branch preserves that commit and adds a selectable channel, not a complete trigger redesign.

Build (use separate build directories or make clean between variants):
```sh
python3 tests/fpga_hwtrigger_verify.py
make clean
make RUST_LLD=/usr/bin/ld.lld release-lt-hw4 FPGA_HWTRIGGER=1 VERSION=HWTRIG-CHSEL
```

0 = stock (unchanged original FPGA and MCU trigger logic); 1 = rising; 2 = anyedge. HW4 remains unchanged. Channel is now selected using the existing trigger-source menu. Threshold still fixed at raw 128, slope fixed by build mode. No pretrigger or AUTO timeout. YT displays the start of the capture as before; other modes remain unverified.

During capture arm PC1 carries the channel (0 CH1, 1 CH2), with PC0 and PC2 high. No SPI clocks are generated during this interval. The new FPGA register samples PC1 only while arm is high, then holds its value. MCU restores PC1 low after the strobe. Changing the menu source during running discards/rearms the pending old-source capture. The PC1=1 arm protocol itself needs hardware validation (it overlaps the existing generator mode selection, but has no SPI writes).

New FPGA logic on the same tile as the tested trigger:
- LogicTILE(6,7) slice00: mask 0xaaf0, C=feedback, A=PC1/PIN26, D=arm; Q holds channel until next arm.
- LogicTILE(6,7) slice06: mask 0xcaca, A=CH1/PIN40, B=CH2/PIN1, C=selector Q.
- Existing IMUX04/16 now receive the mux output, no other original trigger state/stop logic changes.

Routes (all new routing uses previously unused RMUX/IMUX or shares existing selected routes):
- PIN26 -> IOTILE(9,0) InputMUX06 I0 -> L(9,4) RMUX49 I16 -> L(7,4) RMUX07 I6 -> L(7,3) RMUX25 I17 -> L(7,7) RMUX00 I16 -> L(6,7) IMUX00 I9.
- arm -> existing L(6,7) RMUX65 -> IMUX03 I21.
- PIN40 -> existing L(6,7) RMUX16 -> IMUX24 I13.
- PIN1 -> IOTILE(2,9) InputMUX00 I0 -> L(2,7) RMUX01 I18 -> L(6,7) RMUX17 I12 -> IMUX25 I13.
- selector Q -> L(6,7) OMUX01 I1 -> IMUX26 I0.
- mux LUT output -> L(6,7) OMUX19 I0 -> IMUX04/16 I4.

40 configuration bit flips relative to each parent trigger payload. Versus stock: rising 89, anyedge 88. 51 differing bytes including CRC. Rising CRC af04b5c1; anyedge CRC 5eb3b33a. See verification script for SHA256. No modifications to physical IO direction, PLL, BRAM or generator routing. No hardware timing analysis/STA.

Test on old hardware: first verify CH1 as before, then connect Arduino only to CH2, keep CH1 grounded and select trigger CH2. Test rare bursts and SINGLE. Swap channels and verify unselected input does not trigger. Use a signal crossing the ADC midpoint. If anything fails, reflash the previously tested parent branch. Full MCU build and new hardware behavior not yet verified.
