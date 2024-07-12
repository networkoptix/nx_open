// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sequence_parameter_set.h"

#include <QtCore/QString>

#include <nx/codec/nal_units.h>

namespace
{

constexpr int kExtendedSar = 255;

void decodeAspectRatio(int aspect_ratio_idc, unsigned int* w, unsigned int* h)
{
    *w = 0;
    *h = 0;
    switch( aspect_ratio_idc )
    {
        case 1:
            *w = 1;
            *h = 1;
            break;
        case 2:
            *w = 12;
            *h = 11;
            break;
        case 3:
            *w = 10;
            *h = 11;
            break;
        case 4:
            *w = 16;
            *h = 11;
            break;
        case 5:
            *w = 40;
            *h = 33;
            break;
        case 6:
            *w = 24;
            *h = 11;
            break;
        case 7:
            *w = 20;
            *h = 11;
            break;
        case 8:
            *w = 32;
            *h = 11;
            break;
        case 9:
            *w = 80;
            *h = 33;
            break;
        case 10:
            *w = 18;
            *h = 11;
            break;
        case 11:
            *w = 15;
            *h = 11;
            break;
        case 12:
            *w = 64;
            *h = 33;
            break;
        case 13:
            *w = 160;
            *h = 99;
            break;
        case 14:
            *w = 4;
            *h = 3;
            break;
        case 15:
            *w = 3;
            *h = 2;
            break;
        case 16:
            *w = 2;
            *h = 1;
            break;
    }
}
}

namespace nx::media::h264 {

int SequenceParameterSet::deserialize()
{
    if (m_nalBufferLen < 4)
        return NOT_ENOUGHT_BUFFER;
    int rez = NALUnit::deserialize(m_nalBuffer, m_nalBuffer + m_nalBufferLen);
    if (rez != 0)
        return rez;
    profile_idc = m_nalBuffer[1];
    constraint_set0_flag0 = m_nalBuffer[2] >> 7;
    constraint_set0_flag1 = m_nalBuffer[2] >> 6 & 1;
    constraint_set0_flag2 = m_nalBuffer[2] >> 5 & 1;
    constraint_set0_flag3 = m_nalBuffer[2] >> 4 & 1;
    level_idc = m_nalBuffer[3];
    chroma_format_idc = 1; // by default format is 4:2:0
    try {
        bitReader.setBuffer(m_nalBuffer + 4, m_nalBuffer + m_nalBufferLen);
        seq_parameter_set_id = bitReader.getGolomb();
        pic_order_cnt_type = 0;
        if( profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 144
            || profile_idc == 244 || profile_idc == 44 || profile_idc == 83 || profile_idc == 86
            || profile_idc == 118 || profile_idc == 128)
        {
            chroma_format_idc = bitReader.getGolomb();
            if(chroma_format_idc == 3)
                residual_colour_transform_flag = bitReader.getBits(1);
            bit_depth_luma = bitReader.getGolomb() + 8;
            bit_depth_chroma = bitReader.getGolomb() + 8;
            qpprime_y_zero_transform_bypass_flag = bitReader.getBits(1);
            seq_scaling_matrix_present_flag = bitReader.getBits(1);
            if(seq_scaling_matrix_present_flag != 0)
            {
                for(int i = 0; i < 8; i++)
                {
                    seq_scaling_list_present_flag[i] = bitReader.getBits(1);
                    if( seq_scaling_list_present_flag[i]){
                        if( i < 6 )
                            scaling_list( ScalingList4x4[ i ], 16,
                                               UseDefaultScalingMatrix4x4Flag[i]);
                        else
                            scaling_list( ScalingList8x8[ i - 6 ], 64,
                                               UseDefaultScalingMatrix8x8Flag[i-6]);
                    }
                }

            }
        }
        log2_max_frame_num = bitReader.getGolomb() + 4;

        // next parameters not used  now.

        pic_order_cnt_type = bitReader.getGolomb();
        log2_max_pic_order_cnt_lsb = 0;
        delta_pic_order_always_zero_flag = 0;
        if( pic_order_cnt_type == 0)
            log2_max_pic_order_cnt_lsb = bitReader.getGolomb() + 4;
        else if( pic_order_cnt_type == 1 )
        {
            delta_pic_order_always_zero_flag = bitReader.getBits(1);
            offset_for_non_ref_pic = bitReader.getSignedGolomb();
            offset_for_top_to_bottom_field = bitReader.getSignedGolomb();
            num_ref_frames_in_pic_order_cnt_cycle = bitReader.getGolomb();
            if (num_ref_frames_in_pic_order_cnt_cycle >= 256) {
                return NOT_ENOUGHT_BUFFER;;
            }

            for(int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
                offset_for_ref_frame[i] = bitReader.getSignedGolomb();
        }

        num_ref_frames = bitReader.getGolomb();
        gaps_in_frame_num_value_allowed_flag = bitReader.getBits(1);
        pic_width_in_mbs = bitReader.getGolomb() + 1;
        pic_height_in_map_units = bitReader.getGolomb() + 1;
        frame_mbs_only_flag = bitReader.getBits(1);
        if(!frame_mbs_only_flag)
            mb_adaptive_frame_field_flag = bitReader.getBits(1);
        direct_8x8_inference_flag = bitReader.getBits(1);
        frame_cropping_flag = bitReader.getBits(1);
        if( frame_cropping_flag ) {
            frame_crop_left_offset = bitReader.getGolomb();
            frame_crop_right_offset = bitReader.getGolomb();
            frame_crop_top_offset = bitReader.getGolomb();
            frame_crop_bottom_offset = bitReader.getGolomb();
        }
        pic_size_in_map_units = pic_width_in_mbs * pic_height_in_map_units; // * (2 - frame_mbs_only_flag);
        vui_parameters_present_flag = bitReader.getBits(1);
        if( vui_parameters_present_flag ) {
            deserializeVuiParameters();
        }
        full_sps_bit_len = bitReader.getBitsCount() + 32;

        //m_pulldown = ((abs(getFPS() - 23.97) < FRAME_RATE_EPS) || (abs(getFPS() - 59.94) < FRAME_RATE_EPS)) && pic_struct_present_flag;

        return 0;
    } catch (nx::utils::BitStreamException const&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

void SequenceParameterSet::deserializeVuiParameters()
{
    aspect_ratio_info_present_flag = bitReader.getBit();
    if( aspect_ratio_info_present_flag ) {
        aspect_ratio_idc = bitReader.getBits(8);
        if (aspect_ratio_idc == kExtendedSar)
        {
            sar_width  = bitReader.getBits(16);
            sar_height = bitReader.getBits(16);
        }
    }
    overscan_info_present_flag = bitReader.getBit();
    if( overscan_info_present_flag )
        overscan_appropriate_flag = bitReader.getBit();
    int video_signal_type_present_flag = bitReader.getBit();
    if( video_signal_type_present_flag ) {
        video_format = bitReader.getBits(3);
        video_full_range_flag = bitReader.getBit();
        colour_description_present_flag = bitReader.getBit();
        if( colour_description_present_flag ) {
            colour_primaries = bitReader.getBits(8);
            transfer_characteristics = bitReader.getBits(8);
            matrix_coefficients = bitReader.getBits(8);
        }
    }
    chroma_loc_info_present_flag = bitReader.getBit();
    if( chroma_loc_info_present_flag ) {
        chroma_sample_loc_type_top_field = bitReader.getGolomb();
        chroma_sample_loc_type_bottom_field = bitReader.getGolomb();
    }
    timing_info_present_flag = bitReader.getBit();
    if( timing_info_present_flag ) {
        num_units_in_tick_bit_pos = bitReader.getBitsCount();
        num_units_in_tick = bitReader.getBits(32);
        time_scale = bitReader.getBits(32);
        fixed_frame_rate_flag = bitReader.getBit();
    }
    nal_hrd_parameters_bit_pos = bitReader.getBitsCount() +  32;

    //orig_hrd_parameters_present_flag =
    nal_hrd_parameters_present_flag = bitReader.getBit();
    if( nal_hrd_parameters_present_flag )
        hrd_parameters();

    // last part not used now.
    // looks like x264 sets bug in this bits
    vcl_hrd_parameters_present_flag = bitReader.getBit();
    if( vcl_hrd_parameters_present_flag )
        hrd_parameters();
    if( nal_hrd_parameters_present_flag  ||  vcl_hrd_parameters_present_flag )
        low_delay_hrd_flag = bitReader.getBit();
    pic_struct_present_flag = bitReader.getBit();
    bitstream_restriction_flag = bitReader.getBit();
    if( bitstream_restriction_flag ) {
        motion_vectors_over_pic_boundaries_flag = bitReader.getBit();
        max_bytes_per_pic_denom = bitReader.getGolomb();
        max_bits_per_mb_denom = bitReader.getGolomb();
        log2_max_mv_length_horizontal = bitReader.getGolomb();
        log2_max_mv_length_vertical = bitReader.getGolomb();
        num_reorder_frames = bitReader.getGolomb();
        max_dec_frame_buffering = bitReader.getGolomb();
    }
}

void SequenceParameterSet::serializeHDRParameters(nx::utils::BitStreamWriter& writer)
{
    writer.putGolomb(cpb_cnt_minus1);
    writer.putBits(4, bit_rate_scale);
    writer.putBits(4, cpb_size_scale);
    for(int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ )
    {
        writer.putGolomb(cpb[SchedSelIdx].bit_rate_value_minus1);
        writer.putGolomb(cpb[SchedSelIdx].cpb_size_value_minus1);
        writer.putBit(cpb[SchedSelIdx].cbr_flag);
    }
    writer.putBits(5, initial_cpb_removal_delay_length_minus1);
    writer.putBits(5, cpb_removal_delay_length_minus1);
    writer.putBits(5, dpb_output_delay_length_minus1);
    writer.putBits(5, time_offset_length);
    //writer.flushBits();
}

// insert HDR parameters to bitstream.
void SequenceParameterSet::insertHdrParameters()
{
    if (nal_hrd_parameters_present_flag)
        return; // replace hrd parameters not implemented. only insert
    nal_hrd_parameters_present_flag = 1;
    quint8* newNalBuffer = new quint8[m_nalBufferLen + 16];
    int beforeBytes = nal_hrd_parameters_bit_pos >> 3;
    memcpy(newNalBuffer, m_nalBuffer, beforeBytes);
    nx::utils::BitStreamReader reader;
    reader.setBuffer(m_nalBuffer + beforeBytes, m_nalBuffer + m_nalBufferLen);
    nx::utils::BitStreamWriter writer;
    writer.setBuffer(newNalBuffer + beforeBytes, newNalBuffer + m_nalBufferLen + 16);
    int tmpVal = reader.getBits(nal_hrd_parameters_bit_pos & 7);
    writer.putBits(nal_hrd_parameters_bit_pos & 7, tmpVal);
    writer.putBit(nal_hrd_parameters_present_flag);
    serializeHDRParameters(writer);
    reader.skipBit(); // nal_hrd_parameters_present_flag
    reader.skipBit(); // vcl_hrd_parameters_present_flag
    writer.putBit(vcl_hrd_parameters_present_flag);
    if (vcl_hrd_parameters_present_flag) { // this field exists in input stream
        //reader.skipBit(); // low_delay_hrd_flag
    }
    else
        writer.putBit(low_delay_hrd_flag);
    // copy end of SPS
    int bitRest = full_sps_bit_len - reader.getBitsCount() - beforeBytes*8;
    for (;bitRest >=8; bitRest-=8) {
        quint8 tmpVal = reader.getBits(8);
        writer.putBits(8, tmpVal);
    }
    if (bitRest > 0) {
        quint8 tmpVal = reader.getBits(bitRest);
        writer.putBits(bitRest, tmpVal);
    }
    nx::media::nal::write_rbsp_trailing_bits(writer);
    writer.flushBits();
    delete [] m_nalBuffer;
    m_nalBuffer = newNalBuffer;
    m_nalBufferLen =  writer.getBitsCount()/8 + beforeBytes;
}

void SequenceParameterSet::hrd_parameters()
{
    cpb_cnt_minus1 = bitReader.getGolomb();
    if (cpb_cnt_minus1 >= kCpbCntMax)
        throw nx::utils::BitStreamException();

    bit_rate_scale = bitReader.getBits(4);
    cpb_size_scale = bitReader.getBits(4);

    for( int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
    {
        cpb[SchedSelIdx].bit_rate_value_minus1 = bitReader.getGolomb();
        cpb[SchedSelIdx].cpb_size_value_minus1 = bitReader.getGolomb();
        cpb[SchedSelIdx].cbr_flag = bitReader.getBit();
    }
    initial_cpb_removal_delay_length_minus1 = bitReader.getBits(5);
    cpb_removal_delay_length_minus1 = bitReader.getBits(5);
    dpb_output_delay_length_minus1 = bitReader.getBits(5);
    time_offset_length = bitReader.getBits(5);
}

int SequenceParameterSet::getCropY() const {
    if (chroma_format_idc == 0)
        return (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
    else {
        int SubHeightC = 1;
        if (chroma_format_idc == 1)
            SubHeightC = 2;
        return SubHeightC * (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
    }
}

int SequenceParameterSet::getCropX() const {
    if (chroma_format_idc == 0)
        return frame_crop_left_offset + frame_crop_right_offset;
    else {
        int SubWidthC = 1;
        if (chroma_format_idc == 1 || chroma_format_idc == 2)
            SubWidthC = 2;
        return SubWidthC * (frame_crop_left_offset + frame_crop_right_offset);
    }
}

double SequenceParameterSet::getFPS() const
{
    if (num_units_in_tick != 0) {
        double tmp = time_scale/(float)num_units_in_tick / 2; //(float)(frame_mbs_only_flag+1);
        //if (abs(tmp - (double) 23.9760239760) < 3e-3)
        //    return 23.9760239760;
        return tmp;
    }
    else
        return 0;
}

void SequenceParameterSet::setFps(double fps)
{
    time_scale = (quint32)(fps+0.5) * 1'000'000;
    //time_scale = (quint32)(fps+0.5) * 1'000;
    num_units_in_tick = time_scale / fps + 0.5;
    time_scale *= 2;

    //num_units_in_tick = time_scale/2 / fps;
    NX_ASSERT(num_units_in_tick_bit_pos > 0);
    updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
    updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
}

double SequenceParameterSet::getSar() const
{
    if (!vui_parameters_present_flag || !aspect_ratio_info_present_flag)
        return 1.0;

    uint32_t width = 0;
    uint32_t height = 0;
    if (aspect_ratio_idc == kExtendedSar)
    {
        width = sar_width;
        height = sar_height;
    }
    else
    {
        decodeAspectRatio(aspect_ratio_idc, &width, &height);
    }

    if (width == 0 || height == 0)
        return 1.0;

    return double(width) / height;
}

QString SequenceParameterSet::getStreamDescr()
{
    QTextStream rez;
    rez << "Profile: ";
    if (profile_idc <= 66)
        rez << "Baseline@";
    else if (profile_idc <= 77)
        rez << "Main@";
    else
        rez << "High@";
    rez << level_idc / 10;
    rez << ".";
    rez << level_idc % 10 << "  ";
    rez << "Resolution: " << getWidth() << ':' << getHeight();
    rez << (frame_mbs_only_flag ? 'p' : 'i') << "  ";
    rez << "Frame rate: ";
    double fps = getFPS();
    if (fps != 0) {
        rez <<  fps;
    }
    else
        rez << "not found";
    return *rez.string();
}

} // namespace nx::media::h264
