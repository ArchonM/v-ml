#include <iostream>

#include "myproject.h"
#include "parameters.h"


void myproject(
    input_t x[16],
    result_t layer7_out[1]
) {

    // hls-fpga-machine-learning insert IO
    #pragma HLS ARRAY_RESHAPE variable=x complete dim=0
    #pragma HLS ARRAY_PARTITION variable=layer7_out complete dim=0
    #pragma HLS INTERFACE ap_vld port=x,layer7_out 
    #pragma HLS PIPELINE

    // hls-fpga-machine-learning insert load weights
#ifndef __SYNTHESIS__
    static bool loaded_weights = false;
    if (!loaded_weights) {
        nnet::load_weights_from_txt<model_default_t, 512>(w2, "w2.txt");
        nnet::load_weights_from_txt<model_default_t, 32>(b2, "b2.txt");
        nnet::load_weights_from_txt<model_default_t, 512>(w4, "w4.txt");
        nnet::load_weights_from_txt<model_default_t, 16>(b4, "b4.txt");
        nnet::load_weights_from_txt<model_default_t, 16>(w6, "w6.txt");
        nnet::load_weights_from_txt<model_default_t, 1>(b6, "b6.txt");
        loaded_weights = true;    }
#endif
    // ****************************************
    // NETWORK INSTANTIATION
    // ****************************************

    // hls-fpga-machine-learning insert layers

    layer2_t layer2_out[32];
    #pragma HLS ARRAY_PARTITION variable=layer2_out complete dim=0

    layer3_t layer3_out[32];
    #pragma HLS ARRAY_PARTITION variable=layer3_out complete dim=0

    layer4_t layer4_out[16];
    #pragma HLS ARRAY_PARTITION variable=layer4_out complete dim=0

    layer5_t layer5_out[16];
    #pragma HLS ARRAY_PARTITION variable=layer5_out complete dim=0

    layer6_t layer6_out[1];
    #pragma HLS ARRAY_PARTITION variable=layer6_out complete dim=0

    nnet::dense<input_t, layer2_t, config2>(x, layer2_out, w2, b2); // fc1

    nnet::relu<layer2_t, layer3_t, relu_config3>(layer2_out, layer3_out); // relu1

    nnet::dense<layer3_t, layer4_t, config4>(layer3_out, layer4_out, w4, b4); // fc2

    nnet::relu<layer4_t, layer5_t, relu_config5>(layer4_out, layer5_out); // relu2

    nnet::dense<layer5_t, layer6_t, config6>(layer5_out, layer6_out, w6, b6); // fc3

    nnet::sigmoid<layer6_t, result_t, sigmoid_config7>(layer6_out, layer7_out); // sigmoid

}

