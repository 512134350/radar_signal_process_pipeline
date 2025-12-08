#include "radar_defines.h"
void pulse_compression(stream_in_t &adc_input, stream_out_t &pc_output);

void radar_processing_chain(stream_in_t &adc_input, stream_out_t &pc_output) {
    #pragma HLS INTERFACE axis port=adc_input
    #pragma HLS INTERFACE axis port=pc_output
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS DATAFLOW
    pulse_compression(adc_input, pc_output);
}
