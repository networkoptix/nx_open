#ifndef VIDEODATA_H
#define VIDEODATA_H

#include "core/datapacket/mediadatapacket.h"

struct CLVideoDecoderOutput
{
    enum downscale_factor {
        factor_any = 0,
        factor_1 = 1,
        factor_2 = 2,
        factor_4 = 4,
        factor_8 = 8,
        factor_16 = 16,
        factor_32 = 32,
        factor_64 = 64,
        factor_128 = 128,
        factor_256 = 256
    };

    CLVideoDecoderOutput();
    ~CLVideoDecoderOutput();

    void clean();

    static void copy(const CLVideoDecoderOutput *src, CLVideoDecoderOutput *dst);
    static void downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor);

    static bool imagesAreEqual(const CLVideoDecoderOutput *img1, const CLVideoDecoderOutput *img2, unsigned int max_diff);

    static bool isPixelFormatSupported(PixelFormat format);

#if _DEBUG
    void saveToFile(const char* filename);
#endif

public:
    PixelFormat out_type;

    uchar *C1; // color components
    uchar *C2;
    uchar *C3;

    int width; // image width
    int height;// image height

    int stride1; // image width in memory of C1 component
    int stride2;
    int stride3;

private:
    unsigned long m_capacity; // how many bytes are allocated ( if any )
};


struct CLVideoData
{
    int codec;

    //out frame info;
    //client needs only define ColorSpace out_type; decoder will setup ather variables
    CLVideoDecoderOutput outFrame;

    const uchar *inBuffer; // pointer to compressed data
    int bufferLength; // compressed data len

    // is this frame is Intra frame. for JPEG should always be true; not nesseserally to take care about it;
    //decoder just ignores this flag
    // for user purpose only
    int keyFrame;
    bool useTwice; // some decoders delays frame by one

    int width; // image width
    int height; // image height
};

#endif // VIDEODATA_H
