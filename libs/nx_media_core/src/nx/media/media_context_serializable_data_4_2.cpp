// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_context_serializable_data_4_2.h"

#include <nx/fusion/model_functions.h>
#include <nx/media/av_codec_helper.h>
#include <nx/utils/log/assert.h>

#define QnMediaContextSerializableData_4_2_Fields \
    (codecId)(codecType)(rcEq_deprecated)(extradata)(intraMatrix_deprecated)(interMatrix_deprecated)(rcOverride_deprecated)\
    (channels)(sampleRate)(sampleFmt)(bitsPerCodedSample)(codedWidth_deprecated)\
    (codedHeight_deprecated)(width)(height)(bitRate)(channelLayout)(blockAlign)

QN_FUSION_DECLARE_FUNCTIONS(QnMediaContextSerializableData_4_2, (ubjson))

#define RcOverride_Fields (start_frame)(end_frame)(qscale)(quality_factor)
QN_FUSION_DECLARE_FUNCTIONS(RcOverride, (ubjson))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnMediaContextSerializableData_4_2, (ubjson), QnMediaContextSerializableData_4_2_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RcOverride, (ubjson), RcOverride_Fields)

bool QnMediaContextSerializableData_4_2::deserialize(const QByteArray& data)
{
    QnUbjsonReader<QByteArray> stream(&data);
    return QnUbjson::deserialize(&stream, this);
}

QByteArray QnMediaContextSerializableData_4_2::serialize() const
{
    QByteArray result;
    QnUbjson::serialize(*this, &result);
    return result;
}

void QnMediaContextSerializableData_4_2::initializeFrom(const AVCodecParameters* codecParams)
{
    codecId = codecParams->codec_id;
    codecType = codecParams->codec_type;
    if (codecParams->extradata)
    {
        extradata = QByteArray((const char*)codecParams->extradata,
            codecParams->extradata_size);
    }
    channels = codecParams->ch_layout.nb_channels;
    sampleRate = codecParams->sample_rate;
    sampleFmt = (AVSampleFormat)codecParams->format;
    bitsPerCodedSample = codecParams->bits_per_coded_sample;
    codedWidth_deprecated = width = codecParams->width;
    codedHeight_deprecated = height = codecParams->height;
    bitRate = codecParams->bit_rate;
    channelLayout = codecParams->ch_layout.u.mask;
    blockAlign = codecParams->block_align;
}
