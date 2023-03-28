// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <optional>

namespace nx::rtp {

enum class MpegAudioVersion
{
    undefined,
    mpeg1,
    mpeg2,
    mpeg25,
};

enum class MpegLayer
{
    undefined,
    layer1,
    layer2,
    layer3,
};

enum class MpegChannelMode
{
    undefined,
    stereo,
    jointStereo,
    dualChannel,
    singleChannel,
};

struct Mpeg12AudioHeader
{
    MpegAudioVersion version = MpegAudioVersion::undefined;
    MpegLayer layer = MpegLayer::undefined;
    int bitrateKbps = 0;
    int samplingRate = 0;
    bool isPadded = false;
    MpegChannelMode channelMode = MpegChannelMode::undefined;
};

class NX_RTP_API Mpeg12AudioHeaderParser
{
public:
    static std::optional<Mpeg12AudioHeader> parse(const uint8_t* headerStart, int bufferLength);
};

} // namespace nx::rtp
