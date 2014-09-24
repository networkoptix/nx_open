#ifndef __NAL_UNITS_H
#define __NAL_UNITS_H

#include <inttypes.h>

#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QVector>
#include <memory.h>
#include <map>

#include "bitStream.h"


const static int 	Extended_SAR = 255;
const static double h264_ar_coeff[] = {0.0, 1.0, 12.0/11.0, 	10.0/11.0, 	16.0/11.0, 	40.0/33.0, 	24.0/11.0, 	20.0/11.0, 	
									  32.0/11.0, 	80.0/33.0, 	18.0/11.0, 	15.0/11.0, 	64.0/33.0, 	160.0/99.0, 4.0/3.0, 	3.0/2.0, 	2.0/1.0};

enum NALUnitType   {nuUnspecified, nuSliceNonIDR, nuSliceA, nuSliceB, nuSliceC,  // 0..4
                    nuSliceIDR, nuSEI, nuSPS, nuPPS, nuDelimiter,                // 5..9
					nuEOSeq, nuEOStream, nuFillerData, nuSPSExt, nuReserved1,    // 10..14
					nuReserved2, nuReserved3, nuReserved4, nuReserved5, nuSliceWithoutPartitioning,  // 15..19
					nuReserved6, nuReserved7, nuReserved8, nuReserved9, // 20..23
					STAP_A_PACKET, STAP_B_PACKET, MTAP16_PACKET, MTAP24_PACKET, FU_A_PACKET, FU_B_PACKET,
					nuDummy
                   };

const static int SEI_MSG_BUFFERING_PERIOD = 0;
const static int SEI_MSG_PIC_TIMING = 1;

#ifndef __TS_MUXER_COMPILE_MODE
const char* const NALUnitDescr[30] =
                   {"nuUnspecified", "nuSliceNonIDR", "nuSliceA", "nuSliceB", "nuSliceC", 
                    "nuSliceIDR","nuSEI","nuSPS","nuPPS","nuAUD", 
					"nuEOSeq","nuEOStream","nuFillerData","nuSPSExt","nuReserved1", 
					"nuReserved2","nuReserved3","nuReserved4","nuReserved5","nuSliceWithoutPartitioning", 
					"nuReserved6","nuReserved7","nuReserved8"," nuReserved9",
					// ------------------- additional RTP nal units -------------
					"STAP-A","STAP-B","MTAP16","MTAP24","FU-A","FU-B"
                   };
                   
#endif

class NALUnit {
public:
	const static int SPS_OR_PPS_NOT_READY = 1;
	const static int NOT_ENOUGHT_BUFFER = 2;
	const static int UNSUPPORTED_PARAM = 3;
	const static int NOT_FOUND = 4;
	const static int UNIT_SKIPPED = 5;
	int nal_ref_idc;
	int nal_unit_type;

	quint8* m_nalBuffer;
	int m_nalBufferLen;

	NALUnit() {m_nalBufferLen = 0; m_nalBuffer = 0;}
	//NALUnit(const NALUnit& other);
	virtual ~NALUnit() { 
		delete [] m_nalBuffer; 
	}

    static const quint8* findNextNAL(const quint8* buffer, const quint8* end);
	static const quint8* findNALWithStartCode(const quint8* buffer, const quint8* end, bool longCodesAllowed);
	
    static int encodeNAL(quint8* srcBuffer, quint8* srcEnd, quint8* dstBuffer, size_t dstBufferSize);
    int encodeNAL(quint8* dstBuffer, size_t dstBufferSize);


	static int decodeNAL(const quint8* srcBuffer, const quint8* srcEnd, quint8* dstBuffer, size_t dstBufferSize);
	virtual int deserialize(quint8* buffer, quint8* end);
	virtual int serializeBuffer(quint8* dstBuffer, quint8* dstEnd, bool writeStartCode) const;
	virtual int serialize(quint8* dstBuffer);
	//void setBuffer(quint8* buffer, quint8* end);
	void decodeBuffer(const quint8* buffer,const quint8* end);
	static quint8* addStartCode(quint8* buffer, quint8* boundStart);

	static int extractUEGolombCode(BitStreamReader& bitReader);
	static void writeUEGolombCode(BitStreamWriter& bitWriter, quint32 value);
	void writeSEGolombCode(BitStreamWriter& bitWriter, qint32 value);
	const BitStreamReader& getBitReader() const {return bitReader;}
	void write_rbsp_trailing_bits(BitStreamWriter& writer);
	void write_byte_align_bits(BitStreamWriter& writer);

    int calc_rbsp_trailing_bits_cnt(uint8_t val);

protected:
	//GetBitContext getBitContext;
	BitStreamReader bitReader;
	inline int extractUEGolombCode();
	inline int extractSEGolombCode();
	void updateBits(int bitOffset, int bitLen, int value);
    void scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag);

private:
    NALUnit( const NALUnit& );
    NALUnit& operator=( const NALUnit& right );
};

class NALDelimiter: public NALUnit {
public:
	const static int PCT_I_FRAMES = 0;
	const static int PCT_IP_FRAMES = 1;
	const static int PCT_IPB_FRAMES = 2;
	const static int PCT_SI_FRAMES = 3;
	const static int PCT_SI_SP_FRAMES = 4;
	const static int PCT_I_SI_FRAMES = 5;
	const static int PCT_I_SI_P_SP_FRAMES = 6;
	const static int PCT_I_SI_P_SP_B_FRAMES = 7;
	int primary_pic_type;
	NALDelimiter():NALUnit() {nal_unit_type = nuDelimiter;}
	int deserialize(quint8* buffer, quint8* end);
	int serialize(quint8* dstBuffer);
};

class PPSUnit: public NALUnit {
public:
	int pic_parameter_set_id;
	int seq_parameter_set_id;
	int entropy_coding_mode_flag;
	int pic_order_present_flag;
	int num_ref_idx_l0_active_minus1;
	int num_ref_idx_l1_active_minus1;
	int weighted_pred_flag;
	int weighted_bipred_idc;
	int pic_init_qp_minus26;
	int pic_init_qs_minus26;
    int transform_8x8_mode_flag;
    int pic_scaling_matrix_present_flag;
	int chroma_qp_index_offset;
	int deblocking_filter_control_present_flag;
	int constrained_intra_pred_flag;
	int redundant_pic_cnt_present_flag;
	int run_length_minus1[256];
	int top_left[256];
	int bottom_right[256];
	int slice_group_id[256];
	int slice_group_change_direction_flag;
	int slice_group_change_rate;
	int num_slice_groups_minus1;
	int slice_group_map_type;
    int second_chroma_qp_index_offset;
    int scalingLists4x4[6][16];
    bool useDefaultScalingMatrix4x4Flag[6];
    int scalingLists8x8[2][64];
    bool useDefaultScalingMatrix8x8Flag[2];

    PPSUnit();
	virtual ~PPSUnit() {}
	bool isReady() {return m_ready;}
	int deserialize();
	int deserializeID(quint8* buffer, quint8* end);
	// duplicate PPS and change ppsID and cabac parameter for new PPS
	void duplicatePPS(PPSUnit& oldPPS, int ppsID, bool cabac);

private:
	int m_ppsLenInMbit;
	bool m_ready;
	int entropy_coding_mode_BitPos;
};


class SPSUnit: public NALUnit {
public:
	int profile_idc;
	int constraint_set0_flag0;
	int constraint_set0_flag1;
	int constraint_set0_flag2;
	int constraint_set0_flag3;
	int level_idc;
	int seq_parameter_set_id;
	int chroma_format_idc;
	int residual_colour_transform_flag;
	int bit_depth_luma;
	int bit_depth_chroma;
	int qpprime_y_zero_transform_bypass_flag;
	int seq_scaling_matrix_present_flag;
	int log2_max_frame_num;
	int pic_order_cnt_type;
	int log2_max_pic_order_cnt_lsb;
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int num_ref_frames_in_pic_order_cnt_cycle;
	int offset_for_ref_frame[256];
	int num_ref_frames;
	int gaps_in_frame_num_value_allowed_flag;
	int pic_width_in_mbs;
	int pic_height_in_map_units;
	int frame_mbs_only_flag;
	int mb_adaptive_frame_field_flag;
	int direct_8x8_inference_flag;
	int frame_cropping_flag;
	int frame_crop_left_offset;
	int frame_crop_right_offset;
	int frame_crop_top_offset;
	int frame_crop_bottom_offset;
	int vui_parameters_present_flag;
	//int field_pic_flag;
	int pic_size_in_map_units;
	int nal_hrd_parameters_present_flag;
	//int orig_hrd_parameters_present_flag;
	int vcl_hrd_parameters_present_flag;
	int pic_struct_present_flag;

	int sar_width;
	int sar_height;
	quint32 num_units_in_tick;
	int num_units_in_tick_bit_pos;
	quint32 time_scale;
	int fixed_frame_rate_flag;

	int* bit_rate_value_minus1;
	int* cpb_size_value_minus1;
	quint8* cbr_flag;

	int initial_cpb_removal_delay_length_minus1;
	int cpb_removal_delay_length_minus1;
	int dpb_output_delay_length_minus1;
	int time_offset_length;
	int low_delay_hrd_flag;
	int motion_vectors_over_pic_boundaries_flag;
	int max_bytes_per_pic_denom;
	int max_bits_per_mb_denom;
	int log2_max_mv_length_horizontal;
	int log2_max_mv_length_vertical;
	int num_reorder_frames;
	int bitstream_restriction_flag;
	int chroma_loc_info_present_flag;
	int chroma_sample_loc_type_top_field;
	int chroma_sample_loc_type_bottom_field;
	int timing_info_present_flag;
	int video_format;
	int overscan_appropriate_flag;
	int video_full_range_flag;
	int colour_description_present_flag;
	int colour_primaries;
	int transfer_characteristics;
	int matrix_coefficients;
	int aspect_ratio_info_present_flag;
	int aspect_ratio_idc;
	int overscan_info_present_flag;
	int cpb_cnt_minus1;
	int bit_rate_scale;
	int cpb_size_scale;

	int nal_hrd_parameters_bit_pos;
	int full_sps_bit_len;

    int ScalingList4x4[6][16];
    int ScalingList8x8[2][64];
    bool UseDefaultScalingMatrix4x4Flag[6];
    bool UseDefaultScalingMatrix8x8Flag[2];


	int max_dec_frame_buffering;

	//bool m_pulldown;


	QString getStreamDescr();
	int getWidth() const { return pic_width_in_mbs*16 - getCropX();}
	int getHeight() const {
		return (2 - frame_mbs_only_flag) * pic_height_in_map_units*16 - getCropY();
	}
	double getFPS() const;
	void setFps(double fps);


    SPSUnit()
    :
        NALUnit(),
        m_ready(false)
    {
		sar_width = sar_height = 0;
		num_units_in_tick = time_scale = 0;
		fixed_frame_rate_flag = -1;
		bit_rate_value_minus1 = 0;
		cpb_size_value_minus1 = 0;
		cbr_flag = 0;
		num_units_in_tick_bit_pos = -1;
		//orig_hrd_parameters_present_flag = 
		nal_hrd_parameters_present_flag = -1;
		vcl_hrd_parameters_present_flag = -1;
		pic_struct_present_flag = -1;
		mb_adaptive_frame_field_flag = 0;
		frame_crop_left_offset = frame_crop_right_offset = frame_crop_top_offset = frame_crop_bottom_offset = 0;
		nal_hrd_parameters_bit_pos = -1;
		full_sps_bit_len = 0;
		low_delay_hrd_flag = 0;
		bit_rate_scale = 0;
        residual_colour_transform_flag = 0;
        bit_depth_luma = 0;
        bit_depth_chroma = 0;
        qpprime_y_zero_transform_bypass_flag = 0;
        seq_scaling_matrix_present_flag = 0;
		//m_pulldown = false;

        memset( ScalingList4x4, 16, sizeof(ScalingList4x4) );
        memset( ScalingList8x8, 16, sizeof(ScalingList8x8) );
        std::fill( (bool*)UseDefaultScalingMatrix4x4Flag, ((bool*)UseDefaultScalingMatrix4x4Flag)+sizeof(UseDefaultScalingMatrix4x4Flag)/sizeof(*UseDefaultScalingMatrix4x4Flag), true );
        std::fill( (bool*)UseDefaultScalingMatrix8x8Flag, ((bool*)UseDefaultScalingMatrix8x8Flag)+sizeof(UseDefaultScalingMatrix8x8Flag)/sizeof(*UseDefaultScalingMatrix8x8Flag), true );
    }
	virtual ~SPSUnit() {
		delete [] bit_rate_value_minus1;
        bit_rate_value_minus1 = 0;
		delete [] cpb_size_value_minus1;
        cpb_size_value_minus1 = 0;
		delete [] cbr_flag;
        cbr_flag = 0;
	}
	bool isReady() {return m_ready;}
	int deserialize();
	void insertHdrParameters();
	int getMaxBitrate();

private:
	bool seq_scaling_list_present_flag[8];
	bool m_ready;
	void hrd_parameters();
	void deserializeVuiParameters();
	int getCropY() const;
	int getCropX() const;
	void serializeHDRParameters(BitStreamWriter& writer);
};

const char* const sliceTypeStr[5] = {"P_TYPE", "B_TYPE", "I_TYPE", "SP_TYPE", "SI_TYPE"};

class SEIUnit: public NALUnit {
public:
	SEIUnit(): NALUnit(), pic_struct(0), m_cpb_removal_delay_baseaddr(0), m_cpb_removal_delay_bitpos(0) {}
	virtual ~SEIUnit() {}
	void deserialize(SPSUnit& sps, int orig_hrd_parameters_present_flag);
	int cpb_removal_delay;
	int dpb_output_delay;
	int pic_struct;
	int initial_cpb_removal_delay[32];
	int initial_cpb_removal_delay_offset[32];
	QSet<int> m_processedMessages;
    QList<QPair<const quint8*, int> > m_userDataPayload;
	quint8* m_cpb_removal_delay_baseaddr;
	int m_cpb_removal_delay_bitpos;
	void serialize_pic_timing_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader);
	void serialize_buffering_period_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader);
	int updateSeiParam(SPSUnit& sps, bool removePulldown, int orig_hrd_parameters_present_flag);
private:

	void sei_payload(SPSUnit& sps, int payloadType, const quint8* curBuf, int payloadSize, int orig_hrd_parameters_present_flag);
	void buffering_period(int payloadSize) ;
	void pic_timing(SPSUnit& sps, const quint8* curBuf, int payloadSize, bool orig_hrd_parameters_present_flag);
	void pan_scan_rect(int payloadSize) ;
	void filler_payload(int payloadSize) ;
	void user_data_registered_itu_t_t35(int payloadSize) ;
	void user_data_unregistered(const quint8* data, int payloadSize);
	void recovery_point(int payloadSize) ;
	void dec_ref_pic_marking_repetition(int payloadSize) ;
	void spare_pic(int payloadSize) ;
	void scene_info(int payloadSize) ;
	void sub_seq_info(int payloadSize) ;
	void sub_seq_layer_characteristics(int payloadSize) ;
	void sub_seq_characteristics(int payloadSize) ;
	void full_frame_freeze(int payloadSize) ;
	void full_frame_freeze_release(int payloadSize) ;
	void full_frame_snapshot(int payloadSize) ;
	void progressive_refinement_segment_start(int payloadSize) ;
	void progressive_refinement_segment_end(int payloadSize) ;
	void motion_constrained_slice_group_set(int payloadSize) ;
	void film_grain_characteristics(int payloadSize) ;
	void deblocking_filter_display_preference(int payloadSize) ;
	void stereo_video_info(int payloadSize) ;
	void reserved_sei_message(int payloadSize) ;
};

class SliceUnit: public NALUnit
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
	int m_field_pic_flag;
	int memory_management_control_operation;

	SliceUnit();
	virtual ~SliceUnit() {
		;
	}
	int deserialize(quint8* buffer, quint8* end, 
							const QMap<quint32, const SPSUnit*>& spsMap,
							const QMap<quint32, const PPSUnit*>& ppsMap);

	const SPSUnit* getSPS() const {return sps;}
	const PPSUnit* getPPS() const {return pps;}

    //bool increasePicOrderFieldLen(int newLen, int oldLen);
    bool moveHeaderField(int fieldOffset, int newLen, int oldLen);

#ifndef __TS_MUXER_COMPILE_MODE

	int slice_qp_delta;
	int disable_deblocking_filter_idc;
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

	bool m_shortDeserializeMode;
    int m_frameNumBitPos;

	void copyFrom(const SliceUnit& other) {
		delete m_nalBuffer;
		//memcpy(this, &other, sizeof(SliceUnit));
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
	int serializeSliceHeader(BitStreamWriter& bitWriter, const QMap<quint32, SPSUnit*>& spsMap,
                                    const QMap<quint32, PPSUnit*>& ppsMap, quint8* dstBuffer, int dstBufferLen);

    int deserialize(const SPSUnit* sps,const PPSUnit* pps);
private:
	void write_pred_weight_table(BitStreamWriter& bitWriter);
	void write_dec_ref_pic_marking(BitStreamWriter& bitWriter);
	void write_ref_pic_list_reordering(BitStreamWriter& bitWriter);
	void macroblock_layer();
	void pred_weight_table();
	void dec_ref_pic_marking();
	void ref_pic_list_reordering();
	int NextMbAddress(int n);
#endif
private:
	const PPSUnit* pps;
	const SPSUnit* sps;
	int m_frameNumBits;
    int m_fullHeaderLen;
	int deserializeSliceHeader(const QMap<quint32, const SPSUnit*>& spsMap,const QMap<quint32, const PPSUnit*>& ppsMap);
};

namespace h264
{
    namespace AspectRatio
    {
        static const int Extended_SAR = 255;

        void decode( int aspect_ratio_idc, unsigned int* w, unsigned int* h );
    }

    namespace SEIType
    {
        enum Value
        {
            buffering_period = 0,
            pic_timing = 1,
            pan_scan_rect = 2,
            filler_payload = 3,
            user_data_registered_itu_t_t35 = 4,
            user_data_unregistered = 5,
            recovery_point = 6,
            dec_ref_pic_marking_repetition = 7,
            spare_pic = 8,
            scene_info = 9,
            sub_seq_info = 10,
            sub_seq_layer_characteristics = 11,
            sub_seq_characteristics = 12,
            full_frame_freeze = 13,
            full_frame_freeze_release = 14,
            full_frame_snapshot = 15,
            progressive_refinement_segment_start = 16,
            progressive_refinement_segment_end = 17,
            motion_constrained_slice_group_set = 18,
            film_grain_characteristics = 19,
            deblocking_filter_display_preference = 20,
            stereo_video_info = 21
        };
    }
}

#endif
