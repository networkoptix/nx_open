#ifndef frame_info_1730
#define frame_info_1730

#include "data/mediadata.h"
#include "libavcodec/avcodec.h"

struct CLVideoDecoderOutput: public AVFrame
{
    CLVideoDecoderOutput();
    ~CLVideoDecoderOutput();

    enum downscale_factor{factor_any = 0, factor_1 = 1, factor_2 = 2, factor_4 = 4, factor_8 = 8, factor_16 = 16, factor_32 = 32, factor_64 = 64, factor_128 = 128 , factor_256 = 256};
    static void downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor);
    static void copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
    static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
    void saveToFile(const char* filename);
    void clean();
    static bool isPixelFormatSupported(PixelFormat format);
    void setUseExternalData(bool value);
    bool getUseExternalData() const { return m_useExternalData; }
    void setDisplaying(bool value) {m_displaying = value; }
    bool isDisplaying() const { return m_displaying; }
private:
    static void downscalePlate_factor2(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor4(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor8(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);

    static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
    static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);
private:
    bool m_useExternalData; // pointers only copied to this frame
    bool m_displaying;
};

/*
struct CLVideoDecoderOutput
{
	enum downscale_factor{factor_any = 0, factor_1 = 1, factor_2 = 2, factor_4 = 4, factor_8 = 8, factor_16 = 16, factor_32 = 32, factor_64 = 64, factor_128 = 128 , factor_256 = 256};

	CLVideoDecoderOutput();
	~CLVideoDecoderOutput();

	PixelFormat out_type;

	unsigned char* C1; // color components
	unsigned char* C2;
	unsigned char* C3;

	int width; // image width 
	int height;// image height 

	int stride1; // image width in memory of C1 component
	int stride2;
	int stride3;

	static void downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor);
	static void copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
	static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
	void saveToFile(const char* filename);
	void clean();
    static bool isPixelFormatSupported(PixelFormat format);
private:
	static void downscalePlate_factor2(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
	static void downscalePlate_factor4(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
	static void downscalePlate_factor8(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);

	static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
	static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);

private:
	unsigned long m_capacity; // how many bytes are allocated ( if any )
};
*/

struct CLVideoData
{
	CodecID codec; 

	//out frame info;
	//client needs only define ColorSpace out_type; decoder will setup ather variables
	//CLVideoDecoderOutput outFrame; 

	const unsigned char* inBuffer; // pointer to compressed data
	int bufferLength; // compressed data len

	// is this frame is Intra frame. for JPEG should always be true; not nesseserally to take care about it; 
	//decoder just ignores this flag
	// for user purpose only
	int keyFrame; 
	bool useTwice; // some decoders delays frame by one 

    int width; // image width 
    int height;// image height 
};

#endif //frame_info_1730

