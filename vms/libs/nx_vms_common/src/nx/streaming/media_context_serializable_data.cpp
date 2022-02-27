// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_context_serializable_data.h"

#include <nx/utils/log/assert.h>

#include <nx/fusion/model_functions.h>
#include <utils/media/av_codec_helper.h>

#define QnMediaContextSerializableData_Fields \
    (codecId)(codecType)(rcEq_deprecated)(extradata)(intraMatrix_deprecated)(interMatrix_deprecated)(rcOverride_deprecated)\
    (channels)(sampleRate)(sampleFmt)(bitsPerCodedSample)(codedWidth_deprecated)\
    (codedHeight_deprecated)(width)(height)(bitRate)(channelLayout)(blockAlign)

QN_FUSION_DECLARE_FUNCTIONS(QnMediaContextSerializableData, (ubjson))

#define RcOverride_Fields (start_frame)(end_frame)(qscale)(quality_factor)
QN_FUSION_DECLARE_FUNCTIONS(RcOverride, (ubjson))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnMediaContextSerializableData, (ubjson), QnMediaContextSerializableData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RcOverride, (ubjson), RcOverride_Fields)

QByteArray QnMediaContextSerializableData::serialize() const
{
    QByteArray result;
    QnUbjson::serialize(*this, &result);

    return result;
}

bool QnMediaContextSerializableData::deserialize(const QByteArray& data)
{
    QnUbjsonReader<QByteArray> stream(&data);
    if (!QnUbjson::deserialize(&stream, this))
    {
        static const char* const kWarning =
            "QnMediaContext deserialization error: Fusion has failed.";
        //NX_ASSERT(false, kWarning);
        qWarning() << kWarning;
        return false;
    }

    return true;
}

void QnMediaContextSerializableData::initializeFrom(const AVCodecParameters* codecParams)
{
    codecId = codecParams->codec_id;
    codecType = codecParams->codec_type;
    if (codecParams->extradata)
    {
        extradata = QByteArray((const char*) codecParams->extradata,
            codecParams->extradata_size);
    }
    channels = codecParams->channels;
    sampleRate = codecParams->sample_rate;
    sampleFmt = (AVSampleFormat)codecParams->format;
    bitsPerCodedSample = codecParams->bits_per_coded_sample;
    width = codecParams->width;
    height = codecParams->height;
    bitRate = codecParams->bit_rate;
    channelLayout = codecParams->channel_layout;
    blockAlign = codecParams->block_align;
}
