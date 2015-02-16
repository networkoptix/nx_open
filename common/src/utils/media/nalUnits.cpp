#include "nalUnits.h"
#include <assert.h>
#include "bitStream.h"
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>

#ifdef _MSC_VER
#    pragma warning(disable: 4189) /* C4189: '?' : local variable is initialized but not referenced. */
#    pragma warning(disable: 4101) /* C4101: '?' : unreferenced local variable. */
#endif

static const int NAL_RESERVED_SPACE = 16;
static const double FRAME_RATE_EPS = 3e-5;

int NALUnit::calc_rbsp_trailing_bits_cnt(uint8_t val)
{
    int cnt = 1;
    while ((val & 1) == 0) {
        cnt++;
        val >>= 1;
        assert(val != 0);
        if (val == 0)
            return 0;
    }
    return cnt;
}


void NALUnit::write_rbsp_trailing_bits(BitStreamWriter& writer)
{
    writer.putBit(1);
    int rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBits(rest, 0);
}

void NALUnit::write_byte_align_bits(BitStreamWriter& writer)
{
    int rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBit(1);
    if (rest > 1)
        writer.putBits(rest-1, 0);
}

quint8* NALUnit::addStartCode(quint8* buffer, quint8* boundStart)
{
    quint8* rez = buffer;
    if (rez - 3 >= boundStart && rez[-1] == 1 && rez[-2] == 0 && rez[-3] == 0) {
        rez -=3;
        if (rez > boundStart && rez[-1] == 0)
            rez--;
    }
    return rez;
}

const quint8* NALUnit::findNextNAL(const quint8* buffer, const quint8* end)
{
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer ++;
        else if (buffer[-2] == 0 && buffer[-1] == 0)    //*buffer == 1
            return buffer+1;
        else
            buffer+=3;
    }
    return end;
}

const quint8* NALUnit::findNALWithStartCode(const quint8* buffer, const quint8* end, bool longCodesAllowed)
{
    const quint8* bufStart = buffer;
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer ++;
        else if (buffer[-2] == 0 && buffer[-1] == 0) {
            if (longCodesAllowed && buffer-3 >= bufStart && buffer[-3] == 0)
                return buffer - 3;
            else
                return buffer-2;
        }
        else
            buffer+=3;
    }
    return end;
}

const quint8* NALUnit::findNALWithStartCodeEx(
    const quint8* buffer,
    const quint8* end,
    const quint8** startCodePrefix )
{
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer ++;
        else if (buffer[-2] == 0 && buffer[-1] == 0)    //*buffer == 1
        {
            if( startCodePrefix )
                *startCodePrefix = buffer-2;
            return buffer+1;
        }
        else
            buffer+=3;
    }
    if( startCodePrefix )
        *startCodePrefix = end;
    return end;
}

int NALUnit::encodeNAL(quint8* dstBuffer, size_t dstBufferSize)
{
    return encodeNAL(m_nalBuffer, m_nalBuffer+m_nalBufferLen, dstBuffer, dstBufferSize);
}

int NALUnit::encodeNAL(quint8* srcBuffer, quint8* srcEnd, quint8* dstBuffer, size_t dstBufferSize)
{
    quint8* srcStart = srcBuffer;
    quint8* initDstBuffer = dstBuffer;
    for (srcBuffer += 2; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 3;
        else if (srcBuffer[-2] == 0 && srcBuffer[-1] == 0) {
            if (dstBufferSize < (size_t) (srcBuffer - srcStart + 2))
                return -1;
            memcpy(dstBuffer, srcStart, srcBuffer - srcStart);
            dstBuffer += srcBuffer - srcStart;
            dstBufferSize -= srcBuffer - srcStart + 2;
            *dstBuffer++ = 3;
            *dstBuffer++ = *srcBuffer++;
            for (int k = 0; k < 1; k++)
                if (srcBuffer < srcEnd) {
                    if (dstBufferSize < 1)
                        return -1;
                    *dstBuffer++ = *srcBuffer++;
                    dstBufferSize--;
                }
            srcStart = srcBuffer;
        }
        else
            srcBuffer++;
    }
    
    Q_ASSERT(srcEnd >= srcStart);
    //conversion to uint possible cause srcEnd should always be greater then srcStart
    if (dstBufferSize < uint(srcEnd - srcStart)) 
        return -1;
    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

int NALUnit::decodeNAL(const quint8* srcBuffer, const quint8* srcEnd, quint8* dstBuffer, size_t dstBufferSize)
{
    quint8* initDstBuffer = dstBuffer;
    const quint8* srcStart = srcBuffer;
    for (srcBuffer += 3; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 4;
        /*
        else if (*srcBuffer == 3) {
            if (srcBuffer[-1] == 3 && srcBuffer[-2] == 0 && srcBuffer[-3] == 0)
            srcBuffer++;
        }
        */
        else if (srcBuffer[-3] == 0 && srcBuffer[-2] == 0  && srcBuffer[-1] == 3) {
            if (dstBufferSize < (size_t) (srcBuffer - srcStart))
                return -1;
            memcpy(dstBuffer, srcStart, srcBuffer - srcStart - 1);
            dstBuffer += srcBuffer - srcStart - 1;
            dstBufferSize -= srcBuffer - srcStart;
            *dstBuffer++ = *srcBuffer++;
            srcStart = srcBuffer;
        }
        else 
            srcBuffer++;
    }
    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

QByteArray NALUnit::decodeNAL( const QByteArray& srcBuf )
{
    QByteArray decodedStreamBuf;
    decodedStreamBuf.resize( srcBuf.size() );
    decodedStreamBuf.resize( NALUnit::decodeNAL(
        (const quint8*)srcBuf.constData(), (const quint8*)srcBuf.constData() + srcBuf.size(),
        (quint8*)decodedStreamBuf.data(), decodedStreamBuf.size() ) );
    return decodedStreamBuf;
}

int NALUnit::extractUEGolombCode()
{
    uint cnt = 0;
    for ( ; bitReader.getBits(1) == 0; cnt++) {}
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    return (1 << cnt)-1 + bitReader.getBits(cnt);
}

void NALUnit::writeSEGolombCode(BitStreamWriter& bitWriter, qint32 value)
{
    if (value <= 0)
        writeUEGolombCode(bitWriter, -value * 2);
    else
        writeUEGolombCode(bitWriter, value*2-1);
}

void NALUnit::writeUEGolombCode(BitStreamWriter& bitWriter, quint32 value)
{
    /*
    int maxVal = 0;
    int x = 2;
    int nBit = 0;
    for (; maxVal < value; maxVal += x ) {
        x <<= 1;
        nBit++;
    }
    */
    unsigned maxVal = 0;
    int x = 1;
    int nBit = 0;
    for ( ; maxVal < value; maxVal += x) {
        x <<= 1;
        nBit++;
    }

    bitWriter.putBits(nBit+1, 1);
    bitWriter.putBits(nBit, value - (x-1));
}


int NALUnit::extractUEGolombCode(BitStreamReader& bitReader)
{
    int cnt = 0;
    for ( ; bitReader.getBits(1) == 0; cnt++) {}
    return (1 << cnt)-1 + bitReader.getBits(cnt);
}

int NALUnit::extractSEGolombCode()
{
    int rez = extractUEGolombCode();
    if (rez %2 == 0)
        return -rez/2;
    else
        return (rez+1) / 2;
}

int NALUnit::deserialize(quint8* buffer, quint8* end)
{
    if (end == buffer)
        return NOT_ENOUGHT_BUFFER;


    //assert((*buffer & 0x80) == 0);
    if ((*buffer & 0x80) != 0) {
        qWarning() << "Invalid forbidden_zero_bit for nal unit " << (*buffer & 0x1f);
    }

    nal_ref_idc   = (*buffer >> 5) & 0x3;
    nal_unit_type = *buffer & 0x1f;
    return 0;
}

void NALUnit::decodeBuffer(const quint8* buffer, const quint8* end)
{
    delete [] m_nalBuffer;
    m_nalBuffer = new quint8[end - buffer + NAL_RESERVED_SPACE];
    m_nalBufferLen = decodeNAL(buffer, end, m_nalBuffer, end - buffer);
}

int NALUnit::serializeBuffer(quint8* dstBuffer, quint8* dstEnd, bool writeStartCode) const
{
    if (m_nalBufferLen == 0)
        return 0;
    if (writeStartCode) 
    {
        if (dstEnd - dstBuffer < 4)
            return -1;
        *dstBuffer++ = 0;
        *dstBuffer++ = 0;
        *dstBuffer++ = 0;
        *dstBuffer++ = 1;
    }
    int encodeRez = NALUnit::encodeNAL(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    else
        return encodeRez + (writeStartCode ? 4 : 0);
}

int NALUnit::serialize(quint8* dstBuffer)
{
    *dstBuffer++ = 0;
    *dstBuffer++ = 0;
    *dstBuffer++ = 1;
    *dstBuffer = ((nal_ref_idc & 3) << 5) + (nal_unit_type & 0x1f);
    return 4;
}

void NALUnit::scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag)
{
    int lastScale = 8;
    int nextScale = 8;
    for(int j = 0; j < sizeOfScalingList; j++ )
    {
        if( nextScale != 0 ) {
            int delta_scale = extractSEGolombCode();
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j  ==  0 && nextScale  ==  0 );
        }
        scalingList[j] = ( nextScale  ==  0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}


int ceil_log2(double val)
{
    int iVal = (int) val;
    double frac = val - iVal;
    int bits = 0;
    for(;iVal > 0; iVal>>=1) {
        bits++;
    }
    int mask = 1 << (bits-1);
    iVal = (int) val;
    if (iVal - mask == 0 && frac == 0)
        return bits-1; // For example: cail(log2(8.0)) = 3, but for 8.2 or 9.0 is 4
    else
        return bits;
}

// -------------------- NALDelimiter ------------------
int NALDelimiter::deserialize(quint8* buffer, quint8* end)
{
    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;
    if (end - buffer < 2)
        return NOT_ENOUGHT_BUFFER;
    primary_pic_type = buffer[1] >> 5;
    return 0;
}

int NALDelimiter::serialize(quint8* buffer)
{
    quint8* curBuf = buffer;
    curBuf +=  NALUnit::serialize(curBuf);
    *curBuf++ = (primary_pic_type << 5) + 0x10;
    return (int) (curBuf - buffer);
}

PPSUnit::PPSUnit()
:
    NALUnit(),
    transform_8x8_mode_flag(0),
    pic_scaling_matrix_present_flag(0),
    second_chroma_qp_index_offset(0),
    m_ready(false)
{
    memset( scalingLists4x4, 0x10, sizeof(scalingLists4x4) );
    memset( useDefaultScalingMatrix4x4Flag, 0, sizeof(useDefaultScalingMatrix4x4Flag) );
    memset( scalingLists8x8, 0x10, sizeof(scalingLists8x8) );
    memset( useDefaultScalingMatrix8x8Flag, 0, sizeof(useDefaultScalingMatrix8x8Flag) );
}

int PPSUnit::deserializeID(quint8* buffer, quint8* end)
{
    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;
    if (end - buffer < 2)
        return NOT_ENOUGHT_BUFFER;
    bitReader.setBuffer(buffer + 1, end);
    return 0;
    try {
        pic_parameter_set_id = extractUEGolombCode();
    }
    catch(BitStreamException) {
        return NOT_ENOUGHT_BUFFER;
    }
}

// -------------------- PPSUnit --------------------------
int PPSUnit::deserialize()
{
    quint8* nalEnd = m_nalBuffer + m_nalBufferLen;
    int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
    if (rez != 0)
        return rez;
    if (nalEnd - m_nalBuffer < 2)
        return NOT_ENOUGHT_BUFFER;
    try {
        bitReader.setBuffer(m_nalBuffer + 1, nalEnd);
        pic_parameter_set_id = extractUEGolombCode();
        seq_parameter_set_id = extractUEGolombCode();
        entropy_coding_mode_BitPos = bitReader.getBitsCount();
        entropy_coding_mode_flag = bitReader.getBit();
        pic_order_present_flag = bitReader.getBit();
        num_slice_groups_minus1 = extractUEGolombCode();
        slice_group_map_type = 0;
        if( num_slice_groups_minus1 > 0 ) {
            slice_group_map_type = extractUEGolombCode();
            if( slice_group_map_type  ==  0 ) 
            {
                if (num_slice_groups_minus1 >= 256)
                    THROW_BITSTREAM_ERR;
                for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
                    run_length_minus1[iGroup] = extractUEGolombCode();
            }
            else if( slice_group_map_type  ==  2) 
            {
                if (num_slice_groups_minus1 >= 256)
                    THROW_BITSTREAM_ERR;
                for( int iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {
                    top_left[ iGroup ] = extractUEGolombCode();
                    bottom_right[ iGroup ] = extractUEGolombCode();
                }
            }
            else if(  slice_group_map_type  ==  3  ||  
                        slice_group_map_type  ==  4  ||  
                        slice_group_map_type  ==  5 ) 
            {
                slice_group_change_direction_flag = bitReader.getBits(1);
                slice_group_change_rate = extractUEGolombCode() + 1;
            } else if( slice_group_map_type  ==  6 ) 
            {
                int pic_size_in_map_units_minus1 = extractUEGolombCode();
                if (pic_size_in_map_units_minus1 >= 256)
                    THROW_BITSTREAM_ERR;
                for( int i = 0; i <= pic_size_in_map_units_minus1; i++ ) {
                    int bits = ceil_log2( num_slice_groups_minus1 + 1 );
                    Q_UNUSED(bits);
                    slice_group_id[i] = bitReader.getBits(1);
                }
            }
        }
        num_ref_idx_l0_active_minus1 = extractUEGolombCode();
        num_ref_idx_l1_active_minus1 = extractUEGolombCode();
        weighted_pred_flag = bitReader.getBit();
        weighted_bipred_idc = bitReader.getBits(2);
        pic_init_qp_minus26 = extractSEGolombCode();  // relative to 26 
        pic_init_qs_minus26 = extractSEGolombCode();  // relative to 26 
        chroma_qp_index_offset =  extractSEGolombCode();
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
            second_chroma_qp_index_offset = extractSEGolombCode();
        }

        m_ready = true;
        return 0;
    } catch (BitStreamException) {
        return NOT_ENOUGHT_BUFFER;
    }
}

void PPSUnit::duplicatePPS(PPSUnit& oldPPS, int ppsID, bool cabac)
{
    delete m_nalBuffer;
    memcpy(this, &oldPPS, sizeof(PPSUnit));
    m_nalBuffer = new quint8[oldPPS.m_nalBufferLen + 400]; // 4 bytes reserved for new ppsID and cabac values
    m_nalBuffer[0] = oldPPS.m_nalBuffer[0];
    
    pic_parameter_set_id = ppsID;
    entropy_coding_mode_flag = cabac;

    BitStreamWriter bitWriter;
    bitWriter.setBuffer(m_nalBuffer + 1, m_nalBuffer + m_nalBufferLen + 4);
    writeUEGolombCode(bitWriter, ppsID);
    writeUEGolombCode(bitWriter, seq_parameter_set_id);
    bitWriter.putBit(cabac);
    bitReader.setBuffer(oldPPS.m_nalBuffer+1, oldPPS.m_nalBuffer + oldPPS.m_nalBufferLen);
    extractUEGolombCode();
    extractUEGolombCode();
    bitReader.skipBit(); // skip cabac field
    int bitsToCopy = oldPPS.m_ppsLenInMbit - bitReader.getBitsCount();
    for (; bitsToCopy >=32; bitsToCopy -=32) {
        quint32 value = bitReader.getBits(32);
        bitWriter.putBits(32, value);
    }
    if (bitsToCopy > 0) {
        quint32 value = bitReader.getBits(bitsToCopy);
        bitWriter.putBits(bitsToCopy, value);
    }
    if (bitWriter.getBitsCount() % 8 != 0)
        bitWriter.putBits(8 - bitWriter.getBitsCount() % 8, 0);
    m_nalBufferLen = bitWriter.getBitsCount() / 8 + 1;
    assert(m_nalBufferLen <= oldPPS.m_nalBufferLen + 4);
}


// -------------------- SPSUnit --------------------------
int SPSUnit::deserialize()
{
    /*
    quint8* nextNal = findNALWithStartCode(buffer, end);
    long bufSize = nextNal - buffer;
    quint8* tmpBuff = new quint8[bufSize];
    quint8* tmpBufferEnd = tmpBuff + bufSize;
    int m_decodedBuffSize = decodeNAL(buffer, nextNal, tmpBuff, bufSize);
    */
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
        seq_parameter_set_id = extractUEGolombCode();
        pic_order_cnt_type = 0;
        if( profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc  ==  144) 
        {
            chroma_format_idc = extractUEGolombCode();
            if(chroma_format_idc == 3)
                residual_colour_transform_flag = bitReader.getBits(1);
            bit_depth_luma = extractUEGolombCode() + 8;
            bit_depth_chroma = extractUEGolombCode() + 8;
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
        log2_max_frame_num = extractUEGolombCode() + 4;

        // next parameters not used  now.
        
        pic_order_cnt_type = extractUEGolombCode();
        log2_max_pic_order_cnt_lsb = 0;
        delta_pic_order_always_zero_flag = 0;
        if( pic_order_cnt_type == 0)
            log2_max_pic_order_cnt_lsb = extractUEGolombCode() + 4;
        else if( pic_order_cnt_type == 1 ) 
        {
            delta_pic_order_always_zero_flag = bitReader.getBits(1);
            offset_for_non_ref_pic = extractSEGolombCode();
            offset_for_top_to_bottom_field = extractSEGolombCode();
            num_ref_frames_in_pic_order_cnt_cycle = extractUEGolombCode();
            if (num_ref_frames_in_pic_order_cnt_cycle >= 256) {
                THROW_BITSTREAM_ERR;
            }

            for(int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
                offset_for_ref_frame[i] = extractSEGolombCode();
        }

        num_ref_frames = extractUEGolombCode();
        gaps_in_frame_num_value_allowed_flag = bitReader.getBits(1);
        pic_width_in_mbs = extractUEGolombCode() + 1;
        pic_height_in_map_units = extractUEGolombCode() + 1;
        frame_mbs_only_flag = bitReader.getBits(1);
        if(!frame_mbs_only_flag)
            mb_adaptive_frame_field_flag = bitReader.getBits(1);
        direct_8x8_inference_flag = bitReader.getBits(1);
        frame_cropping_flag = bitReader.getBits(1);
        if( frame_cropping_flag ) {
            frame_crop_left_offset = extractUEGolombCode();
            frame_crop_right_offset = extractUEGolombCode();
            frame_crop_top_offset = extractUEGolombCode();
            frame_crop_bottom_offset = extractUEGolombCode();
        }
        pic_size_in_map_units = pic_width_in_mbs * pic_height_in_map_units; // * (2 - frame_mbs_only_flag);
        vui_parameters_present_flag = bitReader.getBits(1);
        if( vui_parameters_present_flag ) {
            deserializeVuiParameters();
        }
        m_ready = true;
        full_sps_bit_len = bitReader.getBitsCount() + 32;

        //m_pulldown = ((abs(getFPS() - 23.97) < FRAME_RATE_EPS) || (abs(getFPS() - 59.94) < FRAME_RATE_EPS)) && pic_struct_present_flag;

        return 0;
    } catch (BitStreamException) {
        return NOT_ENOUGHT_BUFFER;
    }
}


void SPSUnit::deserializeVuiParameters()
{
    aspect_ratio_info_present_flag = bitReader.getBit();
    if( aspect_ratio_info_present_flag ) {
        aspect_ratio_idc = bitReader.getBits(8);
        if( aspect_ratio_idc  ==  Extended_SAR ) 
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
        chroma_sample_loc_type_top_field = extractUEGolombCode();
        chroma_sample_loc_type_bottom_field = extractUEGolombCode();
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
        max_bytes_per_pic_denom = extractUEGolombCode();
        max_bits_per_mb_denom = extractUEGolombCode();
        log2_max_mv_length_horizontal = extractUEGolombCode();
        log2_max_mv_length_vertical = extractUEGolombCode();
        num_reorder_frames = extractUEGolombCode();
        max_dec_frame_buffering = extractUEGolombCode();
    }
}

void SPSUnit::serializeHDRParameters(BitStreamWriter& writer)
{
    writeUEGolombCode(writer, cpb_cnt_minus1);
    writer.putBits(4, bit_rate_scale);
    writer.putBits(4, cpb_size_scale);
    for(int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {
        writeUEGolombCode(writer, bit_rate_value_minus1[ SchedSelIdx]);
        writeUEGolombCode(writer, cpb_size_value_minus1[ SchedSelIdx ]);
        writer.putBit(cbr_flag[ SchedSelIdx ]);
    }
    writer.putBits(5, initial_cpb_removal_delay_length_minus1);
    writer.putBits(5, cpb_removal_delay_length_minus1);
    writer.putBits(5, dpb_output_delay_length_minus1);
    writer.putBits(5, time_offset_length);
    //writer.flushBits();
}

// insert HDR parameters to bitstream.
void SPSUnit::insertHdrParameters()
{
    if (nal_hrd_parameters_present_flag)
        return; // replace hrd parameters not implemented. only insert
    nal_hrd_parameters_present_flag = 1;
    quint8* newNalBuffer = new quint8[m_nalBufferLen + 16];
    int beforeBytes = nal_hrd_parameters_bit_pos >> 3;
    memcpy(newNalBuffer, m_nalBuffer, beforeBytes);
    BitStreamReader reader;
    reader.setBuffer(m_nalBuffer + beforeBytes, m_nalBuffer + m_nalBufferLen);
    BitStreamWriter writer;
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
    write_rbsp_trailing_bits(writer);
    writer.flushBits();
    delete [] m_nalBuffer;
    m_nalBuffer = newNalBuffer;
    m_nalBufferLen =  writer.getBitsCount()/8 + beforeBytes;
}


int SPSUnit::getMaxBitrate()
{
    if (bit_rate_value_minus1 == 0)
        return 0;
    else
        return (bit_rate_value_minus1[0]+1) << (6 + bit_rate_scale);
}

void SPSUnit::hrd_parameters() 
{
    cpb_cnt_minus1 = extractUEGolombCode();
    bit_rate_scale = bitReader.getBits(4);
    cpb_size_scale = bitReader.getBits(4);

    if( bit_rate_value_minus1 )
        delete[] bit_rate_value_minus1;
    bit_rate_value_minus1 = new int[cpb_cnt_minus1 + 1];
    if( cpb_size_value_minus1 )
        delete[] cpb_size_value_minus1;
    cpb_size_value_minus1 = new int[cpb_cnt_minus1 + 1];
    if( cbr_flag )
        delete[] cbr_flag;
    cbr_flag = new quint8[cpb_cnt_minus1 + 1];

    for( int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++) 
    {
        bit_rate_value_minus1[ SchedSelIdx ] = extractUEGolombCode();
        cpb_size_value_minus1[ SchedSelIdx ] = extractUEGolombCode();
        cbr_flag[ SchedSelIdx ] = bitReader.getBit();
    }
    initial_cpb_removal_delay_length_minus1 = bitReader.getBits(5);
    cpb_removal_delay_length_minus1 = bitReader.getBits(5);
    dpb_output_delay_length_minus1 = bitReader.getBits(5);
    time_offset_length = bitReader.getBits(5);
}

int SPSUnit::getCropY() const {
    if (chroma_format_idc == 0) 
        return (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
    else {
        int SubHeightC = 1;
        if (chroma_format_idc == 1)
            SubHeightC = 2;
        return SubHeightC * (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
    }
}

int SPSUnit::getCropX() const {
    if (chroma_format_idc == 0) 
        return frame_crop_left_offset + frame_crop_right_offset;
    else {
        int SubWidthC = 1;
        if (chroma_format_idc == 1 || chroma_format_idc == 2)
            SubWidthC = 2;
        return SubWidthC * (frame_crop_left_offset + frame_crop_right_offset);
    }
}

double SPSUnit::getFPS() const
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

void SPSUnit::setFps(double fps) 
{
    time_scale = (quint32)(fps+0.5) * 1000000;
    //time_scale = (quint32)(fps+0.5) * 1000;
    num_units_in_tick = time_scale / fps + 0.5;
    time_scale *= 2;

    //num_units_in_tick = time_scale/2 / fps;
    assert(num_units_in_tick_bit_pos > 0);
    updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
    updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
}

QString SPSUnit::getStreamDescr()
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

// --------------------- SliceUnit -----------------------

SliceUnit::SliceUnit():NALUnit() {
    m_frameNumBitPos = 0;
    m_field_pic_flag = 0;
#ifndef __TS_MUXER_COMPILE_MODE
    m_shortDeserializeMode = true;
#endif
    memory_management_control_operation = 0;
    m_fullHeaderLen = 0;
    disable_deblocking_filter_idc = 0;
}

int SliceUnit::deserialize(const SPSUnit* sps,const PPSUnit* pps)
{
    QMap<quint32, const SPSUnit*> spsMap;
    QMap<quint32, const PPSUnit*> ppsMap;
    spsMap.insert(0, sps);
    ppsMap.insert(0, pps);
    return deserialize(m_nalBuffer, m_nalBuffer + m_nalBufferLen, spsMap, ppsMap);
}


int SliceUnit::deserialize(quint8* buffer, quint8* end, 
                            const QMap<quint32, const SPSUnit*>& spsMap,
                            const QMap<quint32, const PPSUnit*>& ppsMap)
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
    rez = deserializeSliceHeader(spsMap, ppsMap);
    if (rez != 0 || m_shortDeserializeMode)
        return rez;

    if (nal_unit_type == nuSliceA ||  nal_unit_type == nuSliceB || nal_unit_type == nuSliceC)
    {
        /*int slice_id =*/ extractUEGolombCode();
        if (nal_unit_type == nuSliceB || nal_unit_type == nuSliceC)
            if( pps->redundant_pic_cnt_present_flag )
                /*int redundant_pic_cnt =*/ extractUEGolombCode();
    }
    m_fullHeaderLen = bitReader.getBitsCount() + 8;
    return 0;
    //return deserializeSliceData();
}

/*
int SliceUnit::extractCABAC()
{
}
*/
bool SliceUnit::moveHeaderField(int fieldOffset, int newLen, int oldLen)
{
    int bitDiff = newLen - oldLen;
    if (bitDiff > NAL_RESERVED_SPACE*8)
        return false;


    assert(bitDiff >= 0);
    if (bitDiff > 0)
    {
        if (pps->entropy_coding_mode_flag)
        {
            int oldHeaderLenInBytes = m_fullHeaderLen / 8;
            if (m_fullHeaderLen % 8 != 0)
                oldHeaderLenInBytes++;
            int newHeaderLenInBits = m_fullHeaderLen + bitDiff;
            int newHeaderLenInBytes = newHeaderLenInBits / 8;
            if (newHeaderLenInBits %8 != 0)
                newHeaderLenInBytes++;
            if (newHeaderLenInBytes > oldHeaderLenInBytes)
            {
                uint8_t* sliceData = m_nalBuffer + oldHeaderLenInBytes;
                memmove(sliceData + newHeaderLenInBytes - oldHeaderLenInBytes, 
                    sliceData, m_nalBufferLen - oldHeaderLenInBytes);
            }
            moveBits(m_nalBuffer, fieldOffset, fieldOffset + bitDiff, m_fullHeaderLen-fieldOffset);

            m_nalBufferLen += newHeaderLenInBytes - oldHeaderLenInBytes;
            if (newHeaderLenInBits % 8 != 0)
            {
                int bitRest = 8 - (newHeaderLenInBits % 8);
                int mask = 0;
                for (int i = 0; i < bitRest; ++i) 
                    mask = (mask << 1) + 1;
                m_nalBuffer[newHeaderLenInBytes-1] |= mask; // add padding cabac_alignment_one_bit 
            }
        }
        else 
        {
            int oldLenInBits = m_nalBufferLen * 8;
            int trailingBits = calc_rbsp_trailing_bits_cnt(m_nalBuffer[m_nalBufferLen-1]);
            oldLenInBits -= trailingBits;
            int newLenInBits = oldLenInBits + bitDiff;
            moveBits(m_nalBuffer, fieldOffset, fieldOffset+bitDiff, oldLenInBits);
            m_nalBufferLen = newLenInBits / 8;
            if (newLenInBits % 8 == 0) 
                m_nalBuffer[m_nalBufferLen] = 0x80; // trailing bits
            else {
                quint8 mask = 1;
                int bitRest = 8 - (newLenInBits % 8);
                mask <<= bitRest - 1;
                m_nalBuffer[m_nalBufferLen] &= ~(mask-1);
                m_nalBuffer[m_nalBufferLen] |= mask;
            }
            m_nalBufferLen++;
        }
    }
    updateBits(fieldOffset-8, bitDiff, 0); // reader does not include first NAL bytes, so -8 bits
    m_fullHeaderLen += bitDiff; 
    return true;
}

/*
bool SliceUnit::increasePicOrderFieldLen(int newLen, int oldLen)
{
    int bitDiff = newLen - oldLen;
    if (bitDiff > NAL_RESERVED_SPACE*8)
        return false;

    quint8* sliceHeaderBuf = m_nalBuffer+1;

    int picOrderBitPos = m_picOrderBitPos + 8; // original field does not inculde nal code byte

    assert(bitDiff >= 0);
    if (bitDiff > 0)
    {
        if (pps->entropy_coding_mode_flag)
        {
            int oldHeaderLenInBytes = m_fullHeaderLen / 8;
            if (m_fullHeaderLen % 8 != 0)
                oldHeaderLenInBytes++;
            int newHeaderLenInBits = m_fullHeaderLen + bitDiff;
            int newHeaderLenInBytes = newHeaderLenInBits / 8;
            if (newHeaderLenInBits %8 != 0)
                newHeaderLenInBytes++;
            if (newHeaderLenInBytes > oldHeaderLenInBytes)
            {
                uint8_t* sliceData = m_nalBuffer + oldHeaderLenInBytes;
                memmove(sliceData + newHeaderLenInBytes - oldHeaderLenInBytes, 
                        sliceData, m_nalBufferLen - oldHeaderLenInBytes);
            }
            moveBits(m_nalBuffer, picOrderBitPos, picOrderBitPos + bitDiff, m_fullHeaderLen-picOrderBitPos);

            m_nalBufferLen += newHeaderLenInBytes - oldHeaderLenInBytes;
            if (newHeaderLenInBits % 8 != 0)
            {
                int bitRest = 8 - (newHeaderLenInBits % 8);
                int mask = 0;
                for (int i = 0; i < bitRest; ++i) 
                    mask = (mask << 1) + 1;
                m_nalBuffer[newHeaderLenInBytes-1] |= mask; // add padding cabac_alignment_one_bit 
            }
        }
        else 
        {
            int oldLenInBits = m_nalBufferLen * 8;
            int trailingBits = calc_rbsp_trailing_bits_cnt(m_nalBuffer[m_nalBufferLen-1]);
            oldLenInBits -= trailingBits;
            int newLenInBits = oldLenInBits + bitDiff;
            moveBits(m_nalBuffer, picOrderBitPos, picOrderBitPos+bitDiff, oldLenInBits);
            m_nalBufferLen = newLenInBits / 8;
            if (newLenInBits % 8 == 0) 
                m_nalBuffer[m_nalBufferLen] = 0x80; // trailing bits
            else {
                int mask = 1;
                int bitRest = 8 - (newLenInBits % 8);
                mask <<= bitRest - 1;
                m_nalBuffer[m_nalBufferLen] &= mask;
                m_nalBuffer[m_nalBufferLen] |= mask;
            }
            m_nalBufferLen++;
            BitStreamWriter writer;
            uint8_t* dst = m_nalBuffer + newLenInBits / 8;
            writer.setBuffer(dst, dst + 1);
            writer.skipBits(newLenInBits % 8);
            write_rbsp_trailing_bits(writer);
            writer.flushBits();
        }
    }
    updateBits(m_picOrderBitPos, bitDiff, 0);
    m_fullHeaderLen += bitDiff; 
}
*/

void NALUnit::updateBits(int bitOffset, int bitLen, int value)
{
    //quint8* ptr = m_getbitContextBuffer + (bitOffset/8);
    quint8* ptr = (quint8*) bitReader.getBuffer() + bitOffset/8;
    BitStreamWriter bitWriter;
    int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));
    
    quint8* ptr_end = (quint8*) bitReader.getBuffer() + (bitOffset + bitLen)/8;
    int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0) {
        int prefix = *ptr >> (8-byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8) {
        int postfix = *ptr_end & ( ( 1 << endBitsPostfix) - 1);
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}


int SliceUnit::deserializeSliceHeader(const QMap<quint32, const SPSUnit*>& spsMap,
                                      const QMap<quint32, const PPSUnit*>& ppsMap)
{
    try {
        first_mb_in_slice = extractUEGolombCode();
        orig_slice_type = slice_type = extractUEGolombCode();
        if (slice_type >= 5)
            slice_type -= 5; // +5 flag is: all other slice at this picture must be same type
        pic_parameter_set_id = extractUEGolombCode();
        QMap<quint32, const PPSUnit*>::const_iterator itr = ppsMap.find(pic_parameter_set_id);
        if (itr == ppsMap.end())
            return SPS_OR_PPS_NOT_READY;
        pps = itr.value();

        QMap<quint32, const SPSUnit*>::const_iterator itr2 = spsMap.find(pps->seq_parameter_set_id);
        if (itr2 == spsMap.end())
            return SPS_OR_PPS_NOT_READY;
        sps = itr2.value();

        m_frameNumBitPos = bitReader.getBitsCount(); //getBitContext.buffer
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
            idr_pic_id = extractUEGolombCode();
        m_picOrderBitPos = -1;
        if( sps->pic_order_cnt_type ==  0) 
        {
            m_picOrderBitPos = bitReader.getBitsCount(); //getBitContext.buffer
            m_picOrderNumBits = sps->log2_max_pic_order_cnt_lsb;
            pic_order_cnt_lsb = bitReader.getBits( sps->log2_max_pic_order_cnt_lsb);
            if( pps->pic_order_present_flag &&  !m_field_pic_flag)
                delta_pic_order_cnt_bottom = extractSEGolombCode();
        }
#ifndef __TS_MUXER_COMPILE_MODE
        if (m_shortDeserializeMode)
            return 0;

        if(sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) 
        {
            delta_pic_order_cnt[0] = extractSEGolombCode();
            if( pps->pic_order_present_flag && !m_field_pic_flag)
                delta_pic_order_cnt[1] = extractSEGolombCode();
        }
        if(pps->redundant_pic_cnt_present_flag)
            redundant_pic_cnt = extractUEGolombCode();


        if( slice_type  ==  B_TYPE )
            direct_spatial_mv_pred_flag = bitReader.getBit();
        num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_active_minus1;
        num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_active_minus1;
        if( slice_type == P_TYPE || slice_type == SP_TYPE || slice_type == B_TYPE ) 
        {
            num_ref_idx_active_override_flag = bitReader.getBit();
            if( num_ref_idx_active_override_flag ) {
                num_ref_idx_l0_active_minus1 = extractUEGolombCode();
                if( slice_type == B_TYPE)
                    num_ref_idx_l1_active_minus1 = extractUEGolombCode();
            }
        }
        ref_pic_list_reordering();
        if( ( pps->weighted_pred_flag  &&  ( slice_type == P_TYPE  ||  slice_type == SP_TYPE ) )  ||
            ( pps->weighted_bipred_idc  ==  1  &&  slice_type  ==  B_TYPE ) )
            pred_weight_table();
        if( nal_ref_idc != 0 )
            dec_ref_pic_marking();
        if( pps->entropy_coding_mode_flag  &&  slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE ) {
            cabac_init_idc = extractUEGolombCode();
            //assert(cabac_init_idc >=0 &&  cabac_init_idc <= 2);
        }
        slice_qp_delta = extractSEGolombCode();
        if( slice_type  ==  SP_TYPE  ||  slice_type  ==  SI_TYPE ) {
            if( slice_type  ==  SP_TYPE )
                sp_for_switch_flag = bitReader.getBits(1);
            slice_qs_delta = extractSEGolombCode();
        }
        if( pps->deblocking_filter_control_present_flag ) {
            disable_deblocking_filter_idc = extractUEGolombCode();
            if( disable_deblocking_filter_idc != 1 ) {
                slice_alpha_c0_offset_div2 = extractSEGolombCode();
                slice_beta_offset_div2 = extractSEGolombCode();
            }
        }
        if( pps->num_slice_groups_minus1 > 0  &&
            pps->slice_group_map_type >= 3  &&  pps->slice_group_map_type <= 5) {
            int bits = ceil_log2( sps->pic_size_in_map_units / (double)pps->slice_group_change_rate + 1 );
            slice_group_change_cycle = bitReader.getBits(bits);
        }

#endif
        return 0;
    } catch(BitStreamException) {
        return NOT_ENOUGHT_BUFFER;    
    }
}

#ifndef __TS_MUXER_COMPILE_MODE

void SliceUnit::pred_weight_table()
{
    luma_log2_weight_denom = extractUEGolombCode();
    if( sps->chroma_format_idc  !=  0 )
        chroma_log2_weight_denom = extractUEGolombCode();
    for(int i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) {
        int luma_weight_l0_flag = bitReader.getBit();
        if( luma_weight_l0_flag ) {
            luma_weight_l0.push_back(extractSEGolombCode());
            luma_offset_l0.push_back(extractSEGolombCode());
        }
        else {
            luma_weight_l0.push_back(INT_MAX);
            luma_offset_l0.push_back(INT_MAX);
        }
        if ( sps->chroma_format_idc  !=  0 ) {
            int chroma_weight_l0_flag = bitReader.getBit();
            if( chroma_weight_l0_flag ) {
                for(int j =0; j < 2; j++ ) {
                    chroma_weight_l0.push_back(extractSEGolombCode());
                    chroma_offset_l0.push_back(extractSEGolombCode());
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
                luma_weight_l1.push_back(extractSEGolombCode());
                luma_offset_l1.push_back(extractSEGolombCode());
            }
            else {
                luma_weight_l1.push_back(INT_MAX);
                luma_offset_l1.push_back(INT_MAX);
            }
            if( sps->chroma_format_idc  !=  0 ) {
                int chroma_weight_l1_flag = bitReader.getBit();
                if( chroma_weight_l1_flag ) {
                    for(int j = 0; j < 2; j++ ) {
                        chroma_weight_l1.push_back(extractSEGolombCode());
                        chroma_offset_l1.push_back(extractSEGolombCode());
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

void SliceUnit::dec_ref_pic_marking()
{
    if( nal_unit_type  ==  nuSliceIDR ) {
        no_output_of_prior_pics_flag = bitReader.getBits(1);
        long_term_reference_flag = bitReader.getBits(1);
    } else 
    {
        adaptive_ref_pic_marking_mode_flag =  bitReader.getBits(1);
        if( adaptive_ref_pic_marking_mode_flag )
            do {
                memory_management_control_operation = extractUEGolombCode();
                dec_ref_pic_vector.push_back(memory_management_control_operation);
                if (memory_management_control_operation != 0) {
                    quint32 tmp = extractUEGolombCode();
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

void SliceUnit::ref_pic_list_reordering()
{
    int reordering_of_pic_nums_idc;
    if( slice_type != I_TYPE && slice_type !=  SI_TYPE ) {
        ref_pic_list_reordering_flag_l0 = bitReader.getBit();
        if( ref_pic_list_reordering_flag_l0 )
            do {
                reordering_of_pic_nums_idc = extractUEGolombCode();
                if( reordering_of_pic_nums_idc  ==  0  || reordering_of_pic_nums_idc  ==  1 )
                {
                    int tmp = extractUEGolombCode();
                    abs_diff_pic_num_minus1 = tmp;
                    m_ref_pic_vect.push_back(reordering_of_pic_nums_idc);
                    m_ref_pic_vect.push_back(tmp);
                }
                else if( reordering_of_pic_nums_idc  ==  2 ) {
                    int tmp = extractUEGolombCode();
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
                int reordering_of_pic_nums_idc2 = extractUEGolombCode();
                quint32 tmp = extractUEGolombCode();
                if( reordering_of_pic_nums_idc  ==  0  ||
                    reordering_of_pic_nums_idc  ==  1 )
                    abs_diff_pic_num_minus1 = tmp;
                else if( reordering_of_pic_nums_idc  ==  2 )
                    long_term_pic_num = tmp;
                m_ref_pic_vect2.push_back(reordering_of_pic_nums_idc2);
                m_ref_pic_vect2.push_back(tmp);
            } while(reordering_of_pic_nums_idc  !=  3);
    }
}

int SliceUnit::NextMbAddress(int n)
{
    int FrameHeightInMbs = ( 2 - sps->frame_mbs_only_flag ) * sps->pic_height_in_map_units;
    int FrameWidthInMbs = ( 2 - sps->frame_mbs_only_flag ) * sps->pic_width_in_mbs;
    int PicHeightInMbs = FrameHeightInMbs / ( 1 + m_field_pic_flag );
    int PicWidthInMbs = FrameWidthInMbs / ( 1 + m_field_pic_flag );
    int PicSizeInMbs = PicWidthInMbs * PicHeightInMbs;
    Q_UNUSED(PicSizeInMbs);
    int i = n + 1;
    //while( i < PicSizeInMbs  &&  MbToSliceGroupMap[ i ]  !=  MbToSliceGroupMap[ n ] )
    //    i++;
    return i;
}

void SliceUnit::setFrameNum(int frameNum)
{
    assert(m_frameNumBitPos != 0);
    updateBits(m_frameNumBitPos, m_frameNumBits, frameNum);
    if (m_picOrderBitPos > 0)
        updateBits(m_picOrderBitPos, m_picOrderNumBits, frameNum*2 + bottom_field_flag);
}

int SliceUnit::deserializeSliceData()
{
    //if (nal_unit_type != P_TYPE && nal_unit_type != I_TYPE && nal_unit_type != B_TYPE)
    if (nal_unit_type != nuSliceIDR && nal_unit_type != nuSliceNonIDR)
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
                mb_skip_run = extractUEGolombCode(); // !!!!!!!!!!!!!
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

void SliceUnit::macroblock_layer()
{
}

int SliceUnit::serializeSliceHeader(BitStreamWriter& bitWriter, const QMap<quint32, SPSUnit*>& spsMap,
                                    const QMap<quint32, PPSUnit*>& ppsMap, quint8* dstBuffer, int dstBufferLen)
{
    try 
    {
        dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = 0;
        dstBuffer[3] = 1;
        dstBuffer[4] = (nal_ref_idc << 5) + nal_unit_type;
        bitWriter.setBuffer(dstBuffer + 5, dstBuffer + dstBufferLen);
        bitReader.setBuffer(dstBuffer + 5, dstBuffer + dstBufferLen);
        writeUEGolombCode(bitWriter, first_mb_in_slice);
        writeUEGolombCode(bitWriter, orig_slice_type);
        writeUEGolombCode(bitWriter, pic_parameter_set_id);
        QMap<quint32, PPSUnit*>::const_iterator itr = ppsMap.find(pic_parameter_set_id);
        if (itr == ppsMap.end())
            return SPS_OR_PPS_NOT_READY;
        pps = itr.value();

        QMap<quint32, SPSUnit*>::const_iterator itr2 = spsMap.find(pps->seq_parameter_set_id);
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
        if( nal_unit_type == 5)
            writeUEGolombCode(bitWriter, idr_pic_id);
        if( sps->pic_order_cnt_type ==  0) 
        {
            m_picOrderBitPos = bitWriter.getBitsCount(); //getBitContext.buffer
            m_picOrderNumBits = sps->log2_max_pic_order_cnt_lsb;
            bitWriter.putBits( sps->log2_max_pic_order_cnt_lsb, pic_order_cnt_lsb);
            if( pps->pic_order_present_flag &&  !m_field_pic_flag)
                writeUEGolombCode(bitWriter, delta_pic_order_cnt_bottom);
        }
        assert (m_shortDeserializeMode == false);


        if(sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) 
        {
            writeSEGolombCode(bitWriter, delta_pic_order_cnt[0]);
            if( pps->pic_order_present_flag && !m_field_pic_flag)
                writeSEGolombCode(bitWriter, delta_pic_order_cnt[1]);
        }
        if(pps->redundant_pic_cnt_present_flag)
            writeSEGolombCode(bitWriter, redundant_pic_cnt);


        if( slice_type  ==  B_TYPE ) {
            bitWriter.putBit(direct_spatial_mv_pred_flag);
        }
        if( slice_type == P_TYPE || slice_type == SP_TYPE || slice_type == B_TYPE ) 
        {
            bitWriter.putBit(num_ref_idx_active_override_flag);
            if( num_ref_idx_active_override_flag ) {
                writeUEGolombCode(bitWriter, num_ref_idx_l0_active_minus1);
                if( slice_type == B_TYPE)
                    writeUEGolombCode(bitWriter, num_ref_idx_l1_active_minus1);
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
            writeUEGolombCode(bitWriter, cabac_init_idc);
        }
        writeSEGolombCode(bitWriter, slice_qp_delta);
        if( slice_type  ==  SP_TYPE  ||  slice_type  ==  SI_TYPE ) {
            if( slice_type  ==  SP_TYPE )
                bitWriter.putBit(sp_for_switch_flag);
            writeSEGolombCode(bitWriter, slice_qs_delta);
        }
        if( pps->deblocking_filter_control_present_flag ) {
            writeUEGolombCode(bitWriter, disable_deblocking_filter_idc); 
            if( disable_deblocking_filter_idc != 1 ) {
                writeSEGolombCode(bitWriter, slice_alpha_c0_offset_div2);
                writeSEGolombCode(bitWriter, slice_beta_offset_div2);
            }
        }
        if( pps->num_slice_groups_minus1 > 0  &&
            pps->slice_group_map_type >= 3  &&  pps->slice_group_map_type <= 5) {
            int bits = ceil_log2( sps->pic_size_in_map_units / (double)pps->slice_group_change_rate + 1 );
            bitWriter.putBits(bits, slice_group_change_cycle);
        }

        return 0;
    } catch(BitStreamException& e) {
        return NOT_ENOUGHT_BUFFER;
    }
}


void SliceUnit::write_dec_ref_pic_marking(BitStreamWriter& bitWriter)
{
    if( nal_unit_type  ==  5 ) {
        bitWriter.putBit(no_output_of_prior_pics_flag);
        bitWriter.putBit(long_term_reference_flag);
    } else 
    {
        bitWriter.putBit(adaptive_ref_pic_marking_mode_flag);
        if( adaptive_ref_pic_marking_mode_flag )
            for (int i = 0; i < dec_ref_pic_vector.size(); i++)
                writeUEGolombCode(bitWriter, dec_ref_pic_vector[i]);
    }
}


void SliceUnit::write_pred_weight_table(BitStreamWriter& bitWriter)
{
    writeUEGolombCode(bitWriter, luma_log2_weight_denom);
    if( sps->chroma_format_idc  !=  0 )
        writeUEGolombCode(bitWriter, chroma_log2_weight_denom);
    for(int i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) 
    {
        if (luma_weight_l0[i] != INT_MAX) {
            bitWriter.putBit(1);
            writeSEGolombCode(bitWriter, luma_weight_l0[i]);
            writeSEGolombCode(bitWriter, luma_offset_l0[i]);
        }
        else
            bitWriter.putBit(0);
        if ( sps->chroma_format_idc  !=  0 ) {
            if (chroma_weight_l0[i*2] != INT_MAX) {
                bitWriter.putBit(1);
                for(int j =0; j < 2; j++ ) {
                    writeSEGolombCode(bitWriter, chroma_weight_l0[i*2+j]);
                    writeSEGolombCode(bitWriter, chroma_offset_l0[i*2+j]);
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
                writeSEGolombCode(bitWriter, luma_weight_l1[i]);
                writeSEGolombCode(bitWriter, luma_offset_l1[i]);
            }
            else
                bitWriter.putBit(0);

            if( sps->chroma_format_idc  !=  0 ) {
                if (chroma_weight_l1[i*2] != INT_MAX) {
                    bitWriter.putBit(1);
                    for(int j = 0; j < 2; j++ ) {
                        writeSEGolombCode(bitWriter, chroma_weight_l1[i*2+j]);
                        writeSEGolombCode(bitWriter, chroma_offset_l1[i*2+j]);
                    }
                }
                else
                    bitWriter.putBit(0);
                // -------------
            }
        }
}

void SliceUnit::write_ref_pic_list_reordering(BitStreamWriter& bitWriter)
{

    if( slice_type != I_TYPE && slice_type !=  SI_TYPE ) {
        bitWriter.putBit(ref_pic_list_reordering_flag_l0);
        for (int i = 0; i < m_ref_pic_vect.size(); i++)
            writeUEGolombCode(bitWriter, m_ref_pic_vect[i]);
    }

    if( slice_type  ==  B_TYPE ) {
        bitWriter.putBit(ref_pic_list_reordering_flag_l1);
        if( ref_pic_list_reordering_flag_l1 )
            for (int i = 0; i < m_ref_pic_vect2.size(); i++)
                writeUEGolombCode(bitWriter, m_ref_pic_vect2[i]);
    }
}

#endif

// --------------- SEI UNIT ------------------------
void SEIUnit::deserialize(SPSUnit& sps, int orig_hrd_parameters_present_flag)
{
    quint8* nalEnd = m_nalBuffer + m_nalBufferLen;
    try {
        int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
        if (rez != 0)
            return;
        quint8* curBuff = m_nalBuffer + 1;
        while (curBuff < nalEnd-1) {
            int payloadType = 0;
            for(; *curBuff  ==  0xFF && curBuff < nalEnd; curBuff++) 
                payloadType += 0xFF;
            if (curBuff >= nalEnd)
                return;
            payloadType += *curBuff++;
            if (curBuff >= nalEnd)
                return;

            int payloadSize = 0;
            for(; *curBuff  ==  0xFF && curBuff < nalEnd; curBuff++) 
                payloadSize += 0xFF;
            if (curBuff >= nalEnd)
                return;
            payloadSize += *curBuff++;
            if (curBuff >= nalEnd)
                return;
            sei_payload(sps, payloadType, curBuff, payloadSize, orig_hrd_parameters_present_flag);
            m_processedMessages.insert(payloadType);
            curBuff += payloadSize;
        }
    } catch (BitStreamException) {
        qWarning() << "Bad SEI detected. SEI too short";
    }
    return;
}

int SEIUnit::updateSeiParam(SPSUnit& sps, bool removePulldown, int orig_hrd_parameters_present_flag)
{
    quint8* nalEnd = m_nalBuffer + m_nalBufferLen;
    quint8* curBuff = m_nalBuffer + 1;
    quint8 tmpBuffer[1024*4];
    tmpBuffer[0] = m_nalBuffer[0];
    int tmpBufferLen = 1;

    while (curBuff < nalEnd) {
        int payloadType = 0;
        for(; *curBuff  ==  0xFF && curBuff < nalEnd; curBuff++) {
            payloadType += 0xFF;
            tmpBuffer[tmpBufferLen++] = 0xff;
        }
        if (curBuff >= nalEnd)
            break;
        payloadType += *curBuff++;
        tmpBuffer[tmpBufferLen++] = payloadType;
        if (curBuff >= nalEnd)
            break;

        int payloadSize = 0;
        for(; *curBuff  ==  0xFF && curBuff < nalEnd; curBuff++) {
            payloadSize += 0xFF;
            tmpBuffer[tmpBufferLen++] = 0xff;
        }
        if (curBuff >= nalEnd)
            break;

        payloadSize += *curBuff++;
        tmpBuffer[tmpBufferLen++] = payloadSize;
        if (curBuff >= nalEnd)
            break;
        if (payloadType == 1) 
        { // pic_timing
            pic_timing(sps, curBuff, payloadSize, orig_hrd_parameters_present_flag);
            if (removePulldown) {
                if (pic_struct == 6)
                    pic_struct = 4;
                else if (pic_struct == 5)
                    pic_struct = 3;
                else if (pic_struct == 7 || pic_struct == 8)
                    pic_struct = 0;
            }
            //if (sps.frame_mbs_only_flag)
            //    pic_struct = 0;

            if ((orig_hrd_parameters_present_flag == 1 ||  sps.vcl_hrd_parameters_present_flag) && !removePulldown)
            {
                BitStreamWriter writer;
                tmpBufferLen--;
                writer.setBuffer(tmpBuffer + tmpBufferLen, tmpBuffer + sizeof(tmpBuffer));
                serialize_pic_timing_message(sps, writer, false);
                tmpBufferLen += writer.getBitsCount() / 8;
            }
            else
                tmpBufferLen -= 2; // skip this sei message
        }
        else if (payloadType == 0 && (
            (!orig_hrd_parameters_present_flag && !sps.vcl_hrd_parameters_present_flag) || removePulldown)
            ) 
        { 
            tmpBufferLen -= 2; // skip this sei message
        }
        else {
            memcpy(tmpBuffer + tmpBufferLen, curBuff, payloadSize);
            tmpBufferLen += payloadSize;
        }
        curBuff += payloadSize;
    }
    if (m_nalBufferLen < tmpBufferLen) {
        //assert(m_nalBufferLen < tmpBufferLen);
        delete [] m_nalBuffer;
        m_nalBuffer = new quint8[tmpBufferLen];
    }
    memcpy(m_nalBuffer, tmpBuffer, tmpBufferLen);
    m_nalBufferLen = tmpBufferLen;
    return tmpBufferLen;
}

void SEIUnit::sei_payload(SPSUnit& sps, int payloadType, const quint8* curBuff, int payloadSize, int orig_hrd_parameters_present_flag)
{
    if( payloadType  ==  0 )
        buffering_period( payloadSize );
    else if( payloadType  ==  1 )
        pic_timing(sps, curBuff, payloadSize, orig_hrd_parameters_present_flag);
    else if( payloadType  ==  2 )
        pan_scan_rect( payloadSize );
    else if( payloadType  ==  3 )
        filler_payload( payloadSize );
    else if( payloadType  ==  4 )
        user_data_registered_itu_t_t35( payloadSize );
    else if( payloadType  ==  5 )
        user_data_unregistered(curBuff, payloadSize );
    else if( payloadType  ==  6 )
        recovery_point( payloadSize );
    else if( payloadType  ==  7 )
        dec_ref_pic_marking_repetition( payloadSize );
    else if( payloadType  ==  8 )
        spare_pic( payloadSize );
    else if( payloadType  ==  9 )
        scene_info( payloadSize );
    else if( payloadType  ==  10 )
        sub_seq_info( payloadSize );
    else if( payloadType  ==  11 )
        sub_seq_layer_characteristics( payloadSize );
    else if( payloadType  ==  12 )
        sub_seq_characteristics( payloadSize );
    else if( payloadType  ==  13 )
        full_frame_freeze( payloadSize );
    else if( payloadType  ==  14 )
        full_frame_freeze_release( payloadSize );
    else if( payloadType  ==  15 )
        full_frame_snapshot( payloadSize );
    else if( payloadType  ==  16 )
        progressive_refinement_segment_start( payloadSize );
    else if( payloadType  ==  17 )
        progressive_refinement_segment_end( payloadSize );
    else if( payloadType  ==  18 )
        motion_constrained_slice_group_set( payloadSize );
    else if( payloadType  ==  19 )
        film_grain_characteristics( payloadSize );
    else if( payloadType  ==  20 )
        deblocking_filter_display_preference( payloadSize );
    else if( payloadType  ==  21 )
        stereo_video_info( payloadSize );
    else
        reserved_sei_message( payloadSize );
    /*
    if( !byte_aligned( ) ) {
    bit_equal_to_one  // equal to 1 
    while( !byte_aligned( ) )
    bit_equal_to_zero  // equal to 0 
    }
    */
}

void SEIUnit::buffering_period(int /*payloadSize*/) {}

void SEIUnit::serialize_pic_timing_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader)
{
    if (seiHeader) {
        writer.putBits(8, nuSEI);
        writer.putBits(8, SEI_MSG_PIC_TIMING);
    }
    quint8* size = writer.getBuffer() + writer.getBitsCount()/8;
    writer.putBits(8, 0);
    int beforeMessageLen = writer.getBitsCount();
    // pic timing
    if (sps.nal_hrd_parameters_present_flag || sps.vcl_hrd_parameters_present_flag) 
    {
        m_cpb_removal_delay_baseaddr = writer.getBuffer();
        m_cpb_removal_delay_bitpos = writer.getBitsCount();
        writer.putBits(sps.cpb_removal_delay_length_minus1 + 1, cpb_removal_delay);
        writer.putBits(sps.dpb_output_delay_length_minus1 + 1, dpb_output_delay);
    }
    if (sps.pic_struct_present_flag) {
        writer.putBits(4, pic_struct);
        int NumClockTS = 0;
        switch(pic_struct) {
            case 0:
            case 1:
            case 2:
                NumClockTS = 1;
                break;
            case 3:
            case 4:
            case 7:
                NumClockTS = 2;
                break;
            case 5:
            case 6:
            case 8:
                NumClockTS = 3;
                break;
        }
        for(int i = 0; i < NumClockTS; i++ ) 
            writer.putBit(0); //clock_timestamp_flag
    }
    write_byte_align_bits(writer);
    // ---------
    int msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = msgLen / 8;
    if (seiHeader) {
        write_rbsp_trailing_bits(writer);
    }
    writer.flushBits();
}

void SEIUnit::serialize_buffering_period_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader)
{
    if (seiHeader) {
        writer.putBits(8, nuSEI);
        writer.putBits(8, SEI_MSG_BUFFERING_PERIOD);
    }
    quint8* size = writer.getBuffer() + writer.getBitsCount()/8;
    writer.putBits(8, 0);
    int beforeMessageLen = writer.getBitsCount();
    // buffering period
    writeUEGolombCode(writer, sps.seq_parameter_set_id);
    if (sps.nal_hrd_parameters_present_flag) { // NalHrdBpPresentFlag
        for(int SchedSelIdx = 0; SchedSelIdx <= sps.cpb_cnt_minus1; SchedSelIdx++ ) 
        {
            writer.putBits(sps.initial_cpb_removal_delay_length_minus1 + 1, initial_cpb_removal_delay[SchedSelIdx]);
            writer.putBits(sps.initial_cpb_removal_delay_length_minus1 + 1, initial_cpb_removal_delay_offset[SchedSelIdx]);
        }
    }
    if (sps.vcl_hrd_parameters_present_flag) { // NalHrdBpPresentFlag
        for(int SchedSelIdx = 0; SchedSelIdx <= sps.cpb_cnt_minus1; SchedSelIdx++ ) 
        {
            writer.putBits(sps.initial_cpb_removal_delay_length_minus1 + 1, initial_cpb_removal_delay[SchedSelIdx]);
            writer.putBits(sps.initial_cpb_removal_delay_length_minus1 + 1, initial_cpb_removal_delay_offset[SchedSelIdx]);
        }
    }
    write_byte_align_bits(writer);
    // ---------
    int msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = msgLen / 8;
    if (seiHeader) {
        write_rbsp_trailing_bits(writer);
    }
    writer.flushBits();
}

void SEIUnit::pic_timing(SPSUnit& sps, const quint8* curBuff, int payloadSize, 
                         bool orig_hrd_parameters_present_flag) 
{
    /*
    bool nal_hrd_param_flag = sps.nal_hrd_parameters_present_flag;
    if (useOldHdrParam)
        nal_hrd_param_flag = sps.orig_hrd_parameters_present_flag;
    */

    bool CpbDpbDelaysPresentFlag = orig_hrd_parameters_present_flag == 1 || sps.vcl_hrd_parameters_present_flag == 1;
    bitReader.setBuffer(curBuff, curBuff + payloadSize);
    cpb_removal_delay = dpb_output_delay = 0;
    if( CpbDpbDelaysPresentFlag ) {        
        cpb_removal_delay = bitReader.getBits(sps.cpb_removal_delay_length_minus1 + 1);
        dpb_output_delay = bitReader.getBits(sps.dpb_output_delay_length_minus1 + 1);
    }        
    if( sps.pic_struct_present_flag ) {        
        pic_struct = bitReader.getBits(4);
        pic_struct = pic_struct;
        /*
        int numClockTS = 0; //getNumClockTS();
        for( int i = 0; i < numClockTS; i++ ) {
            Clock_timestamp_flag[i] = bitReader.getBit();
            if( clock_timestamp_flag[i] ) {        
                Ct_type    = bitReader.getBits(2);
                nuit_field_based_flag = bitReader.getBit();
                counting_type bitReader.getBits(5);
                full_timestamp_flag    = bitReader.getBit();
                discontinuity_flag    = bitReader.getBit();
                cnt_dropped_flag = bitReader.getBit();
                n_frames = bitReader.getBits(8);
                if( full_timestamp_flag ) {        
                    seconds_value = bitReader.getBits(6);
                    minutes_value = bitReader.getBits(6);
                    hours_value = bitReader.getBits(5);
                } else {        
                    seconds_flag = bitReader.getBit();
                    if( seconds_flag ) {        
                        seconds_value = bitReader.getBits(6);
                        minutes_flag = bitReader.getBit();
                        if( minutes_flag ) {
                            minutes_value = bitReader.getBits(6);
                            hours_flag = bitReader.getBit();
                            if( hours_flag )
                                hours_value = bitReader.getBits(5);
                        }
                    }
                }
                if( time_offset_length > 0 ) {
                    //time_offset    = bitReader.getBits(time_offset_length); // signed bits!
                }
            }
        }
        */
    }

}

/*
int getNumClockTS()
{
    if (field_pic_flag == 0)
        return 0;
    if (field_pic_flag == 0 && bottom_field_flag == 0)
        return 1;
}
*/

void SEIUnit::pan_scan_rect(int /*payloadSize*/) {}
void SEIUnit::filler_payload(int /*payloadSize*/) {}
void SEIUnit::user_data_registered_itu_t_t35(int /*payloadSize*/) {}

void SEIUnit::user_data_unregistered(const quint8* data, int payloadSize) 
{
    m_userDataPayload << QPair<const quint8*, int>(data, payloadSize);
}

void SEIUnit::recovery_point(int /*payloadSize*/) {}
void SEIUnit::dec_ref_pic_marking_repetition(int /*payloadSize*/) {}
void SEIUnit::spare_pic(int /*payloadSize*/) {}
void SEIUnit::scene_info(int /*payloadSize*/) {}
void SEIUnit::sub_seq_info(int /*payloadSize*/) {}
void SEIUnit::sub_seq_layer_characteristics(int /*payloadSize*/) {}
void SEIUnit::sub_seq_characteristics(int /*payloadSize*/) {}
void SEIUnit::full_frame_freeze(int /*payloadSize*/) {}
void SEIUnit::full_frame_freeze_release(int /*payloadSize*/) {}
void SEIUnit::full_frame_snapshot(int /*payloadSize*/) {}
void SEIUnit::progressive_refinement_segment_start(int /*payloadSize*/) {}
void SEIUnit::progressive_refinement_segment_end(int /*payloadSize*/) {}
void SEIUnit::motion_constrained_slice_group_set(int /*payloadSize*/) {}
void SEIUnit::film_grain_characteristics(int /*payloadSize*/) {}
void SEIUnit::deblocking_filter_display_preference(int /*payloadSize*/) {}
void SEIUnit::stereo_video_info(int /*payloadSize*/) {}
void SEIUnit::reserved_sei_message(int /*payloadSize*/) {}


namespace h264
{
    namespace AspectRatio
    {
        void decode( int aspect_ratio_idc, unsigned int* w, unsigned int* h )
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
}
