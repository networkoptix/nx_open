// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/codec/h264/common.h>
#include <nx/codec/h264/picture_parameter_set.h>
#include <nx/codec/h264/sequence_parameter_set.h>

namespace nx::media::h264 {

class NX_CODEC_API SliceHeader: public NALUnit
{
public:
    enum SliceType {P_TYPE, B_TYPE, I_TYPE, SP_TYPE, SI_TYPE, DUMMY_TYPE};
    int first_mb_in_slice;
    int slice_type;
    int orig_slice_type;
    int pic_parameter_set_id;
    int frame_num;
    int bottom_field_flag;
    int idr_pic_id;
    int pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom;
    int m_picOrderBitPos;
    int m_picOrderNumBits;
    int m_field_pic_flag = 0;
    int memory_management_control_operation = 0;

    int deserialize(quint8* buffer, quint8* end, const SequenceParameterSet* sps, const PictureParameterSet* pps);

    const SequenceParameterSet* getSPS() const {return sps;}
    const PictureParameterSet* getPPS() const {return pps;}

    int slice_qp_delta;
    int disable_deblocking_filter_idc = 0;
    int slice_alpha_c0_offset_div2;
    int slice_beta_offset_div2;
    int delta_pic_order_cnt[2];
    int slice_qs_delta;
    int redundant_pic_cnt;
    int slice_group_change_cycle;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int direct_spatial_mv_pred_flag;
    int num_ref_idx_active_override_flag;
    int sp_for_switch_flag;
    int ref_pic_list_reordering_flag_l0;
    int abs_diff_pic_num_minus1;
    int cabac_init_idc;
    int no_output_of_prior_pics_flag;
    int long_term_reference_flag;
    int adaptive_ref_pic_marking_mode_flag;

    int ref_pic_list_reordering_flag_l1;
    int long_term_pic_num;
    int luma_log2_weight_denom;
    int chroma_log2_weight_denom;

    QVector<int> m_ref_pic_vect;
    QVector<int> m_ref_pic_vect2;
    QVector<int> dec_ref_pic_vector;
    QVector<int> luma_weight_l0;
    QVector<int> luma_offset_l0;
    QVector<int> chroma_weight_l0;
    QVector<int> chroma_offset_l0;
    QVector<int> luma_weight_l1;
    QVector<int> luma_offset_l1;
    QVector<int> chroma_weight_l1;
    QVector<int> chroma_offset_l1;

    bool m_shortDeserializeMode = true;
    int m_frameNumBitPos = 0;

    void copyFrom(const SliceHeader& other) {
        delete m_nalBuffer;
        //memcpy(this, &other, sizeof( SliceHeader));
        m_nalBuffer = 0;
        m_nalBufferLen = 0;

        nal_ref_idc = other.nal_ref_idc;
        nal_unit_type = other.nal_unit_type;

        slice_qp_delta = other.slice_qp_delta;
        disable_deblocking_filter_idc = other.disable_deblocking_filter_idc;
        slice_alpha_c0_offset_div2 = other.slice_alpha_c0_offset_div2;
        slice_beta_offset_div2 = other.slice_beta_offset_div2;
        delta_pic_order_cnt[0] = other.delta_pic_order_cnt[0];
        delta_pic_order_cnt[1] = other.delta_pic_order_cnt[1];
        slice_qs_delta = other.slice_qs_delta;
        redundant_pic_cnt = other.redundant_pic_cnt;
        slice_group_change_cycle = other.slice_group_change_cycle;
        num_ref_idx_l0_active_minus1 = other.num_ref_idx_l0_active_minus1;
        num_ref_idx_l1_active_minus1 = other.num_ref_idx_l1_active_minus1;
        direct_spatial_mv_pred_flag = other.direct_spatial_mv_pred_flag;
        num_ref_idx_active_override_flag = other.num_ref_idx_active_override_flag;
        sp_for_switch_flag = other.sp_for_switch_flag;
        ref_pic_list_reordering_flag_l0 = other.ref_pic_list_reordering_flag_l0;
        abs_diff_pic_num_minus1 = other.abs_diff_pic_num_minus1;
        cabac_init_idc = other.cabac_init_idc;
        no_output_of_prior_pics_flag = other.no_output_of_prior_pics_flag;
        long_term_reference_flag = other.long_term_reference_flag;
        adaptive_ref_pic_marking_mode_flag = other.adaptive_ref_pic_marking_mode_flag;

        ref_pic_list_reordering_flag_l1 = other.ref_pic_list_reordering_flag_l1;
        long_term_pic_num = other.long_term_pic_num;
        luma_log2_weight_denom = other.luma_log2_weight_denom;
        chroma_log2_weight_denom = other.chroma_log2_weight_denom;
        m_ref_pic_vect = other.m_ref_pic_vect;
        m_ref_pic_vect2 = other.m_ref_pic_vect2;
        dec_ref_pic_vector = other.dec_ref_pic_vector;
        luma_weight_l0 = other.luma_weight_l0;
        luma_offset_l0 = other.luma_offset_l0;
        chroma_weight_l0 = other.chroma_weight_l0;
        chroma_offset_l0 = other.chroma_offset_l0;
        luma_weight_l1 = other.luma_weight_l1;
        luma_offset_l1 = other.luma_offset_l1;
        chroma_weight_l1 = other.chroma_weight_l1;
        chroma_offset_l1 = other.chroma_offset_l1;
        m_shortDeserializeMode = other.m_shortDeserializeMode;

        first_mb_in_slice = other.first_mb_in_slice;
        slice_type = other.slice_type;
        orig_slice_type = other.orig_slice_type;
        pic_parameter_set_id = other.pic_parameter_set_id;

        frame_num = other.frame_num;
        bottom_field_flag = other.bottom_field_flag;
        idr_pic_id = other.idr_pic_id;
        pic_order_cnt_lsb = other.pic_order_cnt_lsb;
        delta_pic_order_cnt_bottom = other.delta_pic_order_cnt_bottom;
        m_picOrderBitPos = other.m_picOrderBitPos;
        m_picOrderNumBits = other.m_picOrderNumBits;
        m_field_pic_flag = other.m_field_pic_flag;
        memory_management_control_operation = other.memory_management_control_operation;

    }
    void setFrameNum(int num);
    int deserializeSliceData();
    int serializeSliceHeader(nx::utils::BitStreamWriter& bitWriter, const QMap<quint32, SequenceParameterSet*>& spsMap,
                                    const QMap<quint32, PictureParameterSet*>& ppsMap, quint8* dstBuffer, int dstBufferLen);

    int deserialize(const SequenceParameterSet* sps,const PictureParameterSet* pps);

private:
    void write_pred_weight_table(nx::utils::BitStreamWriter& bitWriter);
    void write_dec_ref_pic_marking(nx::utils::BitStreamWriter& bitWriter);
    void write_ref_pic_list_reordering(nx::utils::BitStreamWriter& bitWriter);
    void macroblock_layer();
    void pred_weight_table();
    void dec_ref_pic_marking();
    void ref_pic_list_reordering();
    int NextMbAddress(int n);

private:
    const PictureParameterSet* pps;
    const SequenceParameterSet* sps;
    int m_frameNumBits = 0;
    int m_fullHeaderLen = 0;
    int deserializeSliceHeader(const SequenceParameterSet* sps, const PictureParameterSet* pps);
};

} // namespace nx::media::h264
