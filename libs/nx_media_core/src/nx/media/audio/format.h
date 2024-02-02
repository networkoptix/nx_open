// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QString>
#include <QtCore/QSysInfo>

class CodecParameters;
using CodecParametersConstPtr = std::shared_ptr<const CodecParameters>;

namespace nx::media::audio {

/**
 * Description of an audio stream.
 */
struct NX_MEDIA_CORE_API Format
{
public:
    enum class SampleType {
        unknown,
        signedInt,
        unsignedInt,
        floatingPoint,
    };

    /** Samples per second of audio data in Hertz. */
    int sampleRate = -1;

    /** The number of audio channels (typically one for mono or two for stereo). */
    int channelCount = -1;

    /**  How much data is stored in each sample (typically 8 or 16 bits). */
    int sampleSize = -1;

    QString codec;

    /** Byte ordering of sample. */
    QSysInfo::Endian byteOrder = QSysInfo::ByteOrder;

    /** Numerical representation of sample. */
    SampleType sampleType = SampleType::unknown;

    bool isValid() const;

    std::chrono::microseconds durationForBytes(int bytes) const;
    int bytesForDuration(std::chrono::microseconds duration) const;

    int bytesPerFrame() const;

    bool operator==(const Format& other) const;
    bool operator!=(const Format& other) const;

    QString toString() const;
};

NX_MEDIA_CORE_API Format formatFromMediaContext(const CodecParametersConstPtr& c);

} // namespace nx::media::audio
