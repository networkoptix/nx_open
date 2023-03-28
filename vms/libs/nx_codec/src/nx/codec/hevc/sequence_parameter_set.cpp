// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sequence_parameter_set.h"

#include <memory>

#include <nx/utils/bit_stream.h>

namespace nx::media::hevc {

bool SequenceParameterSet::read(const uint8_t* data, int size)
{
    nx::utils::BitStreamReader reader;
    try
    {
        reader.setBuffer(data, size);
        sps_video_parameter_set_id = reader.getBits(4);
        sps_max_sub_layers_minus1 = reader.getBits(3);
        sps_temporal_id_nesting_flag = reader.getBit();
        if (!profile_tier_level.read(reader, sps_max_sub_layers_minus1))
            return false;

        sps_seq_parameter_set_id = reader.getGolomb();
        chroma_format_idc = reader.getGolomb();

        if (chroma_format_idc == 3)
            separate_colour_plane_flag = reader.getBit();

        width = pic_width_in_luma_samples = reader.getGolomb();
        height = pic_height_in_luma_samples = reader.getGolomb();

        const bool conformance_window_flag = reader.getBit();
        if (conformance_window_flag)
        {
            // 2 for yuv420 or yuv422.
            const int subWidthC = chroma_format_idc == 1 || chroma_format_idc == 2 ? 2 : 1;
            // 2 for yuv420, otherwise 1.
            const int subHeightC = chroma_format_idc == 1 ? 2 : 1;

            const int conf_win_left_offset = reader.getGolomb();
            const int conf_win_right_offset = reader.getGolomb();
            const int conf_win_top_offset = reader.getGolomb();
            const int conf_win_bottom_offset = reader.getGolomb();

            width -= subWidthC * (conf_win_left_offset + conf_win_right_offset);
            height -= subHeightC * (conf_win_top_offset + conf_win_bottom_offset);
        }
        bit_depth_luma_minus8 = reader.getGolomb();
        bit_depth_chroma_minus8 = reader.getGolomb();
        log2_max_pic_order_cnt_lsb_minus4 = reader.getGolomb();
        sps_sub_layer_ordering_info_present_flag = reader.getBits(1);
        sps_sub_layer_ordering_info_vector.clear();
        for (int i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1); i <= sps_max_sub_layers_minus1; ++ i)
        {
            SubLayerOrderingInfo newInfo;
            newInfo.sps_max_dec_pic_buffering_minus1 = reader.getGolomb();
            newInfo.sps_max_num_reorder_pics = reader.getGolomb();
            newInfo.sps_max_latency_increase_plus1 = reader.getGolomb();
            sps_sub_layer_ordering_info_vector.push_back(newInfo);
        }

        log2_min_luma_coding_block_size_minus3 = reader.getGolomb();
        log2_diff_max_min_luma_coding_block_size = reader.getGolomb();
        log2_min_transform_block_size_minus2 = reader.getGolomb();
        log2_diff_max_min_transform_block_size = reader.getGolomb();

        max_transform_hierarchy_depth_inter = reader.getGolomb();
        max_transform_hierarchy_depth_intra = reader.getGolomb();

        scaling_list_enabled_flag = reader.getBits(1);
        if (scaling_list_enabled_flag)
        {
            sps_scaling_list_data_present_flag = reader.getBits(1);
            if (sps_scaling_list_data_present_flag)
            {
                if (!scaling_list_data.read(reader))
                    return false;
            }
        }
        amp_enabled_flag = reader.getBits(1);
        sample_adaptive_offset_enabled_flag = reader.getBits(1);
        pcm_enabled_flag = reader.getBits(1);
        if (pcm_enabled_flag)
        {
            pcm_sample_bit_depth_luma_minus1 = reader.getBits(4);
            pcm_sample_bit_depth_chroma_minus1 = reader.getBits(4);
            log2_min_pcm_luma_coding_block_size_minus3 = reader.getGolomb();
            log2_diff_max_min_pcm_luma_coding_block_size = reader.getGolomb();
            pcm_loop_filter_disabled_flag = reader.getGolomb();
        }

        num_short_term_ref_pic_sets = reader.getGolomb();
        if (num_short_term_ref_pic_sets > 64)
            return false;

        short_term_ref_pic_sets.clear();
        for (int i = 0; i < num_short_term_ref_pic_sets; ++i)
        {
            ShortTermReferencePictureSet newSet;
            uint32_t max_dec_pic_buffering_minus1 = sps_sub_layer_ordering_info_present_flag
                    ? sps_sub_layer_ordering_info_vector[sps_max_sub_layers_minus1].sps_max_dec_pic_buffering_minus1
                    : sps_sub_layer_ordering_info_vector[0].sps_max_dec_pic_buffering_minus1;

            if (!newSet.read(
                    reader,
                    i,
                    max_dec_pic_buffering_minus1,
                    num_short_term_ref_pic_sets,
                    short_term_ref_pic_sets))
            {
                return false;
            }

            short_term_ref_pic_sets.push_back(newSet);
        }

        long_term_ref_pics_present_flag = reader.getBits(1);
        if (long_term_ref_pics_present_flag)
        {
            num_long_term_ref_pics_sps = reader.getGolomb();
            if (num_long_term_ref_pics_sps > 32)
                return false;

            long_term_ref_pics_info_vector.clear();
            for (int i = 0; i < num_long_term_ref_pics_sps; ++i)
            {
                LongTermRefPicInfo newInfo;

                if (log2_max_pic_order_cnt_lsb_minus4 + 4 > 64)
                    return false;

                newInfo.lt_ref_pic_poc_lsb_sps = reader.getBits(log2_max_pic_order_cnt_lsb_minus4 + 4);
                newInfo.used_by_curr_pic_lt_sps_flag = reader.getBits(1);

                long_term_ref_pics_info_vector.push_back(newInfo);
            }
        }

        sps_temporal_mvp_enabled_flag = reader.getBits(1);
        strong_intra_smoothing_enabled_flag = reader.getBits(1);
        vui_parameters_present_flag = reader.getBits(1);
        if (vui_parameters_present_flag)
        {
            if (!vui_parameters.read(reader, sps_max_sub_layers_minus1))
                return false;
        }

        sps_extension_flag = reader.getBits(1);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

} // namespace nx::media::hevc
