// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picture_parameter_set.h"

namespace nx::media::hevc {

bool PictureParameterSet::read(const uint8_t* data, int size)
{
    try {
        nx::utils::BitStreamReader reader(data, size);
        pps_pic_parameter_set_id = reader.getGolomb();
        pps_seq_parameter_set_id = reader.getGolomb();
        dependent_slice_segments_enabled_flag = reader.getBits(1);
        output_flag_present_flag = reader.getBits(1);
        num_extra_slice_header_bits = reader.getBits(3);
        sign_data_hiding_enabled_flag = reader.getBits(1);
        cabac_init_present_flag = reader.getBits(1);
        num_ref_idx_l0_default_active_minus1 = reader.getGolomb();
        num_ref_idx_l1_default_active_minus1 = reader.getGolomb();
        init_qp_minus26 = reader.getSignedGolomb();
        constrained_intra_pred_flag = reader.getBits(1);
        transform_skip_enabled_flag = reader.getBits(1);
        cu_qp_delta_enabled_flag = reader.getBits(1);
        if (cu_qp_delta_enabled_flag)
            diff_cu_qp_delta_depth = reader.getGolomb();

        pps_cb_qp_offset = reader.getSignedGolomb();
        pps_cr_qp_offset = reader.getSignedGolomb();
        pps_slice_chroma_qp_offsets_present_flag = reader.getBits(1);
        weighted_pred_flag = reader.getBits(1);
        weighted_bipred_flag = reader.getBits(1);
        transquant_bypass_enabled_flag = reader.getBits(1);
        tiles_enabled_flag = reader.getBits(1);
        entropy_coding_sync_enabled_flag = reader.getBits(1);
        if (tiles_enabled_flag)
        {
            num_tile_columns_minus1 = reader.getGolomb();
            num_tile_rows_minus1 = reader.getGolomb();
            uniform_spacing_flag = reader.getBits(1);
            if (!uniform_spacing_flag)
            {
                column_width_minus1.resize(num_tile_columns_minus1);
                for(uint32_t i = 0; i < num_tile_columns_minus1; ++i)
                    column_width_minus1[i] = reader.getGolomb();

                row_height_minus1.resize(num_tile_rows_minus1);
                for(uint32_t i = 0; i < num_tile_rows_minus1; ++i)
                    row_height_minus1[i] = reader.getGolomb();

            }
            loop_filter_across_tiles_enabled_flag = reader.getBits(1);
        }
        pps_loop_filter_across_slices_enabled_flag = reader.getBits(1);
        deblocking_filter_control_present_flag = reader.getBits(1);
        if (deblocking_filter_control_present_flag)
        {
            deblocking_filter_override_enabled_flag = reader.getBits(1);
            pps_deblocking_filter_disabled_flag = reader.getBits(1);
            if (!pps_deblocking_filter_disabled_flag)
            {
                pps_beta_offset_div2 = reader.getSignedGolomb();
                pps_tc_offset_div2 = reader.getSignedGolomb();
            }
        }
        pps_scaling_list_data_present_flag = reader.getBits(1);
        /*if (pps_scaling_list_data_present_flag)
        {
            if (scaling_list_data.read(reader))
                return false;
        }
        lists_modification_present_flag = reader.getBits(1);
        log2_parallel_merge_level_minus2 = reader.getGolomb();
        slice_segment_header_extension_present_flag = reader.getBits(1);
        pps_extension_flag = reader.getBits(1);*/
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}

} // namespace nx::media::hevc
