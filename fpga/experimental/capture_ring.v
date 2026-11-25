// Experimental capture core, NOT a board-level FPGA image.
// arm/config inputs are synchronous to sample_clk. The board wrapper must
// synchronize ready to rd_clk and wait for stable start_addr before reading.
module capture_ring #(
    parameter integer ADDR_BITS = 11,
    parameter integer PRE = 1024
)(
    input wire sample_clk, reset, arm, sample_ce,
    input wire [7:0] adc_ch1, adc_ch2,
    input wire [7:0] trigger_level,
    input wire trigger_channel, trigger_falling,
    output reg ready,
    input wire rd_clk, rd_en,
    input wire [ADDR_BITS-1:0] rd_index,
    output reg [7:0] rd_ch1, rd_ch2
);
    localparam integer DEPTH = 1 << ADDR_BITS;
    localparam integer POST = DEPTH - PRE - 1;
    localparam [1:0] IDLE=0, WAIT_EDGE=1, POST_EDGE=2, DONE=3;
    localparam [ADDR_BITS-1:0] PRE_ADDR=PRE;
    localparam [ADDR_BITS:0] PRE_COUNT=PRE, POST_COUNT=POST;
    reg [7:0] mem1[0:DEPTH-1], mem2[0:DEPTH-1];
    reg [1:0] state;
    reg [ADDR_BITS-1:0] wr_addr, start_addr;
    reg [ADDR_BITS:0] history_count, post_left;
    reg [7:0] threshold, prev;
    reg channel, falling, prev_valid;
    wire [7:0] cur = channel ? adc_ch2 : adc_ch1;
    wire crossed = prev_valid && (falling ? (prev >= threshold && cur < threshold)
                                          : (prev <= threshold && cur > threshold));
    wire writing = sample_ce && !reset && !arm && (state==WAIT_EDGE || state==POST_EDGE);
    wire [ADDR_BITS-1:0] phys_rd = start_addr + rd_index;
    always @(posedge sample_clk) begin
        if (writing) begin
            mem1[wr_addr] <= adc_ch1;
            mem2[wr_addr] <= adc_ch2;
        end
    end
    always @(posedge sample_clk) begin
        if (reset) begin
            state <= IDLE; ready <= 0; wr_addr <= 0; start_addr <= 0;
            history_count <= 0; post_left <= 0; prev_valid <= 0;
            prev <= 0; threshold <= 0; channel <= 0; falling <= 0;
        end else if (arm) begin
            state <= WAIT_EDGE; ready <= 0; wr_addr <= 0; start_addr <= 0;
            history_count <= 0; post_left <= 0; prev_valid <= 0;
            threshold <= trigger_level; channel <= trigger_channel; falling <= trigger_falling;
        end else if (writing) begin
            wr_addr <= wr_addr + 1'b1;
            prev <= cur; prev_valid <= 1;
            case (state)
                WAIT_EDGE: begin
                    if (history_count < PRE_COUNT) history_count <= history_count + 1'b1;
                    else if (crossed) begin
                        start_addr <= wr_addr - PRE_ADDR;
                        post_left <= POST_COUNT;
                        if (POST == 0) begin state <= DONE; ready <= 1; end
                        else state <= POST_EDGE;
                    end
                end
                POST_EDGE: begin
                    if (post_left == 1) begin
                        post_left <= 0; state <= DONE; ready <= 1;
                    end else post_left <= post_left - 1'b1;
                end
                default: ;
            endcase
        end
    end
    always @(posedge rd_clk) begin
        if (rd_en) begin
            rd_ch1 <= mem1[phys_rd];
            rd_ch2 <= mem2[phys_rd];
        end
    end
endmodule
