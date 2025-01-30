// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

extern "C" {
#include <libavformat/avformat.h>
} // extern "C"

#include <camera/camera_plugin.h>

nxcip::AudioFormat::SampleType fromFfmpegSampleFormat(AVSampleFormat format)
{
    switch (format)
    {
        case AV_SAMPLE_FMT_U8:
            return nxcip::AudioFormat::stU8;
        case AV_SAMPLE_FMT_S16:
            return nxcip::AudioFormat::stS16;
        case AV_SAMPLE_FMT_S32:
            return nxcip::AudioFormat::stS32;
        case AV_SAMPLE_FMT_FLT:
            return nxcip::AudioFormat::stFLT;
        case AV_SAMPLE_FMT_DBL:
            return nxcip::AudioFormat::stDBL;
        case AV_SAMPLE_FMT_U8P:
            return nxcip::AudioFormat::stU8P;
        case AV_SAMPLE_FMT_S16P:
            return nxcip::AudioFormat::stS16P;
        case AV_SAMPLE_FMT_S32P:
            return nxcip::AudioFormat::stS32P;
        case AV_SAMPLE_FMT_FLTP:
            return nxcip::AudioFormat::stFLTP;
        case AV_SAMPLE_FMT_DBLP:
            return nxcip::AudioFormat::stDBLP;
        case AV_SAMPLE_FMT_S64:
            return nxcip::AudioFormat::stS64;
        case AV_SAMPLE_FMT_S64P:
            return nxcip::AudioFormat::stS64P;
    }
    return nxcip::AudioFormat::stNone;
}

AVSampleFormat toFfmpegSampleFormat(nxcip::AudioFormat::SampleType format)
{
    switch (format)
    {
        case nxcip::AudioFormat::stU8:
            return AV_SAMPLE_FMT_U8;
        case nxcip::AudioFormat::stS16:
            return AV_SAMPLE_FMT_S16;
        case nxcip::AudioFormat::stS32:
            return AV_SAMPLE_FMT_S32;
        case nxcip::AudioFormat::stFLT:
            return AV_SAMPLE_FMT_FLT;
        case nxcip::AudioFormat::stDBL:
            return AV_SAMPLE_FMT_DBL;
        case nxcip::AudioFormat::stU8P:
            return AV_SAMPLE_FMT_U8P;
        case nxcip::AudioFormat::stS16P:
            return AV_SAMPLE_FMT_S16P;
        case nxcip::AudioFormat::stS32P:
            return AV_SAMPLE_FMT_S32P;
        case nxcip::AudioFormat::stFLTP:
            return AV_SAMPLE_FMT_FLTP;
        case nxcip::AudioFormat::stDBLP:
            return AV_SAMPLE_FMT_DBLP;
        case nxcip::AudioFormat::stS64:
            return AV_SAMPLE_FMT_S64;
        case nxcip::AudioFormat::stS64P:
            return AV_SAMPLE_FMT_S64P;
    }
    return AV_SAMPLE_FMT_NONE;
}
