// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <stdint.h>

#include <nx/utils/bit_stream.h>

namespace nx::media::h265 {

struct NX_CODEC_API HRDParameters
{
    bool read(nx::utils::BitStreamReader& reader, bool commonInfPresentFlag, uint8_t maxNumSubLayersMinus1);

    struct SubLayerHRDParameters
    {
        struct Data
        {
            void read(nx::utils::BitStreamReader& reader, bool sub_pic_hrd_params_present_flag);
            uint32_t bit_rate_value_minus1 = 0;
            uint32_t cpb_size_value_minus1 = 0;
            uint32_t cpb_size_du_value_minus1 = 0;
            uint32_t bit_rate_du_value_minus1 = 0;
            bool cbr_flag = false;
        };
        bool fixed_pic_rate_general_flag = false;
        bool fixed_pic_rate_within_cvs_flag = false;
        uint32_t elemental_duration_in_tc_minus1 = 0;
        bool low_delay_hrd_flag = false;
        uint32_t cpb_cnt_minus1 = 0;
        std::vector<Data> nal_data;
        std::vector<Data> vcl_data;
    };

    bool commonInfPresentFlag = false;
    bool nal_hrd_parameters_present_flag = false;
    bool vcl_hrd_parameters_present_flag = false;
    bool sub_pic_hrd_params_present_flag = false;
    uint8_t tick_divisor_minus2 = 0;
    uint8_t du_cpb_removal_delay_increment_length_minus1 = 0;
    bool sub_pic_cpb_params_in_pic_timing_sei_flag = false;
    uint8_t dpb_output_delay_du_length_minus1 = 0;
    uint8_t bit_rate_scale = 0;
    uint8_t cpb_size_scale = 0;
    uint8_t cpb_size_du_scale = 0;
    uint8_t initial_cpb_removal_delay_length_minus1 = 0;
    uint8_t au_cpb_removal_delay_length_minus1 = 0;
    uint8_t dpb_output_delay_length_minus1 = 0;
    std::vector<SubLayerHRDParameters> sub_layer_hrd_parameters;
};

} //namespace nx::media::h265
