`timescale 1ns/1ps
module capture_ring_tb;
  localparam A=4, N=1<<A, P=8;
  reg clk=0, rd_clk=0; always #5 clk=~clk; always #7 rd_clk=~rd_clk;
  reg reset=1, arm=0, ce=0, ch=0, fall=0, rd_en=0;
  reg [7:0] a=0,b=0,level=128; reg [A-1:0] idx=0;
  wire ready; wire [7:0] ra,rb;
  capture_ring #(.ADDR_BITS(A),.PRE(P)) dut(clk,reset,arm,ce,a,b,level,ch,fall,ready,rd_clk,rd_en,idx,ra,rb);
  task sample(input [7:0] x,input [7:0] y); begin
    @(negedge clk); a=x; b=y; ce=1;
    @(posedge clk); #1;
    @(negedge clk); ce=0;
  end endtask
  task rearm(input channel,input falling); begin
    @(negedge clk); ch=channel; fall=falling; arm=1;
    @(negedge clk); arm=0;
    if(ready) $fatal(1,"ready not cleared");
  end endtask
  task check_read(input integer i,input [7:0] ea,input [7:0] eb); begin
    @(negedge rd_clk); idx=i; rd_en=1;
    @(posedge rd_clk); #1;
    if(ra!==ea || rb!==eb) $fatal(1,"read %0d expected %0d/%0d got %0d/%0d",i,ea,eb,ra,rb);
    @(negedge rd_clk); rd_en=0;
  end endtask
  integer i;
  initial begin
    repeat(2) @(negedge clk); reset=0;
    rearm(0,0);
    for(i=0;i<1000;i=i+1) begin sample(10,i%256); if(ready) $fatal(1,"early ready"); end
    sample(200,1000%256);
    for(i=1;i<N-P;i=i+1) begin
      if(ready) $fatal(1,"early post ready");
      sample(200+i,(1000+i)%256);
    end
    if(!ready) $fatal(1,"missing ready");
    sample(42,42);
    for(i=0;i<P;i=i+1) check_read(i,10,(1000-P+i)%256);
    for(i=P;i<N;i=i+1) check_read(i,200+i-P,(1000+i-P)%256);
    rearm(1,1);
    for(i=0;i<20;i=i+1) sample(i%2 ? 0:255,200);
    if(ready) $fatal(1,"wrong source");
    sample(77,100);
    for(i=1;i<N-P;i=i+1) sample(77,100);
    if(!ready) $fatal(1,"falling missing");
    for(i=P;i<N;i=i+1) check_read(i,77,100);
    $display("capture_ring_tb PASS"); $finish;
  end
  initial begin #1000000; $fatal(1,"timeout"); end
endmodule
