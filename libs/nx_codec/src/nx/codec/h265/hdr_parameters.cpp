// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hdr_parameters.h"

namespace nx::media::h265 {

void HRDParameters::SubLayerHRDParameters::Data::read(
    nx::utils::BitStreamReader& reader, bool sub_pic_hrd_params_present_flag)
{
    bit_rate_value_minus1 = reader.getGolomb();
    cpb_size_value_minus1 = reader.getGolomb();
    if (sub_pic_hrd_params_present_flag)
    {
        cpb_size_du_value_minus1 = reader.getGolomb();
        bit_rate_du_value_minus1 = reader.getGolomb();
    }
    cbr_flag = reader.getBits(1);
}

bool HRDParameters::read(
    nx::utils::BitStreamReader& reader, bool _commonInfPresentFlag, uint8_t maxNumSubLayersMinus1)
{
    commonInfPresentFlag = _commonInfPresentFlag;
    if (commonInfPresentFlag)
    {
        nal_hrd_parameters_present_flag = reader.getBits(1);
        vcl_hrd_parameters_present_flag = reader.getBits(1);
        if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
        {
            sub_pic_hrd_params_present_flag = reader.getBits(1);
            if (sub_pic_hrd_params_present_flag)
            {
                tick_divisor_minus2 = reader.getBits(8);
                du_cpb_removal_delay_increment_length_minus1 = reader.getBits(5);
                sub_pic_cpb_params_in_pic_timing_sei_flag = reader.getBits(1);
                dpb_output_delay_du_length_minus1 = reader.getBits(5);

            }
            bit_rate_scale = reader.getBits(4);
            cpb_size_scale = reader.getBits(4);
            if (sub_pic_hrd_params_present_flag)
                cpb_size_du_scale = reader.getBits(4);

            initial_cpb_removal_delay_length_minus1 = reader.getBits(5);
            au_cpb_removal_delay_length_minus1 = reader.getBits(5);
            dpb_output_delay_length_minus1 = reader.getBits(5);
        }
        else
        {
            sub_pic_hrd_params_present_flag = 0;
            sub_pic_cpb_params_in_pic_timing_sei_flag = 0;
            initial_cpb_removal_delay_length_minus1 = 23;
            au_cpb_removal_delay_length_minus1 = 23;
            dpb_output_delay_length_minus1 = 23;
        }
    }
    else
    {
        au_cpb_removal_delay_length_minus1 = 0;
        dpb_output_delay_length_minus1 = 0;
    }
    sub_layer_hrd_parameters.resize(maxNumSubLayersMinus1 + 1);
    for(uint8_t i = 0; i <= maxNumSubLayersMinus1; ++i)
    {
        sub_layer_hrd_parameters[i].fixed_pic_rate_within_cvs_flag = 1;
        sub_layer_hrd_parameters[i].fixed_pic_rate_general_flag = reader.getBits(1);
        if (!sub_layer_hrd_parameters[i].fixed_pic_rate_general_flag)
            sub_layer_hrd_parameters[i].fixed_pic_rate_within_cvs_flag = reader.getBits(1);

        sub_layer_hrd_parameters[i].low_delay_hrd_flag = 0;
        if (sub_layer_hrd_parameters[i].fixed_pic_rate_within_cvs_flag)
            sub_layer_hrd_parameters[i].elemental_duration_in_tc_minus1 = reader.getGolomb();
        else
            sub_layer_hrd_parameters[i].low_delay_hrd_flag = reader.getBits(1);

        sub_layer_hrd_parameters[i].cpb_cnt_minus1 = 0;
        if (!sub_layer_hrd_parameters[i].low_delay_hrd_flag)
            sub_layer_hrd_parameters[i].cpb_cnt_minus1 = reader.getGolomb();

        if (sub_layer_hrd_parameters[i].cpb_cnt_minus1 > 31)
            return false;

        if (nal_hrd_parameters_present_flag)
        {
            sub_layer_hrd_parameters[i].nal_data.resize(sub_layer_hrd_parameters[i].cpb_cnt_minus1 + 1);
            for (uint32_t j = 0; j <= sub_layer_hrd_parameters[i].cpb_cnt_minus1; ++j)
                sub_layer_hrd_parameters[i].nal_data[j].read(reader, sub_pic_hrd_params_present_flag);

        }
        if (vcl_hrd_parameters_present_flag)
        {
            sub_layer_hrd_parameters[i].vcl_data.resize(sub_layer_hrd_parameters[i].cpb_cnt_minus1 + 1);
            for (uint32_t j = 0; j <= sub_layer_hrd_parameters[i].cpb_cnt_minus1; ++j)
                sub_layer_hrd_parameters[i].vcl_data[j].read(reader, sub_pic_hrd_params_present_flag);
        }
    }
    return true;
}


} //namespace nx::media::h265
