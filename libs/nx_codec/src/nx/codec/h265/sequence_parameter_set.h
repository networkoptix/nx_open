// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <vector>

#include <nx/codec/h265/profile_tier_level.h>
#include <nx/codec/h265/scaling_list_data.h>
#include <nx/codec/h265/short_term_reference_picture_set.h>
#include <nx/codec/h265/video_usability_information_parameters.h>

namespace nx::media::h265 {

struct LongTermRefPicSet
{
    // TODO: #dmishin implement
};

struct NX_CODEC_API SequenceParameterSet
{
public:
    bool read(const uint8_t* data, int size);

public:
    struct SubLayerOrderingInfo
    {
        uint32_t sps_max_dec_pic_buffering_minus1 = 0;
        uint32_t sps_max_num_reorder_pics = 0;
        uint32_t sps_max_latency_increase_plus1 = 0;
    };

    struct LongTermRefPicInfo
    {
        uint32_t lt_ref_pic_poc_lsb_sps = 0;
        uint8_t used_by_curr_pic_lt_sps_flag = 0;
    };

    uint8_t sps_video_parameter_set_id = 0;
    uint8_t sps_max_sub_layers_minus1 = 0;
    bool sps_temporal_id_nesting_flag = false;
    ProfileTierLevel profile_tier_level;
    uint32_t sps_seq_parameter_set_id = 0;
    uint32_t chroma_format_idc = 0;
    bool separate_colour_plane_flag = false;
    uint32_t pic_width_in_luma_samples = 0;
    uint32_t pic_height_in_luma_samples = 0;
    bool conformance_window_flag = false;
    uint32_t conf_win_left_offset = 0;
    uint32_t conf_win_right_offset = 0;
    uint32_t conf_win_top_offset = 0;
    uint32_t conf_win_bottom_offset = 0;
    uint32_t bit_depth_luma_minus8 = 0;
    uint32_t bit_depth_chroma_minus8 = 0;
    uint8_t log2_max_pic_order_cnt_lsb_minus4 = 0;
    uint8_t sps_sub_layer_ordering_info_present_flag = 0;
    std::vector<SubLayerOrderingInfo> sps_sub_layer_ordering_info_vector;

    uint8_t log2_min_luma_coding_block_size_minus3 = 0;
    uint8_t log2_diff_max_min_luma_coding_block_size = 0;
    uint8_t log2_min_transform_block_size_minus2 = 0;
    uint8_t log2_diff_max_min_transform_block_size = 0;
    uint8_t max_transform_hierarchy_depth_inter = 0;
    uint8_t max_transform_hierarchy_depth_intra = 0;

    uint8_t scaling_list_enabled_flag = 0;
    uint8_t sps_scaling_list_data_present_flag = 0;
    ScalingListData scaling_list_data;

    uint8_t amp_enabled_flag = 0;
    uint8_t sample_adaptive_offset_enabled_flag = 0;

    uint8_t pcm_enabled_flag = 0;
    uint8_t pcm_sample_bit_depth_luma_minus1 = 0;
    uint8_t pcm_sample_bit_depth_chroma_minus1 = 0;
    uint8_t log2_min_pcm_luma_coding_block_size_minus3 = 0;
    uint8_t log2_diff_max_min_pcm_luma_coding_block_size = 0;
    uint8_t pcm_loop_filter_disabled_flag = 0;

    uint8_t num_short_term_ref_pic_sets = 0;
    std::vector<ShortTermReferencePictureSet> short_term_ref_pic_sets;

    uint8_t long_term_ref_pics_present_flag = 0;
    uint8_t num_long_term_ref_pics_sps = 0;
    std::vector<LongTermRefPicInfo> long_term_ref_pics_info_vector;

    uint8_t sps_temporal_mvp_enabled_flag = 0;
    uint8_t strong_intra_smoothing_enabled_flag = 0;
    uint8_t vui_parameters_present_flag = 0;
    VideoUsabilityInformationParameters vui_parameters;

    uint8_t sps_extension_flag = 0;
    uint8_t sps_extension_data_flag = 0;

    // derived variables
    int width = 0;
    int height = 0;
};

} // namespace nx::media::h265
