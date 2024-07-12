// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "slice_header.h"

#include <nx/codec/h264/common.h>

namespace nx::media::h264 {

int SliceHeader::deserialize(const SequenceParameterSet* sps, const PictureParameterSet* pps)
{
    return deserialize(m_nalBuffer, m_nalBuffer + m_nalBufferLen, sps, pps);
}

int SliceHeader::deserialize(quint8* buffer, quint8* end, const SequenceParameterSet* sps, const PictureParameterSet* pps)
{
    m_ref_pic_vect.clear();
    m_ref_pic_vect2.clear();
    dec_ref_pic_vector.clear();
    luma_weight_l0.clear();
    luma_offset_l0.clear();
    chroma_weight_l0.clear();
    chroma_offset_l0.clear();
    luma_weight_l1.clear();
    luma_offset_l1.clear();
    chroma_weight_l1.clear();
    chroma_offset_l1.clear();

    if (end - buffer < 2)
        return NOT_ENOUGHT_BUFFER;

    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;
    //init_bitReader.getBits( buffer+1, (end - buffer) * 8);

    bitReader.setBuffer(buffer + 1, end);
    //m_getbitContextBuffer = buffer+1;
    rez = deserializeSliceHeader(sps, pps);
    if (rez != 0 || m_shortDeserializeMode)
        return rez;

    if (nal_unit_type == NALUnitType::nuSliceA || nal_unit_type == NALUnitType::nuSliceB || nal_unit_type == NALUnitType::nuSliceC)
    {
        /*int slice_id =*/ bitReader.getGolomb();
        if (nal_unit_type == NALUnitType::nuSliceB || nal_unit_type == NALUnitType::nuSliceC)
            if( pps->redundant_pic_cnt_present_flag )
                /*int redundant_pic_cnt =*/ bitReader.getGolomb();
    }
    m_fullHeaderLen = bitReader.getBitsCount() + 8;
    return 0;
    //return deserializeSliceData();
}

int SliceHeader::deserializeSliceHeader(const SequenceParameterSet* sps, const PictureParameterSet* pps)
{
    try {
        first_mb_in_slice = bitReader.getGolomb();
        orig_slice_type = slice_type = bitReader.getGolomb();
        if (slice_type >= 5)
            slice_type -= 5; // +5 flag is: all other slice at this picture must be same type
        pic_parameter_set_id = bitReader.getGolomb();

        m_frameNumBitPos = bitReader.getBitsCount(); //getBitContext.buffer

        if (sps == nullptr)
            return SPS_OR_PPS_NOT_READY;

        m_frameNumBits = sps->log2_max_frame_num;
        frame_num = bitReader.getBits( m_frameNumBits);
        bottom_field_flag = 0;
        m_field_pic_flag = 0;
        if( sps->frame_mbs_only_flag == 0) {
            m_field_pic_flag = bitReader.getBits( 1);
            if( m_field_pic_flag )
                bottom_field_flag = bitReader.getBits( 1);
        }
        if( nal_unit_type == 5)
            idr_pic_id = bitReader.getGolomb();

        if (pps == nullptr || pps->pic_parameter_set_id != pic_parameter_set_id)
            return SPS_OR_PPS_NOT_READY;

        m_picOrderBitPos = -1;
        if( sps->pic_order_cnt_type ==  0)
        {
            m_picOrderBitPos = bitReader.getBitsCount(); //getBitContext.buffer
            m_picOrderNumBits = sps->log2_max_pic_order_cnt_lsb;
            pic_order_cnt_lsb = bitReader.getBits( sps->log2_max_pic_order_cnt_lsb);
            if( pps->pic_order_present_flag &&  !m_field_pic_flag)
                delta_pic_order_cnt_bottom = bitReader.getSignedGolomb();
        }

        if (m_shortDeserializeMode)
            return 0;

        if(sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag)
        {
            delta_pic_order_cnt[0] = bitReader.getSignedGolomb();
            if( pps->pic_order_present_flag && !m_field_pic_flag)
                delta_pic_order_cnt[1] = bitReader.getSignedGolomb();
        }
        if(pps->redundant_pic_cnt_present_flag)
            redundant_pic_cnt = bitReader.getGolomb();

        if( slice_type  ==  B_TYPE )
            direct_spatial_mv_pred_flag = bitReader.getBit();
        num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_active_minus1;
        num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_active_minus1;
        if( slice_type == P_TYPE || slice_type == SP_TYPE || slice_type == B_TYPE )
        {
            num_ref_idx_active_override_flag = bitReader.getBit();
            if( num_ref_idx_active_override_flag ) {
                num_ref_idx_l0_active_minus1 = bitReader.getGolomb();
                if( slice_type == B_TYPE)
                    num_ref_idx_l1_active_minus1 = bitReader.getGolomb();
            }
        }
        ref_pic_list_reordering();
        if( ( pps->weighted_pred_flag  &&  ( slice_type == P_TYPE  ||  slice_type == SP_TYPE ) )  ||
            ( pps->weighted_bipred_idc  ==  1  &&  slice_type  ==  B_TYPE ) )
            pred_weight_table();
        if( nal_ref_idc != 0 )
            dec_ref_pic_marking();
        if( pps->entropy_coding_mode_flag  &&  slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE ) {
            cabac_init_idc = bitReader.getGolomb();
            //NX_ASSERT(cabac_init_idc >=0 &&  cabac_init_idc <= 2);
        }
        slice_qp_delta = bitReader.getSignedGolomb();
        if( slice_type  ==  SP_TYPE  ||  slice_type  ==  SI_TYPE ) {
            if( slice_type  ==  SP_TYPE )
                sp_for_switch_flag = bitReader.getBits(1);
            slice_qs_delta = bitReader.getSignedGolomb();
        }
        if( pps->deblocking_filter_control_present_flag ) {
            disable_deblocking_filter_idc = bitReader.getGolomb();
            if( disable_deblocking_filter_idc != 1 ) {
                slice_alpha_c0_offset_div2 = bitReader.getSignedGolomb();
                slice_beta_offset_div2 = bitReader.getSignedGolomb();
            }
        }
        if( pps->num_slice_groups_minus1 > 0  &&
            pps->slice_group_map_type >= 3  &&  pps->slice_group_map_type <= 5) {
            int bits = ceil_log2( sps->pic_size_in_map_units / (double)pps->slice_group_change_rate + 1 );
            slice_group_change_cycle = bitReader.getBits(bits);
        }

        return 0;
    } catch (nx::utils::BitStreamException const&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

void SliceHeader::pred_weight_table()
{
    luma_log2_weight_denom = bitReader.getGolomb();
    if( sps->chroma_format_idc  !=  0 )
        chroma_log2_weight_denom = bitReader.getGolomb();
    for(int i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) {
        int luma_weight_l0_flag = bitReader.getBit();
        if( luma_weight_l0_flag ) {
            luma_weight_l0.push_back(bitReader.getSignedGolomb());
            luma_offset_l0.push_back(bitReader.getSignedGolomb());
        }
        else {
            luma_weight_l0.push_back(INT_MAX);
            luma_offset_l0.push_back(INT_MAX);
        }
        if ( sps->chroma_format_idc  !=  0 ) {
            int chroma_weight_l0_flag = bitReader.getBit();
            if( chroma_weight_l0_flag ) {
                for(int j =0; j < 2; j++ ) {
                    chroma_weight_l0.push_back(bitReader.getSignedGolomb());
                    chroma_offset_l0.push_back(bitReader.getSignedGolomb());
                }
            }
            else {
                for(int j =0; j < 2; j++ ) {
                    chroma_weight_l0.push_back(INT_MAX);
                    chroma_offset_l0.push_back(INT_MAX);
                }
            }
        }
    }
    if( slice_type  ==  B_TYPE )
        for(int i = 0; i <= num_ref_idx_l1_active_minus1; i++ ) {
            int luma_weight_l1_flag = bitReader.getBit();
            if( luma_weight_l1_flag ) {
                luma_weight_l1.push_back(bitReader.getSignedGolomb());
                luma_offset_l1.push_back(bitReader.getSignedGolomb());
            }
            else {
                luma_weight_l1.push_back(INT_MAX);
                luma_offset_l1.push_back(INT_MAX);
            }
            if( sps->chroma_format_idc  !=  0 ) {
                int chroma_weight_l1_flag = bitReader.getBit();
                if( chroma_weight_l1_flag ) {
                    for(int j = 0; j < 2; j++ ) {
                        chroma_weight_l1.push_back(bitReader.getSignedGolomb());
                        chroma_offset_l1.push_back(bitReader.getSignedGolomb());
                    }
                }
                else {
                    for(int j = 0; j < 2; j++ ) {
                        chroma_weight_l1.push_back(INT_MAX);
                        chroma_offset_l1.push_back(INT_MAX);
                    }
                }
            }
        }
}

void SliceHeader::dec_ref_pic_marking()
{
    if( nal_unit_type == NALUnitType::nuSliceIDR ) {
        no_output_of_prior_pics_flag = bitReader.getBits(1);
        long_term_reference_flag = bitReader.getBits(1);
    } else
    {
        adaptive_ref_pic_marking_mode_flag =  bitReader.getBits(1);
        if( adaptive_ref_pic_marking_mode_flag )
            do {
                memory_management_control_operation = bitReader.getGolomb();
                dec_ref_pic_vector.push_back(memory_management_control_operation);
                if (memory_management_control_operation != 0) {
                    quint32 tmp = bitReader.getGolomb();
                    /*if( memory_management_control_operation  ==  1  ||
                        memory_management_control_operation  ==  3 )
                        int difference_of_pic_nums_minus1 = tmp;
                    if(memory_management_control_operation  ==  2  )
                        int long_term_pic_num = tmp;
                     if( memory_management_control_operation  ==  3  ||
                        memory_management_control_operation  ==  6 )
                        int long_term_frame_idx = tmp;
                    if( memory_management_control_operation  ==  4 )
                        int max_long_term_frame_idx_plus1 = tmp;*/
                    dec_ref_pic_vector.push_back(tmp);
                }
            } while( memory_management_control_operation  !=  0 );
    }
}

void SliceHeader::ref_pic_list_reordering()
{
    int reordering_of_pic_nums_idc = 0;
    if( slice_type != I_TYPE && slice_type !=  SI_TYPE ) {
        ref_pic_list_reordering_flag_l0 = bitReader.getBit();
        if( ref_pic_list_reordering_flag_l0 )
            do {
                reordering_of_pic_nums_idc = bitReader.getGolomb();
                if( reordering_of_pic_nums_idc  ==  0  || reordering_of_pic_nums_idc  ==  1 )
                {
                    int tmp = bitReader.getGolomb();
                    abs_diff_pic_num_minus1 = tmp;
                    m_ref_pic_vect.push_back(reordering_of_pic_nums_idc);
                    m_ref_pic_vect.push_back(tmp);
                }
                else if( reordering_of_pic_nums_idc  ==  2 ) {
                    int tmp = bitReader.getGolomb();
                    long_term_pic_num = tmp;
                    m_ref_pic_vect.push_back(reordering_of_pic_nums_idc);
                    m_ref_pic_vect.push_back(tmp);
                }
            } while(reordering_of_pic_nums_idc  !=  3);
    }
    if( slice_type  ==  B_TYPE ) {
        ref_pic_list_reordering_flag_l1 = bitReader.getBit();
        if( ref_pic_list_reordering_flag_l1 )
            do {
                reordering_of_pic_nums_idc = bitReader.getGolomb();
                quint32 tmp = bitReader.getGolomb();
                if( reordering_of_pic_nums_idc  ==  0  ||
                    reordering_of_pic_nums_idc  ==  1 )
                    abs_diff_pic_num_minus1 = tmp;
                else if( reordering_of_pic_nums_idc  ==  2 )
                    long_term_pic_num = tmp;
                m_ref_pic_vect2.push_back(reordering_of_pic_nums_idc);
                m_ref_pic_vect2.push_back(tmp);
            } while(reordering_of_pic_nums_idc  !=  3);
    }
}

int SliceHeader::NextMbAddress(int n)
{
    int FrameHeightInMbs = ( 2 - sps->frame_mbs_only_flag ) * sps->pic_height_in_map_units;
    int FrameWidthInMbs = ( 2 - sps->frame_mbs_only_flag ) * sps->pic_width_in_mbs;
    int PicHeightInMbs = FrameHeightInMbs / ( 1 + m_field_pic_flag );
    int PicWidthInMbs = FrameWidthInMbs / ( 1 + m_field_pic_flag );
    int PicSizeInMbs = PicWidthInMbs * PicHeightInMbs;
    (void)PicSizeInMbs;
    int i = n + 1;
    //while( i < PicSizeInMbs  &&  MbToSliceGroupMap[ i ]  !=  MbToSliceGroupMap[ n ] )
    //    i++;
    return i;
}

void SliceHeader::setFrameNum(int frameNum)
{
    NX_ASSERT(m_frameNumBitPos != 0);
    updateBits(m_frameNumBitPos, m_frameNumBits, frameNum);
    if (m_picOrderBitPos > 0)
        updateBits(m_picOrderBitPos, m_picOrderNumBits, frameNum*2 + bottom_field_flag);
}

int SliceHeader::deserializeSliceData()
{
    if (nal_unit_type != NALUnitType::nuSliceIDR && nal_unit_type != NALUnitType::nuSliceNonIDR)
        return 0; // deserialize other nal types are not supported now
    if( pps->entropy_coding_mode_flag) {
        int bitOffs = bitReader.getBitsCount() % 8;
        if (bitOffs > 0)
            bitReader.skipBits(8 - bitOffs);
    }
    int MbaffFrameFlag = ( sps->mb_adaptive_frame_field_flag!=0  &&  m_field_pic_flag==0);
    int CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    int moreDataFlag = 1;
    int mb_skip_run = 0;
    //int mb_skip_flag = 0;
    int prevMbSkipped = 0;
    //int mb_field_decoding_flag = 0;
    do {
        if( slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE ){
            if( !pps->entropy_coding_mode_flag ) {
                mb_skip_run = bitReader.getGolomb(); // !!!!!!!!!!!!!
                prevMbSkipped = ( mb_skip_run > 0 );
                for(int i = 0; i<mb_skip_run; i++ )
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                moreDataFlag = bitReader.getBitsLeft() >= 8;
            }/* else {
                mb_skip_flag = extractCABAC();
                moreDataFlag = mb_skip_flag == 0;
            }*/
        }
        if( moreDataFlag ) {
            if( MbaffFrameFlag && ( CurrMbAddr % 2  ==  0  ||
                ( CurrMbAddr % 2  ==  1  &&  prevMbSkipped ) ) ) {
                    //mb_field_decoding_flag = bitReader.getBit(); // || ae(v) for CABAC
                    bitReader.skipBit();
            }
            macroblock_layer();
        }
        if( !pps->entropy_coding_mode_flag )
            moreDataFlag = bitReader.getBitsLeft() >= 8; //more_rbsp_data();
        /*else {

            if( slice_type  !=  I  &&  slice_type  !=  SI )
                prevMbSkipped = mb_skip_flag
            if( MbaffFrameFlag  &&  CurrMbAddr % 2  ==  0 )
                moreDataFlag = 1
            else {
                end_of_slice_flag
                moreDataFlag = !end_of_slice_flag
            }

        }*/
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag);
    return 0;
}

void SliceHeader::macroblock_layer()
{
}

int SliceHeader::serializeSliceHeader(nx::utils::BitStreamWriter& bitWriter, const QMap<quint32, SequenceParameterSet*>& spsMap,
                                    const QMap<quint32, PictureParameterSet*>& ppsMap, quint8* dstBuffer, int dstBufferLen)
{
    try
    {
        dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = 0;
        dstBuffer[3] = 1;
        dstBuffer[4] = (nal_ref_idc << 5) + nal_unit_type;
        bitWriter.setBuffer(dstBuffer + 5, dstBuffer + dstBufferLen);
        bitReader.setBuffer(dstBuffer + 5, dstBuffer + dstBufferLen);
        bitWriter.putGolomb(first_mb_in_slice);
        bitWriter.putGolomb(orig_slice_type);
        bitWriter.putGolomb(pic_parameter_set_id);
        QMap<quint32, PictureParameterSet*>::const_iterator itr = ppsMap.find(pic_parameter_set_id);
        if (itr == ppsMap.end())
            return SPS_OR_PPS_NOT_READY;
        pps = itr.value();

        QMap<quint32, SequenceParameterSet*>::const_iterator itr2 = spsMap.find(pps->seq_parameter_set_id);
        if (itr2 == spsMap.end())
            return SPS_OR_PPS_NOT_READY;
        sps = itr2.value();
        m_frameNumBitPos = bitWriter.getBitsCount(); //getBitContext.buffer
        m_frameNumBits = sps->log2_max_frame_num;
        bitWriter.putBits(m_frameNumBits, frame_num);
        if( sps->frame_mbs_only_flag == 0) {
            bitWriter.putBit(m_field_pic_flag);
            if( m_field_pic_flag )
                bitWriter.putBit(bottom_field_flag);
        }
        if(nal_unit_type == NALUnitType::nuSliceIDR)
            bitWriter.putGolomb(idr_pic_id);
        if( sps->pic_order_cnt_type ==  0)
        {
            m_picOrderBitPos = bitWriter.getBitsCount(); //getBitContext.buffer
            m_picOrderNumBits = sps->log2_max_pic_order_cnt_lsb;
            bitWriter.putBits( sps->log2_max_pic_order_cnt_lsb, pic_order_cnt_lsb);
            if( pps->pic_order_present_flag &&  !m_field_pic_flag)
                bitWriter.putGolomb(delta_pic_order_cnt_bottom);
        }
        NX_ASSERT (m_shortDeserializeMode == false);

        if(sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag)
        {
            bitWriter.putSignedGolomb(delta_pic_order_cnt[0]);
            if( pps->pic_order_present_flag && !m_field_pic_flag)
                bitWriter.putSignedGolomb(delta_pic_order_cnt[1]);
        }
        if(pps->redundant_pic_cnt_present_flag)
            bitWriter.putSignedGolomb(redundant_pic_cnt);

        if( slice_type  ==  B_TYPE ) {
            bitWriter.putBit(direct_spatial_mv_pred_flag);
        }
        if( slice_type == P_TYPE || slice_type == SP_TYPE || slice_type == B_TYPE )
        {
            bitWriter.putBit(num_ref_idx_active_override_flag);
            if( num_ref_idx_active_override_flag ) {
                bitWriter.putGolomb(num_ref_idx_l0_active_minus1);
                if( slice_type == B_TYPE)
                    bitWriter.putGolomb(num_ref_idx_l1_active_minus1);
            }
        }
        write_ref_pic_list_reordering(bitWriter);
        if( ( pps->weighted_pred_flag  &&  ( slice_type == P_TYPE  ||  slice_type == SP_TYPE ) )  ||
            ( pps->weighted_bipred_idc  ==  1  &&  slice_type  ==  B_TYPE ) )
            write_pred_weight_table(bitWriter);
        if( nal_ref_idc != 0 )
            write_dec_ref_pic_marking(bitWriter);
        // ------------------------

        if( pps->entropy_coding_mode_flag  &&  slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE ) {
            bitWriter.putGolomb(cabac_init_idc);
        }
        bitWriter.putSignedGolomb(slice_qp_delta);
        if( slice_type  ==  SP_TYPE  ||  slice_type  ==  SI_TYPE ) {
            if( slice_type  ==  SP_TYPE )
                bitWriter.putBit(sp_for_switch_flag);
            bitWriter.putSignedGolomb(slice_qs_delta);
        }
        if( pps->deblocking_filter_control_present_flag ) {
            bitWriter.putGolomb(disable_deblocking_filter_idc);
            if( disable_deblocking_filter_idc != 1 ) {
                bitWriter.putSignedGolomb(slice_alpha_c0_offset_div2);
                bitWriter.putSignedGolomb(slice_beta_offset_div2);
            }
        }
        if( pps->num_slice_groups_minus1 > 0  &&
            pps->slice_group_map_type >= 3  &&  pps->slice_group_map_type <= 5) {
            int bits = ceil_log2( sps->pic_size_in_map_units / (double)pps->slice_group_change_rate + 1 );
            bitWriter.putBits(bits, slice_group_change_cycle);
        }

        return 0;
    } catch(nx::utils::BitStreamException& e) {
        return NOT_ENOUGHT_BUFFER;
    }
}

void SliceHeader::write_dec_ref_pic_marking(nx::utils::BitStreamWriter& bitWriter)
{
    if(nal_unit_type == NALUnitType::nuSliceIDR)
    {
        bitWriter.putBit(no_output_of_prior_pics_flag);
        bitWriter.putBit(long_term_reference_flag);
    }
    else
    {
        bitWriter.putBit(adaptive_ref_pic_marking_mode_flag);
        if( adaptive_ref_pic_marking_mode_flag )
            for (int i = 0; i < dec_ref_pic_vector.size(); i++)
                bitWriter.putGolomb(dec_ref_pic_vector[i]);
    }
}

void SliceHeader::write_pred_weight_table(nx::utils::BitStreamWriter& bitWriter)
{
    bitWriter.putGolomb(luma_log2_weight_denom);
    if( sps->chroma_format_idc  !=  0 )
        bitWriter.putGolomb(chroma_log2_weight_denom);
    for(int i = 0; i <= num_ref_idx_l0_active_minus1; i++ )
    {
        if (luma_weight_l0[i] != INT_MAX) {
            bitWriter.putBit(1);
            bitWriter.putSignedGolomb(luma_weight_l0[i]);
            bitWriter.putSignedGolomb(luma_offset_l0[i]);
        }
        else
            bitWriter.putBit(0);
        if ( sps->chroma_format_idc  !=  0 ) {
            if (chroma_weight_l0[i*2] != INT_MAX) {
                bitWriter.putBit(1);
                for(int j =0; j < 2; j++ ) {
                    bitWriter.putSignedGolomb(chroma_weight_l0[i*2+j]);
                    bitWriter.putSignedGolomb(chroma_offset_l0[i*2+j]);
                }
            }
            else {
                bitWriter.putBit(0);
            }
        }
    }

    if( slice_type  ==  B_TYPE )
        for(int i = 0; i <= num_ref_idx_l1_active_minus1; i++ ) {
            if (luma_weight_l1[i] != INT_MAX) {
                bitWriter.putBit(1);
                bitWriter.putSignedGolomb(luma_weight_l1[i]);
                bitWriter.putSignedGolomb(luma_offset_l1[i]);
            }
            else
                bitWriter.putBit(0);

            if( sps->chroma_format_idc  !=  0 ) {
                if (chroma_weight_l1[i*2] != INT_MAX) {
                    bitWriter.putBit(1);
                    for(int j = 0; j < 2; j++ ) {
                        bitWriter.putSignedGolomb(chroma_weight_l1[i*2+j]);
                        bitWriter.putSignedGolomb(chroma_offset_l1[i*2+j]);
                    }
                }
                else
                    bitWriter.putBit(0);
                // -------------
            }
        }
}

void SliceHeader::write_ref_pic_list_reordering(nx::utils::BitStreamWriter& bitWriter)
{

    if( slice_type != I_TYPE && slice_type !=  SI_TYPE ) {
        bitWriter.putBit(ref_pic_list_reordering_flag_l0);
        for (int i = 0; i < m_ref_pic_vect.size(); i++)
            bitWriter.putGolomb(m_ref_pic_vect[i]);
    }

    if( slice_type  ==  B_TYPE ) {
        bitWriter.putBit(ref_pic_list_reordering_flag_l1);
        if( ref_pic_list_reordering_flag_l1 )
            for (int i = 0; i < m_ref_pic_vect2.size(); i++)
                bitWriter.putGolomb(m_ref_pic_vect2[i]);
    }
}

} // namespace nx::media::h264
