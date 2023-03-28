// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_parameter_set.h"

namespace nx::media::hevc {

bool VideoParameterSet::read(const uint8_t* data, int size)
{
    try
    {
        nx::utils::BitStreamReader reader(data, size);
        vps_video_parameter_set_id = reader.getBits(4);
        vps_reserved_three_2bits = reader.getBits(2);
        vps_max_layers_minus1 = reader.getBits(6);
        vps_max_sub_layers_minus1 = reader.getBits(3) ;
        vps_temporal_id_nesting_flag = reader.getBits(1);
        vps_reserved_0xffff_16bits = reader.getBits(16);
        profile_tier_level.read(reader, vps_max_sub_layers_minus1);
        vps_sub_layer_ordering_info_present_flag = reader.getBits(1);
        if (vps_max_sub_layers_minus1 > 6)
            return false;

        ordering_info.resize(vps_max_sub_layers_minus1 + 1);
        for(uint32_t i = ( vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1); i <= vps_max_sub_layers_minus1; ++i )
        {
            ordering_info[i].vps_max_dec_pic_buffering_minus1 = reader.getGolomb();
            ordering_info[i].vps_max_num_reorder_pics = reader.getGolomb();
            ordering_info[i].vps_max_latency_increase_plus1 = reader.getGolomb();
        }
        vps_max_layer_id = reader.getBits(6);
        vps_num_layer_sets_minus1 = reader.getGolomb();
        if (vps_num_layer_sets_minus1 > 1023)
            return false;

        layer_id_included_flags.resize(vps_num_layer_sets_minus1 + 1);
        for(uint32_t i = 1; i <= vps_num_layer_sets_minus1; i++)
        {
            for(uint32_t j = 0; j <= vps_max_layer_id; j++ )
                layer_id_included_flags[i][j] = reader.getBits(1);
        }
        vps_timing_info_present_flag = reader.getBits(1);
        /*if (vps_timing_info_present_flag)
        {
            vps_num_units_in_tick = reader.getBits(32);
            vps_time_scale = reader.getBits(32);
            vps_poc_proportional_to_timing_flag = reader.getBits(1);
            if (vps_poc_proportional_to_timing_flag)
                vps_num_ticks_poc_diff_one_minus1 = reader.getGolomb();

            vps_num_hrd_parameters = reader.getGolomb();
            if (vps_num_hrd_parameters > 1024)
                return false;

            hrd_data.resize(vps_num_hrd_parameters);
            for(uint32_t i = 0; i < vps_num_hrd_parameters; i++ )
            {
                hrd_data[i].hrd_layer_set_idx = reader.getGolomb();
                if (i > 0)
                    hrd_data[i].cprms_present_flag = reader.getBits(1);
                else
                    hrd_data[i].cprms_present_flag = 1;

                if (!hrd_data[i].hrd_parameters.read(reader, hrd_data[i].cprms_present_flag, vps_max_sub_layers_minus1))
                    return false;
            }
        }
        vps_extension_flag = reader.getBits(1);*/
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}

} // namespace nx::media::hevc
