// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picture_parameter_set.h"

#include <memory>

namespace nx::media::h264 {

int calc_rbsp_trailing_bits_cnt(uint8_t val)
{
    int cnt = 1;
    while ((val & 1) == 0) {
        cnt++;
        val >>= 1;
        NX_ASSERT(val != 0);
        if (val == 0)
            return 0;
    }
    return cnt;
}

 PictureParameterSet:: PictureParameterSet()
{
    memset( scalingLists4x4, 0x10, sizeof(scalingLists4x4) );
    memset( useDefaultScalingMatrix4x4Flag, 0, sizeof(useDefaultScalingMatrix4x4Flag) );
    memset( scalingLists8x8, 0x10, sizeof(scalingLists8x8) );
    memset( useDefaultScalingMatrix8x8Flag, 0, sizeof(useDefaultScalingMatrix8x8Flag) );
}

int PictureParameterSet::deserializeID(quint8* buffer, quint8* end)
{
    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;
    if (end - buffer < 2)
        return NOT_ENOUGHT_BUFFER;
    bitReader.setBuffer(buffer + 1, end);
    return 0;
    try {
        pic_parameter_set_id = bitReader.getGolomb();
    }
    catch (nx::utils::BitStreamException const&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

int PictureParameterSet::deserialize()
{
    quint8* nalEnd = m_nalBuffer + m_nalBufferLen;
    int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
    if (rez != 0)
        return rez;
    if (nalEnd - m_nalBuffer < 2)
        return NOT_ENOUGHT_BUFFER;
    try {
        bitReader.setBuffer(m_nalBuffer + 1, nalEnd);
        pic_parameter_set_id = bitReader.getGolomb();
        seq_parameter_set_id = bitReader.getGolomb();
        entropy_coding_mode_BitPos = bitReader.getBitsCount();
        entropy_coding_mode_flag = bitReader.getBit();
        pic_order_present_flag = bitReader.getBit();
        num_slice_groups_minus1 = bitReader.getGolomb();
        slice_group_map_type = 0;
        if( num_slice_groups_minus1 > 0 ) {
            slice_group_map_type = bitReader.getGolomb();
            if( slice_group_map_type  ==  0 )
            {
                if (num_slice_groups_minus1 >= 256)
                    return NOT_ENOUGHT_BUFFER;
                for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
                    run_length_minus1[iGroup] = bitReader.getGolomb();
            }
            else if( slice_group_map_type  ==  2)
            {
                if (num_slice_groups_minus1 >= 256)
                    return NOT_ENOUGHT_BUFFER;
                for( int iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {
                    top_left[ iGroup ] = bitReader.getGolomb();
                    bottom_right[ iGroup ] = bitReader.getGolomb();
                }
            }
            else if(  slice_group_map_type  ==  3  ||
                        slice_group_map_type  ==  4  ||
                        slice_group_map_type  ==  5 )
            {
                slice_group_change_direction_flag = bitReader.getBits(1);
                slice_group_change_rate = bitReader.getGolomb() + 1;
            } else if( slice_group_map_type  ==  6 )
            {
                int pic_size_in_map_units_minus1 = bitReader.getGolomb();
                if (pic_size_in_map_units_minus1 >= 256)
                    return NOT_ENOUGHT_BUFFER;
                for( int i = 0; i <= pic_size_in_map_units_minus1; i++ ) {
                    int bits = ceil_log2( num_slice_groups_minus1 + 1 );
                    (void)bits;
                    slice_group_id[i] = bitReader.getBits(1);
                }
            }
        }
        num_ref_idx_l0_active_minus1 = bitReader.getGolomb();
        num_ref_idx_l1_active_minus1 = bitReader.getGolomb();
        weighted_pred_flag = bitReader.getBit();
        weighted_bipred_idc = bitReader.getBits(2);
        pic_init_qp_minus26 = bitReader.getSignedGolomb();  // relative to 26
        pic_init_qs_minus26 = bitReader.getSignedGolomb();  // relative to 26
        chroma_qp_index_offset =  bitReader.getSignedGolomb();
        deblocking_filter_control_present_flag = bitReader.getBit();
        constrained_intra_pred_flag = bitReader.getBit();
        redundant_pic_cnt_present_flag = bitReader.getBit();
        m_ppsLenInMbit = bitReader.getBitsCount() + 8;

        int trailingBits = calc_rbsp_trailing_bits_cnt(nalEnd[-1]);
        int nalLenInBits = m_nalBufferLen*8 - trailingBits;
        if (m_ppsLenInMbit < nalLenInBits)
        {
            transform_8x8_mode_flag = bitReader.getBit();
            pic_scaling_matrix_present_flag = bitReader.getBit();

            if( pic_scaling_matrix_present_flag )
                for( int i = 0; i < 6 + 2* transform_8x8_mode_flag; ++i )
                {
                    int pic_scaling_list_present_flag_i = bitReader.getBit();
                    if( pic_scaling_list_present_flag_i )
                    {
                        if( i < 6 )
                            scaling_list( scalingLists4x4[i], 16, useDefaultScalingMatrix4x4Flag[i] );
                        else
                            scaling_list( scalingLists8x8[i-6], 64, useDefaultScalingMatrix8x8Flag[i-6] );
                    }
                }
            second_chroma_qp_index_offset = bitReader.getSignedGolomb();
        }

        return 0;
    } catch (nx::utils::BitStreamException const&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

} // namespace nx::media::h264
