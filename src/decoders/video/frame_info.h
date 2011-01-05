#ifndef frame_info_1730
#define frame_info_1730

#include "data\mediadata.h"



enum CLColorSpace{CL_DECODER_YUV420 = 0, CL_DECODER_YUV422 = 1, CL_DECODER_YUV444 = 2 , CL_DECODER_RGB24 = 3};

struct CLVideoDecoderOutput
{
	CLVideoDecoderOutput();
	~CLVideoDecoderOutput();

	CLColorSpace out_type;

	unsigned char* C1; // color components
	unsigned char* C2;
	unsigned char* C3;

	int width; // image width 
	int height;// image height 
	

	int stride1; // image width in memory of C1 component
	int stride2;
	int stride3;


	void needToCleanUP(bool val);

	static void downscale_factor2(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
	static void downscale_factor4(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);

	static void copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
	static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
	void saveToFile(const char* filename);
	void clean();
private:
	static void downscalePlate_factor2(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
	static void downscalePlate_factor4(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);

	static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
	static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);
private:

bool m_needToclean;
};


struct CLVideoData
{
	CLCodecType codec; 

	//out frame info;
	//client needs only define ColorSpace out_type; decoder will setup ather variables
	CLVideoDecoderOutput out_frame; 

	const unsigned char* inbuf; // pointer to compressed data
	int buff_len; // compressed data len


	// is this frame is Intra frame. for JPEG should always be true; not nesseserally to take care about it; 
	//decoder just ignores this flag
	// for user purpose only
	int key_frame; 
	bool use_twice; // some decoders delays frame by one 


};


#endif //frame_info_1730


