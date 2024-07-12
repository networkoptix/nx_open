// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/codec/h265/hdr_parameters.h>
#include <nx/utils/bit_stream.h>

namespace nx::media::h265 {

struct VideoUsabilityInformationParameters
{
    bool read(nx::utils::BitStreamReader& reader, uint8_t sps_max_sub_layers_minus1);

    uint8_t aspect_ratio_info_present_flag;
    uint8_t aspect_ratio_idc;
    uint16_t sar_width;
    uint16_t sar_height;

    uint8_t overscan_info_present_flag;
    uint8_t overscan_appropriate_flag;

    uint8_t video_signal_type_present_flag;
    uint8_t video_format;
    uint8_t video_full_range_flag;
    uint8_t colour_description_present_flag;
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coeffs;

    uint8_t chroma_loc_info_present_flag;
    uint32_t chroma_sample_loc_type_top_field;
    uint32_t chroma_sample_loc_type_bottom_field;

    uint8_t neutral_chroma_indication_flag;
    uint8_t field_seq_flag;
    uint8_t frame_field_info_present_flag;

    uint8_t default_display_window_flag;
    uint32_t def_disp_win_left_offset;
    uint32_t def_disp_win_right_offset;
    uint32_t def_disp_win_top_offset;
    uint32_t def_disp_win_bottom_offset;

    uint8_t vui_timing_info_present_flag;
    uint32_t vui_num_units_in_tick;
    uint32_t vui_time_scale;
    uint8_t vui_poc_proportional_to_timing_flag;
    uint32_t vui_num_ticks_poc_diff_one_minus1;
    uint8_t vui_hrd_parameters_present_flag;
    HRDParameters hrd_parameters;

    uint8_t bitstream_restriction_flag;
    uint8_t tiles_fixed_structure_flag;
    uint8_t motion_vectors_over_pic_boundaries_flag;
    uint8_t restricted_ref_pic_lists_flag;
    uint32_t min_spatial_segmentation_idc;
    uint32_t max_bytes_per_pic_denom;
    uint32_t max_bits_per_min_cu_denom;
    uint32_t log2_max_mv_length_horizontal;
    uint32_t log2_max_mv_length_vertical;
};

} // namespace nx::media::h265
