#ifndef _UNICLIENT_FFMPEG_CODEC_H
#define _UNICLIENT_FFMPEG_CODEC_H

#ifdef ENABLE_DATA_PROVIDERS

// ffmpeg headers
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #ifdef _USE_DXVA
    #include <libavcodec/dxva2.h>
    #endif
    #include <libswscale/swscale.h>
    #include <libavutil/avstring.h>
}

enum CLCodecType 
{
    CL_JPEG, 
    CL_H264,
    CL_WMV3,
    CL_MPEG2, 
    CL_MPEG4, 
    CL_MSVIDEO1, 
    CL_MSMPEG4V2,
    CL_MSMPEG4V3,
    CL_MPEG1VIDEO,
    CL_QTRLE,
    CL_SVQ3,
    CL_VP6F,
    CL_CINEPAK,

    CL_PCM_S16LE,
    CL_MP2,
    CL_MP3, 
    CL_AC3,
    CL_AAC, 
    CL_WMAV2, 
    CL_WMAPRO,
    CL_ADPCM_MS,
    CL_AMR_NB,
    CL_PCM_U8,
    CL_UNKNOWN, 
    CL_VARIOUSE_DECODERS
};

CLCodecType ffmpegCodecIdToInternal(CodecID ffmpegVideoCodecId);
CodecID internalCodecIdToFfmpeg(CLCodecType internalCodecId);

#endif // ENABLE_DATA_PROVIDERS

#endif // _UNICLIENT_FFMPEG_CODEC_H
