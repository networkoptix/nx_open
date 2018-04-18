
#ifndef CAMERA_PLUGIN_TYPES_H
#define CAMERA_PLUGIN_TYPES_H


namespace nxcip
{
    enum CompressionType
    {
        AV_CODEC_ID_NONE,
        AV_CODEC_ID_MPEG2VIDEO,
        AV_CODEC_ID_H263,
        AV_CODEC_ID_MJPEG,
        AV_CODEC_ID_MPEG4,
        AV_CODEC_ID_H264,
        AV_CODEC_ID_THEORA,
        AV_CODEC_ID_PNG,
        AV_CODEC_ID_GIF,

        AV_CODEC_ID_MP2 = 0x15000,
        AV_CODEC_ID_MP3,
        AV_CODEC_ID_AAC,
        AV_CODEC_ID_AC3,
        AV_CODEC_ID_DTS,
        //raw 16-bit little-endian PCM
        AV_CODEC_ID_PCM_S16LE,
        AV_CODEC_ID_PCM_MULAW,
        AV_CODEC_ID_VORBIS
    };

    enum PixelFormat
    {
        //!planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
        AV_PIX_FMT_YUV420P,
        //!planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
        AV_PIX_FMT_YUV422P,
        //!planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
        AV_PIX_FMT_YUV444P,
        //!1bpp, 0 is black, 1 is white, in each byte pixels are ordered from the msb to the lsb
        AV_PIX_FMT_MONOBLACK,
        //!Y plane only, 8bpp
        AV_PIX_FMT_GRAY8,
        //!packed RGB 8:8:8, 24bpp, RGBRGB...
        AV_PIX_FMT_RGB24,
        //!planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
        AV_PIX_FMT_NV12,
        //!packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
        AV_PIX_FMT_BGRA,
        //!packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
        AV_PIX_FMT_RGBA,
        //!planar YUV 4:2:0, 20bpp, (1 Cr & Cb sample per 2x2 Y & A samples)
        AV_PIX_FMT_YUVA420P
    };
}

#endif  //CAMERA_PLUGIN_TYPES_H
