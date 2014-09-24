// nalconstructor.cpp : Defines the entry point for the console application.
//

#include <stdint.h>

typedef struct bs_s
{
	uint8_t *p_start; // pointer to the begining
	uint8_t *p;			// pointer to the current
	uint8_t *p_end; // pointer to the end

	int     i_left;    /* i_count number of available bits */
} bs_t;

static inline void bs_init( bs_t *s, uint8_t *p_data, int i_data )
{
	s->p_start = p_data;
	s->p       = p_data;
	s->p_end   = s->p + i_data;
	s->i_left  = 8;
}

static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
{
	if( s->p >= s->p_end - 4 )
		return;
	while( i_count > 0 )
	{
		if( i_count < 32 )
			i_bits &= (1<<i_count)-1;
		if( i_count < s->i_left )
		{
			*s->p = (*s->p << i_count) | i_bits;
			s->i_left -= i_count;
			break;
		}
		else
		{
			*s->p = (*s->p << s->i_left) | (i_bits >> (i_count - s->i_left));
			i_count -= s->i_left;
			s->p++;
			s->i_left = 8;
		}
	}
}

static inline void bs_write1( bs_t *s, uint32_t i_bit )
{
	if( s->p < s->p_end )
	{
		*s->p <<= 1;
		*s->p |= i_bit;
		s->i_left--;
		if( s->i_left == 0 )
		{
			s->p++;
			s->i_left = 8;
		}
	}
}

static inline void bs_align_0( bs_t *s )
{
	if( s->i_left != 8 )
	{
		*s->p <<= s->i_left;
		s->i_left = 8;
		s->p++;
	}
}
static inline void bs_align_1( bs_t *s )
{
	if( s->i_left != 8 )
	{
		*s->p <<= s->i_left;
		*s->p |= (1 << s->i_left) - 1;
		s->i_left = 8;
		s->p++;
	}
}
static inline void bs_align( bs_t *s )
{
	bs_align_0( s );
}

/* golomb functions */

static inline void bs_write_ue( bs_t *s, unsigned int val )
{
	int i_size = 0;
	static const int i_size0_255[256] =
	{
		1,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
			6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
			8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
			8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
			8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
			8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
	};

	if( val == 0 )
	{
		bs_write1( s, 1 );
	}
	else
	{
		unsigned int tmp = ++val;

		if( tmp >= 0x00010000 )
		{
			i_size += 16;
			tmp >>= 16;
		}
		if( tmp >= 0x100 )
		{
			i_size += 8;
			tmp >>= 8;
		}
		i_size += i_size0_255[tmp];

		bs_write( s, 2 * i_size - 1, val );
	}
}

static inline void bs_write_se( bs_t *s, int val )
{
	bs_write_ue( s, val <= 0 ? -val * 2 : val * 2 - 1);
}

static inline void bs_rbsp_trailing( bs_t *s )
{
	bs_write1( s, 1 );
	if( s->i_left != 8 )
	{
		bs_write( s, s->i_left, 0x00 );
	}
}

static inline void bs_write_startcode( bs_t *s)
{
	bs_align(s);
	bs_write(s,32,0x00000001);// 0x00000001 - start code.
}

//==============================================================================
// P = 1 for P frame ; P = 0 for I frame
int create_unit_delimiter(int P, unsigned char* data, int max_datalen)
{
	bs_t stream;
	bs_init(&stream,data,max_datalen); // just stream init

	bs_write_startcode(&stream); // write start code

	//int header = ( 0x00 << 7 ) | ( i_ref_idc << 5 ) | i_type;
	//for unit_delimiter header = 9 (0x9)

	bs_write(&stream,8,0x9); // header
	bs_write(&stream, 3, P); //pic_type

	bs_rbsp_trailing(&stream); // adding 0 bits till the edge of the byte

	return stream.p - stream.p_start;

}
//==============================================================================
int create_sps_pps(
				   int frameWidth,
				   int frameHeight,
				   int deblock_filter,
				   unsigned char* data, int max_datalen)
{
	bs_t stream;
	bs_init(&stream,data,max_datalen); // just stream init

	bs_write_startcode(&stream); // write start code

	//int header = ( 0x00 << 7 ) | ( i_ref_idc << 5 ) | i_type;
	//for sps header = 103 (0x67)

	bs_write(&stream,8,0x67); // header

	//======= sps main======
	bs_write( &stream, 8, 66 ); //i_profile_idc = 66

	bs_write( &stream, 1, 0 );
	bs_write( &stream, 1, 0 );
	bs_write( &stream, 1, 0 );
	bs_write( &stream, 1, 0 );

	bs_write( &stream, 4, 0 );    /* reserved_zero_4bits */

	bs_write( &stream, 8, 32 ); //i_level_idc = 32

	bs_write_ue( &stream, 0 ); //seq_parameter_set_id

	bs_write_ue( &stream, 0 ); //log2_max_frame_num_minus4 - 4 ????
	bs_write_ue( &stream, 0 ); //pic_order_cnt_type
	//if( pic_order_cnt_type == 0 )
	{
		bs_write_ue( &stream, 1 ); //log2_max_pic_order_cnt_lsb_minus4
	}

	bs_write_ue( &stream, 1 ); // num_ref_frames
	bs_write( &stream, 1, 0); //gaps_in_frame_num_value_allowed_flag

	int i_mb_width = frameWidth/16; // in first version we belive that width is devided on 16
	int i_mb_height = frameHeight/16; // in first version we belive that width is devided on 16

	bs_write_ue( &stream, i_mb_width - 1 );
	bs_write_ue( &stream, i_mb_height - 1);
	bs_write( &stream, 1, 1); //frame_mbs_only_flag

	bs_write( &stream, 1, 1); // direct_8x8_inference_flag

	bs_write( &stream, 1, 0 ); //frame_cropping_flag; in first version = 0;

	bs_write( &stream, 1, 0 ); //vui_parameters_present_flag

	bs_rbsp_trailing( &stream );
	//======= sps main======

	bs_write_startcode(&stream); // write start code
	//int header = ( 0x00 << 7 ) | ( i_ref_idc << 5 ) | i_type;
	//for sps header = 104 (0x68)
	bs_write(&stream,8,0x68); // header
	//=======pps main========

	bs_write_ue( &stream, 0); //pic_parameter_set_id
	bs_write_ue( &stream, 0); //seq_parameter_set_id
	bs_write( &stream, 1, 0 ); //cabac

	bs_write( &stream, 1, 0 ); //pic_order_present_flag
	bs_write_ue( &stream, 0 ); //num_slice_groups_minus1

	bs_write_ue( &stream, 0 ); //num_ref_idx_l0_active_minus1
	bs_write_ue( &stream, 0 ); //num_ref_idx_l1_active_minus1
	bs_write( &stream, 1, 0 ); //weighted_pred_flag
	bs_write( &stream, 2, 0 ); //weighted_bipred_idc

	bs_write_se( &stream, 0 ); //pic_init_qp_minus26
	bs_write_se( &stream, 0 ); //pic_init_qs_minus26
	bs_write_se( &stream, 0 ); //chroma_qp_index_offset

	if (deblock_filter) // if we have deblocking filter
		bs_write( &stream, 1, 0 );
	else // we have ti disable it in sh
		bs_write( &stream, 1, 1 );

	bs_write( &stream, 1, 0); //constrained_intra_pred_flag
	bs_write( &stream, 1, 0 ); //redundant_pic_cnt_present_flag

	bs_rbsp_trailing( &stream );
	//=======================

	return stream.p - stream.p_start;

}
//==============================================================================

