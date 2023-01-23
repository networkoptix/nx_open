// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>
#include <camera/camera_plugin.h>

namespace nx::sdk {
namespace archive {

struct Rational
{
    int num;
    int den;
};

class ICodecInfo: public Interface<ICodecInfo>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::ICodecInfo"); }

    virtual nxcip::CompressionType compressionType() const = 0;
    virtual nxcip::PixelFormat pixelFormat() const = 0;
    virtual nxcip::MediaType mediaType() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual int64_t codecTag() const = 0;
    virtual int64_t bitRate() const = 0;
    virtual int channels() const = 0;
    virtual int frameSize() const = 0;
    virtual int blockAlign() const = 0;
    virtual int sampleRate() const = 0;
    virtual nxcip::SampleFormat sampleFormat() const = 0;
    virtual int bitsPerCodedSample() const = 0;
    virtual int64_t channelLayout() const = 0;
    virtual int extradataSize() const = 0;
    virtual const uint8_t* extradata() const = 0;

    // Corresponds to MediaDataPacket::channelNumber() passed to StreamWriter::putData.
    virtual int channelNumber() const = 0;
};

} // namespace archive
} // namespace nx::sdk
