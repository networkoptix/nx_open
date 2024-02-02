// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#include <QtCore/QByteArray>

/**
 * Copy of Media context struct from vms_4.2 for backward compatibility.
 * Contains fields of AVCodecParameters (and respectively of CodecParameters) which
 * are transferred from Server to Client with a media stream.
 *
 * Used in implementation of CodecParameters descendants. Can be deep-copied by
 * value. Serializable via Fusion. Does not depend on ffmpeg implementation.
 */
struct NX_MEDIA_CORE_API QnMediaContextSerializableData_4_2
{
    /**
     * @return Success.
     * All deserialization errors (if any) are logged, and asserted if
     * configuration is debug. On failure, fields state should be considered
     * undefined.
     */
    bool deserialize(const QByteArray& data);

    QByteArray serialize() const;

    void initializeFrom(const AVCodecParameters* codecParams);

    //--------------------------------------------------------------------------
    /// Fields defined in ffmpeg's AVCodecParameters.
    //@{

    AVCodecID codecId = AV_CODEC_ID_NONE;
    AVMediaType codecType = AVMEDIA_TYPE_UNKNOWN;
    QByteArray rcEq_deprecated; ///< Deprecated fields are needed for backward compatibility for mobile client
    QByteArray extradata;

    /// Length is 0 or QnAvCodecHelper::kMatrixLength.
    std::vector<quint16> intraMatrix_deprecated;

    /// Length is 0 or QnAvCodecHelper::kMatrixLength.
    std::vector<quint16> interMatrix_deprecated;

    std::vector<RcOverride> rcOverride_deprecated;

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
