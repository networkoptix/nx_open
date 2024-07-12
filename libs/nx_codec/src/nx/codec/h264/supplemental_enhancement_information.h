// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/codec/h264/common.h>
#include <nx/codec/h264/sequence_parameter_set.h>

namespace nx::media::h264 {

class NX_CODEC_API SupplementalEnhancementInformation: public NALUnit
{
public:
    bool deserialize(SequenceParameterSet& sps, int orig_hrd_parameters_present_flag);
    int cpb_removal_delay = 0;
    int dpb_output_delay = 0;
    int pic_struct = 0;
    int initial_cpb_removal_delay[32];
    int initial_cpb_removal_delay_offset[32];
    QSet<int> m_processedMessages;
    QList<QPair<const quint8*, int> > m_userDataPayload;
    quint8* m_cpb_removal_delay_baseaddr = 0;
    int m_cpb_removal_delay_bitpos = 0;
    void serialize_pic_timing_message(const SequenceParameterSet& sps, nx::utils::BitStreamWriter& writer, bool seiHeader);
    void serialize_buffering_period_message(const SequenceParameterSet& sps, nx::utils::BitStreamWriter& writer, bool seiHeader);
    int updateSeiParam(SequenceParameterSet& sps, bool removePulldown, int orig_hrd_parameters_present_flag);

private:
    void sei_payload(SequenceParameterSet& sps, int payloadType, const quint8* curBuf, int payloadSize, int orig_hrd_parameters_present_flag);
    void buffering_period(int payloadSize);
    void pic_timing(SequenceParameterSet& sps, const quint8* curBuf, int payloadSize, bool orig_hrd_parameters_present_flag);
    void pan_scan_rect(int payloadSize);
    void filler_payload(int payloadSize);
    void user_data_registered_itu_t_t35(int payloadSize);
    void user_data_unregistered(const quint8* data, int payloadSize);
    void recovery_point(int payloadSize);
    void dec_ref_pic_marking_repetition(int payloadSize);
    void spare_pic(int payloadSize);
    void scene_info(int payloadSize);
    void sub_seq_info(int payloadSize);
    void sub_seq_layer_characteristics(int payloadSize);
    void sub_seq_characteristics(int payloadSize);
    void full_frame_freeze(int payloadSize);
    void full_frame_freeze_release(int payloadSize);
    void full_frame_snapshot(int payloadSize);
    void progressive_refinement_segment_start(int payloadSize);
    void progressive_refinement_segment_end(int payloadSize);
    void motion_constrained_slice_group_set(int payloadSize);
    void film_grain_characteristics(int payloadSize);
    void deblocking_filter_display_preference(int payloadSize);
    void stereo_video_info(int payloadSize);
    void reserved_sei_message(int payloadSize);
};

} // namespace nx::media::h264
