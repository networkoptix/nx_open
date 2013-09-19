
#ifndef CAMERA_PLUGIN_TYPES_H
#define CAMERA_PLUGIN_TYPES_H


namespace nxcip
{
    enum CompressionType
    {
        CODEC_ID_NONE,
        CODEC_ID_MPEG2VIDEO,
        CODEC_ID_H263,
        CODEC_ID_MJPEG,
        CODEC_ID_MPEG4,
        CODEC_ID_H264,
        CODEC_ID_THEORA,
        CODEC_ID_PNG,
        CODEC_ID_GIF,

        CODEC_ID_MP2 = 0x15000,
        CODEC_ID_MP3,
        CODEC_ID_AAC,
        CODEC_ID_AC3,
        CODEC_ID_DTS,
        CODEC_ID_VORBIS
    };

    enum PixelFormat
    {
        //!planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
        PIX_FMT_YUV420P,
        //!planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
        PIX_FMT_YUV422P,
        //!planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
        PIX_FMT_YUV444P,
        //!1bpp, 0 is black, 1 is white, in each byte pixels are ordered from the msb to the lsb
        PIX_FMT_MONOBLACK,
        //!packed RGB 8:8:8, 24bpp, RGBRGB...
        PIX_FMT_RGB24,
        //!planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
        PIX_FMT_NV12,
        //!packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
        PIX_FMT_BGRA,
        //!packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
        PIX_FMT_RGBA,
        //!planar YUV 4:2:0, 20bpp, (1 Cr & Cb sample per 2x2 Y & A samples)
        PIX_FMT_YUVA420P
    };
}

#endif  //CAMERA_PLUGIN_TYPES_H
