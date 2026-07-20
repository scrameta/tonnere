module infopacketstate (
clock      , // clock
reset      , // Active high, syn reset

start_of_frame,
audio_regen_needed,
packet_sent,

audio_regen,
audio_info,
video_info,
packet_needed
);
//-------------Input Ports-----------------------------
input   clock,reset,start_of_frame,audio_regen_needed,packet_sent;
 //-------------Output Ports----------------------------
output  audio_regen,audio_info,video_info,packet_needed;
//-------------Input ports Data Type-------------------
wire    clock,reset,start_of_frame,audio_regen_needed,packet_sent;
//-------------Output Ports Data Type------------------
reg     audio_regen,audio_info,video_info,packet_needed;
//-------------Internal Constants--------------------------
parameter SIZE = 5           ;
parameter CHOOSE  = 2'b00,AUDIO_REGEN = 2'b01,AUDIO_INFO = 2'b10,VIDEO_INFO = 2'b11;
//-------------Internal Variables---------------------------
reg   [SIZE-1:0]          state        ;// Seq part of the FSM
wire  [SIZE-1:0]          next_state   ;// combo part of FSM
//----------Code startes Here------------------------
assign next_state = fsm_function(state,start_of_frame,audio_regen_needed,packet_sent);
//----------Function for Combo Logic-----------------
function [SIZE-1:0] fsm_function;
  input  [SIZE-1:0]  state ;	
  input    start_of_frame ;
  input    audio_regen_needed ;
  input    packet_sent ;
  
  fsm_function[4] = start_of_frame | state[4];     //video_info
  fsm_function[3] = start_of_frame | state[3];     //audio_info
  fsm_function[2] = audio_regen_needed | state[2]; //audio_regen
  fsm_function[1:0] = state[1:0];

  case(state[1:0])
   CHOOSE : if (state[2] == 1'b1) begin
                fsm_function[1:0] = AUDIO_REGEN;;
              end else if (state[3] == 1'b1) begin
                fsm_function[1:0] = AUDIO_INFO;
               end else if (state[4] == 1'b1) begin
                fsm_function[1:0] = VIDEO_INFO;
              end
   AUDIO_REGEN : if (packet_sent == 1'b1) begin
                fsm_function[1:0] = CHOOSE;
		fsm_function[2] = 0;
              end
   AUDIO_INFO : if (packet_sent == 1'b1) begin
                fsm_function[1:0] = CHOOSE;
		fsm_function[3] = 0;
              end
   VIDEO_INFO : if (packet_sent == 1'b1) begin
                fsm_function[1:0] = CHOOSE;
		fsm_function[4] = 0;
              end
   default : fsm_function = CHOOSE;
  endcase
endfunction
//----------Seq Logic-----------------------------
always @ (posedge clock)
begin : FSM_SEQ
  if (reset == 1'b1) begin
    state <=  #1  CHOOSE;
  end else begin
    state <=  #1  next_state;
  end
end
//----------Output Logic-----------------------------
always @ (posedge clock)
begin : OUTPUT_LOGIC
if (reset == 1'b1) begin
  audio_regen <=  #1  1'b0;
  video_info <=   #1  1'b0;
  audio_info <=   #1  1'b0;
  packet_needed <=   #1  1'b0;
end
else begin
  case(state[1:0])
    CHOOSE : begin
                  audio_regen <=  #1  1'b0;
                  video_info <=  #1  1'b0;
                  audio_info <=  #1  1'b0;
                  packet_needed <=  #1  1'b0;
               end
   AUDIO_REGEN : begin
                  audio_regen <=  #1  1'b1;
                  video_info <=  #1  1'b0;
                  audio_info <=  #1  1'b0;
                  packet_needed <=  #1  1'b1;
                end
   AUDIO_INFO : begin
                  audio_regen <=  #1  1'b0;
                  video_info <=  #1  1'b0;
                  audio_info <=  #1  1'b1;
                  packet_needed <=  #1  1'b1;
                end
   VIDEO_INFO : begin
                  audio_regen <=  #1  1'b0;
                  video_info <=  #1  1'b1;
                  audio_info <=  #1  1'b0;
                  packet_needed <=  #1  1'b1;
                end
   default : begin
                  audio_regen <=  #1  1'b0;
                  video_info <=  #1  1'b0;
                  audio_info <=  #1  1'b0;
                  packet_needed <=  #1  1'b0;
                  end
  endcase
end
end // End Of Block OUTPUT_LOGIC

endmodule // End of Module arbiter
