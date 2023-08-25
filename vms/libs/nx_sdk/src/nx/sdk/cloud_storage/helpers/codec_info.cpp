// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "codec_info.h"

#include <nx/sdk/cloud_storage/helpers/algorithm.h>

namespace nx::sdk::cloud_storage {

CodecInfo::CodecInfo(const CodecInfoData& codecInfo) : m_codecInfo(codecInfo)
{
    m_extradata = nx::sdk::cloud_storage::fromBase64(m_codecInfo.extradataBase64);
}

nxcip::CompressionType CodecInfo::compressionType() const
{
    return m_codecInfo.compressionType;
}

nxcip::PixelFormat CodecInfo::pixelFormat() const
{
    return m_codecInfo.pixelFormat;
}

nxcip::MediaType CodecInfo::mediaType() const
{
    return m_codecInfo.mediaType;
}

int CodecInfo::width() const
{
    return m_codecInfo.width;
}

int CodecInfo::height() const
{
    return m_codecInfo.height;
}

int64_t CodecInfo::codecTag() const
{
    return m_codecInfo.codecTag;
}

int64_t CodecInfo::bitRate() const
{
    return m_codecInfo.bitRate;
}

int CodecInfo::channels() const
{
    return m_codecInfo.channels;
}

int CodecInfo::frameSize() const
{
    return m_codecInfo.frameSize;
}

int CodecInfo::blockAlign() const
{
    return m_codecInfo.blockAlign;
}

int CodecInfo::sampleRate() const
{
    return m_codecInfo.sampleRate;
}

nxcip::SampleFormat CodecInfo::sampleFormat() const
{
    return m_codecInfo.sampleFormat;
}

int CodecInfo::bitsPerCodedSample() const
{
    return m_codecInfo.bitsPerCodedSample;
}

int64_t CodecInfo::channelLayout() const
{
    return m_codecInfo.channelLayout;
}

int CodecInfo::extradataSize() const
{
    return m_extradata.size();;
}

const uint8_t* CodecInfo::extradata() const
{
    return (const uint8_t*) m_extradata.data();
}

int CodecInfo::channelNumber() const
{
    return m_codecInfo.channelNumber;
}

} // namespace nx::sdk::cloud_storage
