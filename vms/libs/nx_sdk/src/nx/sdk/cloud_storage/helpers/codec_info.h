// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/helpers/ref_countable.h>

#include "data.h"

namespace nx::sdk::cloud_storage {

class CodecInfo: public nx::sdk::RefCountable<ICodecInfo>
{
public:
    CodecInfo(const CodecInfoData& codecInfo);
    virtual nxcip::CompressionType compressionType() const override;
    virtual nxcip::PixelFormat pixelFormat() const override;
    virtual nxcip::MediaType mediaType() const override;
    virtual int width() const override;
    virtual int height() const override;
    virtual int64_t codecTag() const override;
    virtual int64_t bitRate() const override;
    virtual int channels() const override;
    virtual int frameSize() const override;
    virtual int blockAlign() const override;
    virtual int sampleRate() const override;
    virtual nxcip::SampleFormat sampleFormat() const override;
    virtual int bitsPerCodedSample() const override;
    virtual int64_t channelLayout() const override;
    virtual int extradataSize() const override;
    virtual const uint8_t* extradata() const override;
    virtual int channelNumber() const override;

private:
    CodecInfoData m_codecInfo;
    std::vector<uint8_t> m_extradata;
};

} // namespace nx::sdk::cloud_storage
