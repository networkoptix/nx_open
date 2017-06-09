#include "utils.h"

#include <QtCore/QIODevice>

int readFromQIODevice(void* opaque, uint8_t* buf, int size)
{

    auto ioDevice = reinterpret_cast<QIODevice*>(opaque);
    auto result = ioDevice->read((char*)buf, size);

    return result;
}

nxcip::AudioFormat::SampleType fromFfmpegSampleFormatToNx(AVSampleFormat avFormat)
{
    // TODO: #dmishin maybe it's worth to extend nxcip::SampleType enum
    switch (avFormat)
    {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return nxcip::AudioFormat::SampleType::stU8;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return nxcip::AudioFormat::SampleType::stS16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return nxcip::AudioFormat::SampleType::stS32;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return nxcip::AudioFormat::SampleType::stFLT;
        default:
            return nxcip::AudioFormat::SampleType::stU8;
    }
};

nxcip::CompressionType fromFfmpegCodecIdToNx(AVCodecID codecId)
{
    switch (codecId)
    {
        case AV_CODEC_ID_H264:
            return nxcip::CompressionType::AV_CODEC_ID_H264;
        case AV_CODEC_ID_MPEG2VIDEO:
            return nxcip::CompressionType::AV_CODEC_ID_MPEG2VIDEO;
        case AV_CODEC_ID_H263:
            return nxcip::CompressionType::AV_CODEC_ID_H263;
        case AV_CODEC_ID_MJPEG:
            return nxcip::CompressionType::AV_CODEC_ID_MJPEG;
        case AV_CODEC_ID_MPEG4:
            return nxcip::CompressionType::AV_CODEC_ID_MPEG4;
        case AV_CODEC_ID_THEORA:
            return nxcip::CompressionType::AV_CODEC_ID_THEORA;
        case AV_CODEC_ID_PNG:
            return nxcip::CompressionType::AV_CODEC_ID_PNG;
        case AV_CODEC_ID_GIF:
            return nxcip::CompressionType::AV_CODEC_ID_GIF;
        case AV_CODEC_ID_MP2:
            return nxcip::CompressionType::AV_CODEC_ID_MP2;
        case AV_CODEC_ID_MP3:
            return nxcip::CompressionType::AV_CODEC_ID_MP3;
        case AV_CODEC_ID_AAC:
            return nxcip::CompressionType::AV_CODEC_ID_AAC;
        case AV_CODEC_ID_AC3:
            return nxcip::CompressionType::AV_CODEC_ID_AC3;
        case AV_CODEC_ID_DTS:
            return nxcip::CompressionType::AV_CODEC_ID_DTS;
        case AV_CODEC_ID_PCM_S16LE:
            return nxcip::CompressionType::AV_CODEC_ID_PCM_S16LE;
        case AV_CODEC_ID_PCM_MULAW:
            return nxcip::CompressionType::AV_CODEC_ID_PCM_MULAW;
        case AV_CODEC_ID_VORBIS:
            return nxcip::CompressionType::AV_CODEC_ID_VORBIS;
        default:
            return nxcip::CompressionType::AV_CODEC_ID_NONE;
    }
}
