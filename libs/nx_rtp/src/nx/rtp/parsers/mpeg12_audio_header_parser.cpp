// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mpeg12_audio_header_parser.h"

#include <map>

#include <nx/utils/bit_stream.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

static constexpr int kFrameSyncBits = 11;
static constexpr int kMpegAudioVersionBits = 2;
static constexpr int kMpegLayerBits = 2;
static constexpr int kProtecitionBit = 1;
static constexpr int kBitrateIndexBits = 4;
static constexpr int kSamplingFrequencyIndexBits = 2;
static constexpr int kPaddingBit = 1;
static constexpr int kPrivateBit = 1;
static constexpr int kChannelModeBits = 2;

static constexpr int kFrameSyncSequenceValue = 0x7ff;
static constexpr int kFullMpegAudioHeaderSize = 32;

using BitrateIndex = uint32_t;
using BitrateKbps = int;

using BitrateTable =
    std::map<MpegAudioVersion, std::map<MpegLayer, std::map<BitrateIndex, BitrateKbps>>>;

static const BitrateTable kBitrateTable = {{
    MpegAudioVersion::mpeg1, {{
        MpegLayer::layer1, {
            {1, 32}, {2, 64}, {3, 96}, {4, 128}, {5, 160}, {6, 192}, {7, 224}, {8, 256}, {9, 288},
            {10, 320}, {11, 352}, {12, 384}, {13, 416}, {14, 448}}
        }, {
        MpegLayer::layer2, {
            {1, 32}, {2, 48}, {3, 56}, {4, 64}, {5, 80}, {6, 96}, {7, 112}, {8, 128}, {9, 160},
            {10, 192}, {11, 224}, {12, 256}, {13, 320}, {14, 384}}
        }, {
        MpegLayer::layer3, {
            {1, 32}, {2, 40}, {3, 48}, {4, 56}, {5, 64}, {6, 80}, {7, 96}, {8, 112}, {9, 128},
            {10, 160}, {11, 192}, {12, 224}, {13, 256}, {14, 320}}
        }}
    }, {
    MpegAudioVersion::mpeg2, {{
        MpegLayer::layer1, {
            {1, 32}, {2, 48}, {3, 56}, {4, 64}, {5, 80}, {6, 96}, {7, 112}, {8, 128}, {9, 144},
            {10, 160}, {11, 176}, {12, 192}, {13, 224}, {14, 256}}
        }, {
        MpegLayer::layer2, {
            {1, 8}, {2, 16}, {3, 24}, {4, 32}, {5, 40}, {6, 48}, {7, 56}, {8, 64}, {9, 80},
            {10, 96}, {11, 112}, {12, 128}, {13, 144}, {14, 160}}}
        }
    }
};

using SamplingRateIndex = uint32_t;
using SamplingRate = int;

using SamplingRateTable = std::map<MpegAudioVersion, std::map<SamplingRateIndex, SamplingRate>>;

static const SamplingRateTable kSamplingRateTable = {{
    MpegAudioVersion::mpeg1, {{0, 44100}, {1, 48000}, {2, 32000}}}, {
    MpegAudioVersion::mpeg2, {{0, 22050}, {1, 24000}, {2, 16000}}}, {
    MpegAudioVersion::mpeg25, {{0, 11025}, {1, 12000}, {2, 8000}}}
};

static MpegAudioVersion toMpegAudioVersion(uint32_t mpegAudioVersionEncoded)
{
    switch (mpegAudioVersionEncoded)
    {
        case 0: return MpegAudioVersion::mpeg25;
        case 2: return MpegAudioVersion::mpeg2;
        case 3: return MpegAudioVersion::mpeg1;
        default: return MpegAudioVersion::undefined;
    }
}

static MpegLayer toMpegLayer(uint32_t mpegLayerEncoded)
{
    switch (mpegLayerEncoded)
    {
        case 1: return MpegLayer::layer3;
        case 2: return MpegLayer::layer2;
        case 3: return MpegLayer::layer1;
        default: return MpegLayer::undefined;
    }
}

static MpegChannelMode toMpegChannelMode(uint32_t mpegChannelModeEncoded)
{
    switch (mpegChannelModeEncoded)
    {
        case 0: return MpegChannelMode::stereo;
        case 1: return MpegChannelMode::jointStereo;
        case 2: return MpegChannelMode::dualChannel;
        case 3: return MpegChannelMode::singleChannel;
        default: return MpegChannelMode::undefined;
    }
}

static std::optional<BitrateKbps> toBitrateKbps(
    MpegAudioVersion mpegAudioVersion, MpegLayer mpegLayer, BitrateIndex bitrateIndex)
{
    if (!NX_ASSERT(mpegAudioVersion != MpegAudioVersion::undefined))
        return std::nullopt;

    if (!NX_ASSERT(mpegLayer != MpegLayer::undefined))
        return std::nullopt;

    if (mpegAudioVersion == MpegAudioVersion::mpeg25)
        mpegAudioVersion = MpegAudioVersion::mpeg2; //< Bitrate tables are the same for v2 and v2.5.

    const auto mpegAudioVersionIt = kBitrateTable.find(mpegAudioVersion);
    if (!NX_ASSERT(mpegAudioVersionIt != kBitrateTable.cend()))
        return std::nullopt;

    if (mpegAudioVersion == MpegAudioVersion::mpeg2 && mpegLayer == MpegLayer::layer3)
        mpegLayer = MpegLayer::layer2; //< Bitrate tables are the same for MPEG-2 layer 2 and 3.

    const auto mpegLayerIt = mpegAudioVersionIt->second.find(mpegLayer);
    if (!NX_ASSERT(mpegLayerIt != mpegAudioVersionIt->second.cend()))
        return std::nullopt;

    const auto bitrateIt = mpegLayerIt->second.find(bitrateIndex);
    if (bitrateIt == mpegLayerIt->second.cend())
        return std::nullopt;

    return bitrateIt->second;
}

static std::optional<SamplingRate> toSamplingRate(
    MpegAudioVersion mpegAudioVersion, SamplingRateIndex samplingRateIndex)
{
    if (!NX_ASSERT(mpegAudioVersion != MpegAudioVersion::undefined))
        return std::nullopt;

    const auto mpegAudioVersionIt = kSamplingRateTable.find(mpegAudioVersion);
    if (!NX_ASSERT(mpegAudioVersionIt != kSamplingRateTable.cend()))
        return std::nullopt;

    const auto samplingRateIt = mpegAudioVersionIt->second.find(samplingRateIndex);
    if (samplingRateIt == mpegAudioVersionIt->second.cend())
        return std::nullopt;

    return samplingRateIt->second;
}

/*static*/
std::optional<Mpeg12AudioHeader> Mpeg12AudioHeaderParser::parse(
    const uint8_t* headerStart, int bufferLength)
{
    if (bufferLength < kFullMpegAudioHeaderSize)
    {
        NX_VERBOSE(NX_SCOPE_TAG,
            "Buffer length is less than MPEG audio header size: %1, expected at least %2",
            bufferLength, kFullMpegAudioHeaderSize);
        return std::nullopt;
    }

    Mpeg12AudioHeader result;
    nx::utils::BitStreamReader reader(headerStart, bufferLength);

    try
    {
        const uint32_t frameSyncSequence = reader.getBits(kFrameSyncBits);
        if (frameSyncSequence != kFrameSyncSequenceValue)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Wrong frame synchronization sequence: %1, expected %2",
                frameSyncSequence, kFrameSyncSequenceValue);
            return std::nullopt;
        }

        const uint32_t mpegAudioVersionEncoded = reader.getBits(kMpegAudioVersionBits);
        result.version = toMpegAudioVersion(mpegAudioVersionEncoded);

        if (result.version == MpegAudioVersion::undefined)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Unable to determine MPEG version, encoded value: %1",
                mpegAudioVersionEncoded);
            return std::nullopt;
        }

        const uint32_t mpegLayerEncoded = reader.getBits(kMpegLayerBits);
        result.layer = toMpegLayer(mpegLayerEncoded);

        if (result.layer == MpegLayer::undefined)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Unable to determine MPEG layer, encoded value: %1",
                mpegLayerEncoded);
            return std::nullopt;
        }

        reader.skipBits(kProtecitionBit);

        const uint32_t bitrateIndex = reader.getBits(kBitrateIndexBits);
        const std::optional<BitrateKbps> bitrateKbps =
            toBitrateKbps(result.version, result.layer, bitrateIndex);

        if (!bitrateKbps)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Unable to determine bitrate, bitrate index: %1",
                bitrateIndex);
            return std::nullopt;
        }

        result.bitrateKbps = *bitrateKbps;

        const uint32_t samplingRateIndex = reader.getBits(kSamplingFrequencyIndexBits);
        const std::optional<SamplingRate> samplingRate =
            toSamplingRate(result.version, samplingRateIndex);

        if (!samplingRate)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Unable to determine sampling rate, sampling rate index: %1",
                samplingRateIndex);
            return std::nullopt;
        }

        result.samplingRate = *samplingRate;
        result.isPadded = reader.getBits(kPaddingBit);

        reader.skipBits(kPrivateBit);

        const uint32_t channelModeEncoded = reader.getBits(kChannelModeBits);
        result.channelMode = toMpegChannelMode(channelModeEncoded);

        if (result.channelMode == MpegChannelMode::undefined)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Unable to determine channel mode, encoded channel mode: %1",
                channelModeEncoded);
            return std::nullopt;
        }
    }
    catch (const nx::utils::BitStreamException& exception)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "An exception has occurred while parsing MPEG audio header: %1",
            exception.what());
        return std::nullopt;
    }

    return result;
}

} // namespace nx::rtp
