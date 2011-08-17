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

    PixelFormat out_type;

    uchar *C1; // color components
    uchar *C2;
    uchar *C3;

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
    static void downscalePlate_factor2(uchar *dst, const uchar *src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor4(uchar *dst, const uchar *src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor8(uchar *dst, const uchar *src, int src_width, int src_stride, int src_height);

    static void copyPlane(uchar *dst, const uchar *src, int width, int dst_stride, int src_stride, int height);
    static bool equalPlanes(const uchar *plane1, const uchar *plane2, int width, int stride1, int stride2, int height, int max_diff);

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
