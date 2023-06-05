// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "format.h"

#include <nx/media/codec_parameters.h>
#include <nx/utils/log/format.h>

namespace {

static constexpr auto kMicrosecondsInSecond = 1000000LL;

} // namespace

using namespace std::chrono;

namespace nx::media::audio {

bool Format::isValid() const
{
    return sampleRate != -1
        && channelCount != -1
        && sampleSize != -1
        && sampleType != SampleType::unknown
        && !codec.isEmpty();
}

microseconds Format::durationForBytes(int bytes) const
{
    if (!isValid() || bytes <= 0)
        return 0ms;

    // We round the byte count to ensure whole frames
    return microseconds(kMicrosecondsInSecond * (bytes / bytesPerFrame())) / sampleRate;
}

int Format::bytesForDuration(microseconds duration) const
{
    if (!isValid() || duration <= 0ms)
        return 0;

    return int((duration.count() * bytesPerFrame() * sampleRate) / kMicrosecondsInSecond);
}

int Format::bytesPerFrame() const
{
    if (!isValid())
        return 0;

    return (sampleSize * channelCount) / 8;
}

bool Format::operator==(const Format& other) const
{
    return sampleRate == other.sampleRate
        && channelCount == other.channelCount
        && sampleSize == other.sampleSize
        && codec == other.codec
        && byteOrder == other.byteOrder
        && sampleType == other.sampleType;
}

bool Format::operator!=(const Format& other) const
{
    return !(*this == other);
}

QString Format::toString() const
{
    return NX_FMT(
        "Format(%1 Hz, %2 bit, %3 channels, sampleType %4, byteOrder %5, codec %6)",
        sampleRate,
        sampleSize,
        channelCount,
        (int) sampleType,
        (int) byteOrder,
        codec
    );
}

Format formatFromMediaContext(const CodecParametersConstPtr& c)
{
    Format result;

    if (c == nullptr)
        return result;

    if (c->getSampleRate())
        result.sampleRate = c->getSampleRate();

    if (c->getChannels())
        result.channelCount = c->getChannels();

    result.byteOrder = QSysInfo::LittleEndian;

    switch(c->getSampleFmt())
    {
        case AV_SAMPLE_FMT_U8: ///< unsigned 8 bits
        case AV_SAMPLE_FMT_U8P:
            result.sampleSize = 8;
            result.sampleType = Format::SampleType::unsignedInt;
            break;

        case AV_SAMPLE_FMT_S16: ///< signed 16 bits
        case AV_SAMPLE_FMT_S16P:
            result.sampleSize = 16;
            result.sampleType = Format::SampleType::signedInt;
            break;

        case AV_SAMPLE_FMT_S32:///< signed 32 bits
        case AV_SAMPLE_FMT_S32P:
            result.sampleSize = 32;
            result.sampleType = Format::SampleType::signedInt;
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            result.sampleSize = 32;
            result.sampleType = Format::SampleType::floatingPoint;
            break;

        default:
            result.sampleSize = 16;
            result.sampleType = Format::SampleType::signedInt;
            break;
    }

    return result;
}

} // namespace nx::media::audio
