// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/codec/h264/common.h>
#include <nx/utils/bit_stream.h>

namespace nx::media::h264 {

class NX_CODEC_API SequenceParameterSet: public NALUnit
{
public:
    int profile_idc = 0;
    int constraint_set0_flag0 = 0;
    int constraint_set0_flag1 = 0;
    int constraint_set0_flag2 = 0;
    int constraint_set0_flag3 = 0;
    int level_idc = 0;
    int seq_parameter_set_id = 0;
    int chroma_format_idc = 0;
    int residual_colour_transform_flag = 0;
    int bit_depth_luma = 0;
    int bit_depth_chroma = 0;
    int qpprime_y_zero_transform_bypass_flag = 0;
    int seq_scaling_matrix_present_flag = 0;
    int log2_max_frame_num = 0;
    int pic_order_cnt_type = 0;
    int log2_max_pic_order_cnt_lsb = 0;
    int delta_pic_order_always_zero_flag = 0;
    int offset_for_non_ref_pic = 0;
    int offset_for_top_to_bottom_field = 0;
    int num_ref_frames_in_pic_order_cnt_cycle = 0;
    int offset_for_ref_frame[256];
    int num_ref_frames = 0;
    int gaps_in_frame_num_value_allowed_flag = 0;
    int pic_width_in_mbs = 0;
    int pic_height_in_map_units = 0;
    int frame_mbs_only_flag = 0;
    int mb_adaptive_frame_field_flag = 0;
    int direct_8x8_inference_flag = 0;
    int frame_cropping_flag = 0;
    int frame_crop_left_offset = 0;
    int frame_crop_right_offset = 0;
    int frame_crop_top_offset = 0;
    int frame_crop_bottom_offset = 0;
    int vui_parameters_present_flag = 0;
    //int field_pic_flag;
    int pic_size_in_map_units = 0;
    int nal_hrd_parameters_present_flag = 0;
    //int orig_hrd_parameters_present_flag;
    int vcl_hrd_parameters_present_flag = 0;
    int pic_struct_present_flag = 0;

    int sar_width = 0;
    int sar_height = 0;
    quint32 num_units_in_tick = 0;
    int num_units_in_tick_bit_pos = 0;
    quint32 time_scale = 0;
    int fixed_frame_rate_flag = 0;

    struct Cpb
    {
        int bit_rate_value_minus1 = 0;
        int cpb_size_value_minus1 = 0;
        quint8 cbr_flag = 0;
    };
    static const int kCpbCntMax = 32;
    Cpb cpb[kCpbCntMax];

    int initial_cpb_removal_delay_length_minus1 = 0;
    int cpb_removal_delay_length_minus1 = 0;
    int dpb_output_delay_length_minus1 = 0;
    int time_offset_length = 0;
    int low_delay_hrd_flag = 0;
    int motion_vectors_over_pic_boundaries_flag = 0;
    int max_bytes_per_pic_denom = 0;
    int max_bits_per_mb_denom = 0;
    int log2_max_mv_length_horizontal = 0;
    int log2_max_mv_length_vertical = 0;
    int num_reorder_frames = 0;
    int bitstream_restriction_flag = 0;
    int chroma_loc_info_present_flag = 0;
    int chroma_sample_loc_type_top_field = 0;
    int chroma_sample_loc_type_bottom_field = 0;
    int timing_info_present_flag = 0;
    int video_format = 0;
    int overscan_appropriate_flag = 0;
    int video_full_range_flag = 0;
    int colour_description_present_flag = 0;
    int colour_primaries = 0;
    int transfer_characteristics = 0;
    int matrix_coefficients = 0;
    int aspect_ratio_info_present_flag = 0;
    int aspect_ratio_idc = 0;
    int overscan_info_present_flag = 0;
    int cpb_cnt_minus1 = 0;
    int bit_rate_scale = 0;
    int cpb_size_scale = 0;

    int nal_hrd_parameters_bit_pos = 0;
    int full_sps_bit_len = 0;

    int ScalingList4x4[6][16];
    int ScalingList8x8[2][64];
    bool UseDefaultScalingMatrix4x4Flag[6];
    bool UseDefaultScalingMatrix8x8Flag[2];


    int max_dec_frame_buffering = 0;

    QString getStreamDescr();
    int getWidth() const { return pic_width_in_mbs*16 - getCropX();}
    int getHeight() const {
        return (2 - frame_mbs_only_flag) * pic_height_in_map_units*16 - getCropY();
    }
    double getFPS() const;
    void setFps(double fps);
    double getSar() const;


    SequenceParameterSet()
    {
        memset( ScalingList4x4, 16, sizeof(ScalingList4x4) );
        memset( ScalingList8x8, 16, sizeof(ScalingList8x8) );
        std::fill( (bool*)UseDefaultScalingMatrix4x4Flag, ((bool*)UseDefaultScalingMatrix4x4Flag)+sizeof(UseDefaultScalingMatrix4x4Flag)/sizeof(*UseDefaultScalingMatrix4x4Flag), true );
        std::fill( (bool*)UseDefaultScalingMatrix8x8Flag, ((bool*)UseDefaultScalingMatrix8x8Flag)+sizeof(UseDefaultScalingMatrix8x8Flag)/sizeof(*UseDefaultScalingMatrix8x8Flag), true );
    }

    int deserialize();
    void insertHdrParameters();

private:
    bool seq_scaling_list_present_flag[8];
    void hrd_parameters();
    void deserializeVuiParameters();
    int getCropY() const;
    int getCropX() const;
    void serializeHDRParameters(nx::utils::BitStreamWriter& writer);
};

} // namespace nx::media::h264
