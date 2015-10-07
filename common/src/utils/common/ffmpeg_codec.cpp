#include "ffmpeg_codec.h"

#ifdef ENABLE_DATA_PROVIDERS

CLCodecType ffmpegCodecIdToInternal(CodecID ffmpeg_video_codec_id)
{
    CLCodecType m_videoCodecId = CL_UNKNOWN;
    CLCodecType &m_audioCodecId = m_videoCodecId;

    switch(ffmpeg_video_codec_id)
    {
    case CODEC_ID_MSVIDEO1:
        m_videoCodecId = CL_MSVIDEO1;
        break;

    case CODEC_ID_MJPEG:
        m_videoCodecId =CL_JPEG;
        break;

    case CODEC_ID_MPEG2VIDEO:
        m_videoCodecId = CL_MPEG2;
        break;

    case CODEC_ID_MPEG4:
        m_videoCodecId = CL_MPEG4;
        break;

    case CODEC_ID_MSMPEG4V3:
        m_videoCodecId = CL_MSMPEG4V3;
        break;

    case CODEC_ID_QTRLE:
        m_videoCodecId = CL_QTRLE;
        break;

    case CODEC_ID_MPEG1VIDEO:
        m_videoCodecId = CL_MPEG1VIDEO;
        break;

    case CODEC_ID_MSMPEG4V2:
        m_videoCodecId = CL_MSMPEG4V2;
        break;

    case CODEC_ID_WMV3:
        m_videoCodecId = CL_WMV3;
        break;

    case CODEC_ID_SVQ3:
        m_videoCodecId = CL_SVQ3;
        break;

    case CODEC_ID_VP6F:
        m_videoCodecId = CL_VP6F;
        break;

    case CODEC_ID_CINEPAK:
        m_videoCodecId = CL_CINEPAK;
        break;

    case CODEC_ID_H264:
        m_videoCodecId = CL_H264;
        break;

    case CODEC_ID_PCM_S16LE:
        m_audioCodecId = CL_PCM_S16LE;
        break;

    case CODEC_ID_PCM_U8:
        m_audioCodecId = CL_PCM_U8;
        break;

    case CODEC_ID_MP2:
        m_audioCodecId = CL_MP2;
        break;

    case CODEC_ID_MP3:
        m_audioCodecId = CL_MP3;
        break;

    case CODEC_ID_AC3:
        m_audioCodecId = CL_AC3;
        break;

    case CODEC_ID_AAC:
        m_audioCodecId = CL_AAC;  // crashes 
        break;

    case CODEC_ID_WMAV2:
        m_audioCodecId = CL_WMAV2;
        break;

    case CODEC_ID_WMAPRO:
        m_audioCodecId = CL_WMAPRO;
        break;

    case CODEC_ID_ADPCM_MS:
        m_audioCodecId = CL_ADPCM_MS;
        break;

    case CODEC_ID_AMR_NB:
        m_audioCodecId = CL_AMR_NB;
        break;

    default:
        break;
    }

    return m_videoCodecId;
}

CodecID internalCodecIdToFfmpeg(CLCodecType internalCodecId)
{
    CodecID codec = CODEC_ID_NONE;

    switch(internalCodecId)
    {
    case CL_JPEG:
        codec = (CODEC_ID_MJPEG);
        break;

    case CL_MPEG2:
        codec = (CODEC_ID_MPEG2VIDEO);
        break;

    case CL_MPEG4:
        codec = (CODEC_ID_MPEG4);
        break;

    case CL_MSMPEG4V2:
        codec = (CODEC_ID_MSMPEG4V2);
        break;

    case CL_MSMPEG4V3:
        codec = (CODEC_ID_MSMPEG4V3);
        break;

    case CL_MPEG1VIDEO:
        codec = (CODEC_ID_MPEG1VIDEO);
        break;

    case CL_H264:
        codec = (CODEC_ID_H264);
        break;

    case CL_WMV3:
        codec = (CODEC_ID_WMV3);
        break;

    case CL_MSVIDEO1:
        codec = (CODEC_ID_MSVIDEO1);
        break;

    case CL_QTRLE:
        codec = (CODEC_ID_QTRLE);
        break;

    case CL_SVQ3:
        codec = (CODEC_ID_SVQ3);
        break;

    case CL_VP6F:
        codec = (CODEC_ID_VP6F);
        break;

    case CL_CINEPAK:
        codec = (CODEC_ID_CINEPAK);
        break;

    case CL_PCM_S16LE:
        codec = (CODEC_ID_PCM_S16LE);
        break;

    case CL_PCM_U8:
        codec = (CODEC_ID_PCM_U8);
        break;


    case CL_MP2:
        codec = (CODEC_ID_MP2);
        break;

    case CL_MP3:
        codec = (CODEC_ID_MP3);
        break;

    case CL_AC3:
        codec = (CODEC_ID_AC3);
        break;

    case CL_AAC:
        codec = (CODEC_ID_AAC);
        break;

    case CL_WMAPRO:
        codec = (CODEC_ID_WMAPRO);
        break;

    case CL_WMAV2:
        codec = (CODEC_ID_WMAV2);
        break;

    case CL_ADPCM_MS:
        codec = (CODEC_ID_ADPCM_MS);
        break;

    case CL_AMR_NB:
        codec = (CODEC_ID_AMR_NB);
        break;

    default:
        break;
    }

    return codec;

}

#endif // ENABLE_DATA_PROVIDERS
