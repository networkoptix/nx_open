// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "supplemental_enhancement_information.h"

#include <nx/codec/h264/common.h>
#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace nx::media::h264 {

const static int SEI_MSG_BUFFERING_PERIOD = 0;
const static int SEI_MSG_PIC_TIMING = 1;

bool SupplementalEnhancementInformation::deserialize(SequenceParameterSet& sps, int orig_hrd_parameters_present_flag)
{
    quint8* nalEnd = m_nalBuffer + m_nalBufferLen;
    try {
        int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
        if (rez != 0)
            return false;

        quint8* curBuff = m_nalBuffer + 1;
        while (curBuff < nalEnd-1)
        {
            int payloadType = 0;
            for(; *curBuff  ==  0xFF && curBuff < nalEnd; curBuff++)
                payloadType += 0xFF;
            if (curBuff >= nalEnd)
                return false;

            payloadType += *curBuff++;
            if (curBuff >= nalEnd)
                return false;

            int payloadSize = 0;
            for(; *curBuff  ==  0xFF && curBuff < nalEnd; curBuff++)
                payloadSize += 0xFF;

            if (curBuff >= nalEnd)
                return false;

            payloadSize += *curBuff++;
            if (curBuff >= nalEnd || curBuff + payloadSize > nalEnd)
                return false;

            sei_payload(sps, payloadType, curBuff, payloadSize, orig_hrd_parameters_present_flag);
            m_processedMessages.insert(payloadType);
            curBuff += payloadSize;
        }
    }
    catch (nx::utils::BitStreamException const&)
    {
        NX_DEBUG(this, "Bad SEI detected. SEI too short");
        return false;
    }
    return true;
}

int SupplementalEnhancementInformation::updateSeiParam(SequenceParameterSet& sps, bool removePulldown, int orig_hrd_parameters_present_flag)
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
                nx::utils::BitStreamWriter writer;
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
        //NX_ASSERT(m_nalBufferLen < tmpBufferLen);
        delete [] m_nalBuffer;
        m_nalBuffer = new quint8[tmpBufferLen];
    }
    memcpy(m_nalBuffer, tmpBuffer, tmpBufferLen);
    m_nalBufferLen = tmpBufferLen;
    return tmpBufferLen;
}

void SupplementalEnhancementInformation::sei_payload(SequenceParameterSet& sps, int payloadType, const quint8* curBuff, int payloadSize, int orig_hrd_parameters_present_flag)
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

void SupplementalEnhancementInformation::serialize_pic_timing_message(const SequenceParameterSet& sps, nx::utils::BitStreamWriter& writer, bool seiHeader)
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
    nx::media::nal::write_byte_align_bits(writer);

    int msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = msgLen / 8;
    if (seiHeader) {
        nx::media::nal::write_rbsp_trailing_bits(writer);
    }
    writer.flushBits();
}

void SupplementalEnhancementInformation::serialize_buffering_period_message(const SequenceParameterSet& sps, nx::utils::BitStreamWriter& writer, bool seiHeader)
{
    if (seiHeader) {
        writer.putBits(8, nuSEI);
        writer.putBits(8, SEI_MSG_BUFFERING_PERIOD);
    }
    quint8* size = writer.getBuffer() + writer.getBitsCount()/8;
    writer.putBits(8, 0);
    int beforeMessageLen = writer.getBitsCount();
    // buffering period
    writer.putGolomb(sps.seq_parameter_set_id);
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
    nx::media::nal::write_byte_align_bits(writer);
    // ---------
    int msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = msgLen / 8;
    if (seiHeader) {
        nx::media::nal::write_rbsp_trailing_bits(writer);
    }
    writer.flushBits();
}

void SupplementalEnhancementInformation::pic_timing(
    SequenceParameterSet& sps,
    const quint8* curBuff,
    int payloadSize,
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

void SupplementalEnhancementInformation::user_data_unregistered(const quint8* data, int payloadSize)
{
    m_userDataPayload << QPair<const quint8*, int>(data, payloadSize);
}

void SupplementalEnhancementInformation::buffering_period(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::pan_scan_rect(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::filler_payload(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::user_data_registered_itu_t_t35(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::recovery_point(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::dec_ref_pic_marking_repetition(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::spare_pic(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::scene_info(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::sub_seq_info(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::sub_seq_layer_characteristics(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::sub_seq_characteristics(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::full_frame_freeze(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::full_frame_freeze_release(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::full_frame_snapshot(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::progressive_refinement_segment_start(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::progressive_refinement_segment_end(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::motion_constrained_slice_group_set(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::film_grain_characteristics(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::deblocking_filter_display_preference(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::stereo_video_info(int /*payloadSize*/) {}
void SupplementalEnhancementInformation::reserved_sei_message(int /*payloadSize*/) {}

} // namespace nx::media::h264
