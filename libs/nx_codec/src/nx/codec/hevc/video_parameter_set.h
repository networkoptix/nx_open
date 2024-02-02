// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <stdint.h>

#include <nx/codec/hevc/hdr_parameters.h>
#include <nx/codec/hevc/profile_tier_level.h>

namespace nx::media::hevc {

struct NX_CODEC_API VideoParameterSet
{
    bool read(const uint8_t* data, int size);

    struct OrderingInfo
    {
        uint32_t vps_max_dec_pic_buffering_minus1;
        uint32_t vps_max_num_reorder_pics;
        uint32_t vps_max_latency_increase_plus1;
    };

    struct HRDData
    {
        int32_t hrd_layer_set_idx;
        bool cprms_present_flag;
        HRDParameters hrd_parameters;
    };

    uint8_t vps_video_parameter_set_id;
    uint8_t vps_reserved_three_2bits;
    uint8_t vps_max_layers_minus1;
    uint8_t vps_max_sub_layers_minus1;
    bool vps_temporal_id_nesting_flag;
    uint16_t vps_reserved_0xffff_16bits;
    ProfileTierLevel profile_tier_level;
    bool vps_sub_layer_ordering_info_present_flag;
    std::vector<OrderingInfo> ordering_info;
    uint8_t vps_max_layer_id;
    uint32_t vps_num_layer_sets_minus1;
    std::vector< std::bitset<64> > layer_id_included_flags; // 64 - it`s max size of vps_max_layer_id (6) bit
    bool vps_timing_info_present_flag;
    /*TODO uint32_t vps_num_units_in_tick;
    uint32_t vps_time_scale;
    bool vps_poc_proportional_to_timing_flag;
    uint32_t vps_num_ticks_poc_diff_one_minus1;
    uint32_t vps_num_hrd_parameters;
    std::vector<HRDData> hrd_data;
    bool vps_extension_flag;*/
};

} // namespace nx::media::hevc
