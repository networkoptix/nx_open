// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_usability_information_parameters.h"

namespace{
static constexpr uint8_t kArExtendedSar = 255;
}

namespace nx::media::hevc {

bool VideoUsabilityInformationParameters::read(
    nx::utils::BitStreamReader& reader, uint8_t sps_max_sub_layers_minus1)
{
    aspect_ratio_info_present_flag = reader.getBits(1);
    if (aspect_ratio_info_present_flag)
    {
        aspect_ratio_idc = reader.getBits(8);
        if (aspect_ratio_idc == kArExtendedSar)
        {
            sar_width = reader.getBits(16);
            sar_height = reader.getBits(16);
        }
    }

    overscan_info_present_flag = reader.getBits(1);
    if (overscan_info_present_flag)
        overscan_appropriate_flag = reader.getBits(1);

    video_signal_type_present_flag = reader.getBits(1);
    if (video_signal_type_present_flag)
    {
        video_format = reader.getBits(3);
        video_full_range_flag = reader.getBits(1);
        colour_description_present_flag = reader.getBits(1);
        if (colour_description_present_flag)
        {
            colour_primaries = reader.getBits(8);
            transfer_characteristics = reader.getBits(8);
            matrix_coeffs = reader.getBits(8);
        }
    }

    chroma_loc_info_present_flag = reader.getBits(1);
    if (chroma_loc_info_present_flag)
    {
        chroma_sample_loc_type_top_field = reader.getGolomb();
        chroma_sample_loc_type_bottom_field = reader.getGolomb();
    }

    neutral_chroma_indication_flag = reader.getBits(1);
    field_seq_flag = reader.getBits(1);
    frame_field_info_present_flag = reader.getBits(1);

    default_display_window_flag = reader.getBits(1);
    if (default_display_window_flag)
    {
        def_disp_win_left_offset = reader.getGolomb();
        def_disp_win_right_offset = reader.getGolomb();
        def_disp_win_top_offset = reader.getGolomb();
    }

    vui_timing_info_present_flag = reader.getBits(1);
    if (vui_timing_info_present_flag)
    {
        vui_num_units_in_tick = reader.getBits(32);
        vui_time_scale = reader.getBits(32);

        vui_poc_proportional_to_timing_flag = reader.getBits(1);
        if (vui_poc_proportional_to_timing_flag)
            vui_num_ticks_poc_diff_one_minus1 = reader.getGolomb();

        vui_hrd_parameters_present_flag = reader.getBits(1);
        if (vui_hrd_parameters_present_flag)
        {
            if (!hrd_parameters.read(reader, 1, sps_max_sub_layers_minus1))
                return false;
        }
    }

    bitstream_restriction_flag = reader.getBits(1);
    if (bitstream_restriction_flag)
    {
        tiles_fixed_structure_flag = reader.getBits(1);
        motion_vectors_over_pic_boundaries_flag = reader.getBits(1);
        restricted_ref_pic_lists_flag = reader.getBits(1);
        min_spatial_segmentation_idc = reader.getGolomb();
        max_bytes_per_pic_denom = reader.getGolomb();
        max_bits_per_min_cu_denom = reader.getGolomb();
        log2_max_mv_length_horizontal = reader.getGolomb();
        log2_max_mv_length_vertical = reader.getGolomb();
    }
    return true;
}

} // namespace nx::media::hevc
