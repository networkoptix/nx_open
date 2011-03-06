#include "frame_info.h"

#include <string.h>
#include <stdio.h>

#define q_abs(x) ((x) >= 0 ? (x) : (-x))

CLVideoDecoderOutput::CLVideoDecoderOutput():
m_capacity(0),
C1(0), C2(0), C3(0),
stride1(0),
width(0)
{

}

CLVideoDecoderOutput::~CLVideoDecoderOutput()
{
	if (m_capacity)
		clean();
}

void CLVideoDecoderOutput::clean()
{
	if (m_capacity)
	{
		delete[] C1;
		C1 = 0;
		m_capacity = 0;
	}
}

void CLVideoDecoderOutput::copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst)
{
	if (src->width != dst->width || src->height != dst->height || src->out_type != dst->out_type)
	{
		// need to realocate dst memory 
		dst->clean();
		dst->stride1 = src->width;
		dst->stride2 = src->width/2;
		dst->stride3 = src->width/2;

		dst->width = src->width;

		dst->height = src->height;
		dst->out_type = src->out_type;

		int yu_h = dst->out_type == CL_DECODER_YUV420 ? dst->height/2 : dst->height;

		dst->m_capacity = dst->stride1*dst->height + (dst->stride2+dst->stride3)*yu_h;

		dst->C1 = new unsigned char[dst->m_capacity];
		dst->C2 = dst->C1 + dst->stride1*dst->height;
		dst->C3 = dst->C2 + dst->stride2*yu_h;

	}

	int yu_h = dst->out_type == CL_DECODER_YUV420 ? dst->height/2 : dst->height;

	copyPlane(dst->C1, src->C1, dst->stride1, dst->stride1, src->stride1, src->height);
	copyPlane(dst->C2, src->C2, dst->stride2, dst->stride2, src->stride2, yu_h);
	copyPlane(dst->C3, src->C3, dst->stride3, dst->stride3, src->stride3, yu_h);

}

void CLVideoDecoderOutput::copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride,  int src_stride, int height)
{

	for (int i = 0; i < height; ++i)
	{
		memcpy(dst, src, width);
		dst+=dst_stride;
		src+=src_stride;
	}
}

void CLVideoDecoderOutput::downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor)
{

	int src_width = src->width;
	int src_height = src->height;

	const int chroma_h_factor = (src->out_type == CL_DECODER_YUV420 || src->out_type == CL_DECODER_YUV422) ? 2 : 1;
	const int chroma_v_factor = (src->out_type == CL_DECODER_YUV420) ? 2 : 1;

	// after downscale chroma_width must be divisible by 4 ( opengl requirements )
	const int mod_w = chroma_h_factor*factor*4;
	if ((src_width%mod_w) != 0)
		src_width = src_width/mod_w*mod_w;

	dst->stride1 = src_width/factor;
	dst->stride2 = src_width/chroma_h_factor/factor;
	dst->stride3 = dst->stride2;

	dst->width = dst->stride1;

	dst->height = src_height/factor;
	dst->out_type = src->out_type;

	int yu_h = dst->height/chroma_v_factor;

	unsigned int new_min_capacity = dst->stride1*dst->height + (dst->stride2+dst->stride3)*yu_h;

	if (dst->m_capacity<new_min_capacity)
	{
		dst->clean();
		dst->m_capacity = new_min_capacity;
		dst->C1 = new unsigned char[dst->m_capacity];
	}

	dst->C2 = dst->C1 + dst->stride1*dst->height;
	dst->C3 = dst->C2 + dst->stride2*yu_h;

	int src_yu_h = src_height/chroma_v_factor;

	if (factor == factor_2)
	{
		downscalePlate_factor2(dst->C1, src->C1, src_width, src->stride1, src_height);
		downscalePlate_factor2(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
		downscalePlate_factor2(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
	}
	else if(factor == factor_4)
	{
		downscalePlate_factor4(dst->C1, src->C1, src_width, src->stride1, src->height);
		downscalePlate_factor4(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
		downscalePlate_factor4(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
	}
	else if(factor == factor_8)
	{
		downscalePlate_factor8(dst->C1, src->C1, src_width, src->stride1, src->height);
		downscalePlate_factor8(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
		downscalePlate_factor8(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
	}

}

void CLVideoDecoderOutput::downscalePlate_factor2(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height)
{
	const unsigned char* src_line1 = src;
	const unsigned char* src_line2 = src + src_stride;

	for (int y = 0; y < src_height/2; ++y)
	{
		for (int x = 0; x < src_width/2; ++x)
		{
			*dst = ( (unsigned int)*src_line1 + *(src_line1+1) + (unsigned int)*src_line2 + *(src_line2+1) + 2 ) >> 2;
			src_line1+=2;
			src_line2+=2;

			++dst;
		}

		src_line1-= src_width;
		src_line1+=(src_stride<<1);

		src_line2-= src_width;
		src_line2+=(src_stride<<1);
	}
}

void CLVideoDecoderOutput::downscalePlate_factor4(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height)
{
	const unsigned char* src_line1 = src;
	const unsigned char* src_line2 = src + 3*src_stride;

	for (int y = 0; y < src_height/4; ++y)
	{
		for (int x = 0; x < src_width/4; ++x)
		{
			*dst = ( (unsigned int)*src_line1 + *(src_line1+3) + (unsigned int)*src_line2 + *(src_line2+3) + 2 ) >> 2;
			src_line1+=4;
			src_line2+=4;

			++dst;
		}

		src_line1-= src_width;
		src_line1+=(src_stride<<2);

		src_line2-= src_width;
		src_line2+=(src_stride<<2);
	}
}

void CLVideoDecoderOutput::downscalePlate_factor8(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height)
{
	const unsigned char* src_line1 = src;
	const unsigned char* src_line2 = src + 7*src_stride;

	for (int y = 0; y < src_height/8; ++y)
	{
		for (int x = 0; x < src_width/8; ++x)
		{
			*dst = ( (unsigned int)*src_line1 + *(src_line1+7) + (unsigned int)*src_line2 + *(src_line2+7) + 2 ) >> 2;
			src_line1+=8;
			src_line2+=8;

			++dst;
		}

		src_line1-= src_width;
		src_line1+=(src_stride<<3);

		src_line2-= src_width;
		src_line2+=(src_stride<<3);
	}
}

bool CLVideoDecoderOutput::imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff)
{
	if (img1->width!=img2->width || img1->height!=img2->height || img1->out_type != img2->out_type) 
		return false;

	if (!equalPlanes(img1->C1, img2->C1, img1->width, img1->stride1, img2->stride1, img1->height, max_diff))
		return false;

	int uv_h = img1->out_type == CL_DECODER_YUV420 ? img1->height/2 : img1->height;

	if (!equalPlanes(img1->C2, img2->C2, img1->width/2, img1->stride2, img2->stride2, uv_h, max_diff))
		return false;

	if (!equalPlanes(img1->C3, img2->C3, img1->width/2, img1->stride3, img2->stride3, uv_h, max_diff))
		return false;

	return true;
}

bool CLVideoDecoderOutput::equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, 
									   int stride1, int stride2, int height, int max_diff)
{
	const unsigned char* p1 = plane1;
	const unsigned char* p2 = plane2;

	int difs = 0;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			if (q_abs (*p1 - *p2) > max_diff )
			{
				++difs;
				if (difs==2)
					return false;

			}
			else
			{
				difs = 0;
			}

			++p1;
			++p2;
		}

		p1+=(stride1-width);
		p2+=(stride2-width);
	}

	return true;
}

void CLVideoDecoderOutput::saveToFile(const char* filename)
{

	static bool first_time  = true;
	FILE * out = 0;

	if (first_time)
	{
		out = fopen(filename, "wb");
		first_time = false;
	}
	else
		out = fopen(filename, "ab");

	if (!out) return;

	//Y
	unsigned char* p = C1;
	for (int i = 0; i < height; ++i)
	{
		fwrite(p, 1, width, out);
		p+=stride1;
	}

	//U
	p = C2;
	for (int i = 0; i < height/2; ++i)
	{
		fwrite(p, 1, width/2, out);
		p+=stride2;
	}

	//V
	p = C3;
	for (int i = 0; i < height/2; ++i)
	{
		fwrite(p, 1, width/2, out);
		p+=stride3;
	}

}
