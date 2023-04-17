// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#include <QtCore/QByteArray>

/**
 * Contains fields of AVCodecParameters (and respectively of CodecParameters) which
 * are transferred from Server to Client with a media stream.
 *
 * Used in implementation of CodecParameters descendants. Can be deep-copied by
 * value. Serializable via Fusion. Does not depend on ffmpeg implementation.
 */
struct NX_MEDIA_CORE_API QnMediaContextSerializableData
{
    QByteArray serialize() const;

    /**
     * @return Success.
     * All deserialization errors (if any) are logged, and asserted if
     * configuration is debug. On failure, fields state should be considered
     * undefined.
     */
    bool deserialize(const QByteArray& data);

    /**
     * Initialize all fields copying data from the specified AVCodecParameters.
     *
     * ATTENTION: All fields are assumed to be non-initialized before the call.
     */
    void initializeFrom(const AVCodecParameters* codecParams);

    //--------------------------------------------------------------------------
    /// Fields defined in ffmpeg's AVCodecParameters.
    //@{

    AVCodecID codecId = AV_CODEC_ID_NONE;
    AVMediaType codecType = AVMEDIA_TYPE_UNKNOWN;
    QByteArray extradata;

    int channels = 0;
    int sampleRate = 0;
    AVSampleFormat sampleFmt = AV_SAMPLE_FMT_NONE;
    int bitsPerCodedSample = 0;
    int codedWidth_deprecated = 0;
    int codedHeight_deprecated = 0;
    int width = 0;
    int height = 0;
    int bitRate = 0;
    quint64 channelLayout = 0;
    int blockAlign = 0;

    //@}
};
