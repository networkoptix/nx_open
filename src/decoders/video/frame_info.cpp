#include "frame_info.h"

#include <string.h>
#include <stdio.h>

#define q_abs(x) ((x) >= 0 ? (x) : (-x))

CLVideoDecoderOutput::CLVideoDecoderOutput():
m_needToclean(false),
C1(0), C2(0), C3(0),
stride1(0),
width(0)
{

}

CLVideoDecoderOutput::~CLVideoDecoderOutput()
{
	if (m_needToclean)
		clean();
}

void CLVideoDecoderOutput::clean()
{
	delete[] C1;
	C1 = 0;
	m_needToclean = false;
	stride1 = 0;
	width = 0;
}


void CLVideoDecoderOutput::needToCleanUP(bool val)
{
	m_needToclean = val;
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

		dst->C1 = new unsigned char[dst->stride1*dst->height + (dst->stride2+dst->stride3)*yu_h];
		dst->C2 = dst->C1 + dst->stride1*dst->height;
		dst->C3 = dst->C2 + dst->stride2*yu_h;

		dst->needToCleanUP(true);
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
