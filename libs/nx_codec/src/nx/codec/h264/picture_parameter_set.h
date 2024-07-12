// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/codec/h264/common.h>

namespace nx::media::h264 {

class NX_CODEC_API PictureParameterSet: public NALUnit
{
public:
    int pic_parameter_set_id = 0;
    int seq_parameter_set_id = 0;
    int entropy_coding_mode_flag;
    int pic_order_present_flag = 0;
    int num_ref_idx_l0_active_minus1 = 0;
    int num_ref_idx_l1_active_minus1 = 0;
    int weighted_pred_flag = 0;
    int weighted_bipred_idc = 0;
    int pic_init_qp_minus26 = 0;
    int pic_init_qs_minus26 = 0;
    int transform_8x8_mode_flag = 0;
    int pic_scaling_matrix_present_flag = 0;
    int chroma_qp_index_offset = 0;
    int deblocking_filter_control_present_flag = 0;
    int constrained_intra_pred_flag = 0;
    int redundant_pic_cnt_present_flag = 0;
    int run_length_minus1[256];
    int top_left[256];
    int bottom_right[256];
    int slice_group_id[256];
    int slice_group_change_direction_flag = 0;
    int slice_group_change_rate = 0;
    int num_slice_groups_minus1 = 0;
    int slice_group_map_type = 0;
    int second_chroma_qp_index_offset = 0;
    int scalingLists4x4[6][16];
    bool useDefaultScalingMatrix4x4Flag[6];
    int scalingLists8x8[2][64];
    bool useDefaultScalingMatrix8x8Flag[2];

    PictureParameterSet();
    int deserialize();
    int deserializeID(quint8* buffer, quint8* end);

private:
    int m_ppsLenInMbit = 0;
    int entropy_coding_mode_BitPos = 0;
};

} // namespace nx::media::h264
