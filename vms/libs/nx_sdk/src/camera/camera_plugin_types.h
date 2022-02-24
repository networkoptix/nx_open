// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


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
        AV_CODEC_ID_HEVC,
        AV_CODEC_ID_VP8,
        AV_CODEC_ID_VP9,
        #define AV_CODEC_ID_H265 AV_CODEC_ID_HEVC

        AV_CODEC_ID_MP2 = 0x15000,
        AV_CODEC_ID_MP3,
        AV_CODEC_ID_AAC,
        AV_CODEC_ID_AC3,
        AV_CODEC_ID_DTS,
        //raw 16-bit little-endian PCM
        AV_CODEC_ID_PCM_S16LE,
        AV_CODEC_ID_PCM_MULAW,
        AV_CODEC_ID_VORBIS,
        AV_CODEC_ID_TEXT,
        AV_CODEC_ID_BIN_DATA,
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
        AV_PIX_FMT_YUVA420P,
        AV_PIX_FMT_YUVJ420P,
        AV_PIX_FMT_NONE,
    };

    enum MediaType
    {
        AVMEDIA_TYPE_UNKNOWN = -1,  ///< Usually treated as AVMEDIA_TYPE_DATA
        AVMEDIA_TYPE_VIDEO,
        AVMEDIA_TYPE_AUDIO,
        AVMEDIA_TYPE_DATA,          ///< Opaque data information usually continuous
        AVMEDIA_TYPE_SUBTITLE,
        AVMEDIA_TYPE_ATTACHMENT,    ///< Opaque data information usually sparse
        AVMEDIA_TYPE_NB
    };

    enum SampleFormat
    {
        AV_SAMPLE_FMT_NONE = -1,
        AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
        AV_SAMPLE_FMT_S16,         ///< signed 16 bits
        AV_SAMPLE_FMT_S32,         ///< signed 32 bits
        AV_SAMPLE_FMT_FLT,         ///< float
        AV_SAMPLE_FMT_DBL,         ///< double

        AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
        AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
        AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
        AV_SAMPLE_FMT_FLTP,        ///< float, planar
        AV_SAMPLE_FMT_DBLP,        ///< double, planar
        AV_SAMPLE_FMT_S64,         ///< signed 64 bits
        AV_SAMPLE_FMT_S64P,        ///< signed 64 bits, planar


        AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
    };

    struct RcOverride
    {
        int start_frame;
        int end_frame;
        int qscale;
        float quality_factor;
    };
}

#endif  //CAMERA_PLUGIN_TYPES_H
