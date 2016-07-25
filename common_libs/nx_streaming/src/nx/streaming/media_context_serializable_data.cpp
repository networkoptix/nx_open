#include "media_context_serializable_data.h"

#include <nx/utils/log/assert.h>

#include <nx/fusion/model_functions.h>
#include <utils/media/av_codec_helper.h>

#define QnMediaContextSerializableData_Fields \
    (codecId)(codecType)(rcEq)(extradata)(intraMatrix)(interMatrix)(rcOverride)\
    (channels)(sampleRate)(sampleFmt)(bitsPerCodedSample)(codedWidth)\
    (codedHeight)(width)(height)(bitRate)(channelLayout)(blockAlign)

QN_FUSION_DECLARE_FUNCTIONS(QnMediaContextSerializableData, (ubjson))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AVCodecID)
QN_FUSION_DECLARE_FUNCTIONS(AVCodecID, (numeric))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AVMediaType)
QN_FUSION_DECLARE_FUNCTIONS(AVMediaType, (numeric))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AVSampleFormat)
QN_FUSION_DECLARE_FUNCTIONS(AVSampleFormat, (numeric))

#define RcOverride_Fields (start_frame)(end_frame)(qscale)(quality_factor)
QN_FUSION_DECLARE_FUNCTIONS(RcOverride, (ubjson))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnMediaContextSerializableData)(RcOverride), (ubjson), _Fields)

QByteArray QnMediaContextSerializableData::serialize() const
{
    QByteArray result;
    QnUbjson::serialize(*this, &result);

    return result;
}

static bool checkMatrixSize(
    const std::vector<quint16>& matrix, const char* caption)
{
    if (matrix.size() == 0 || matrix.size() == QnAvCodecHelper::kMatrixLength)
        return true;

    const QString warning11 = QString(QLatin1String(
        "QnMediaContext deserialization error: Invalid %s length: %d")).
        arg(QLatin1String(caption)).arg(matrix.size());
    NX_ASSERT(false, Q_FUNC_INFO, warning11);
    qWarning() << warning11;
    return false;
}

bool QnMediaContextSerializableData::deserialize(const QByteArray& data)
{
    QnUbjsonReader<QByteArray> stream(&data);
    if (!QnUbjson::deserialize(&stream, this))
    {
        static const char* const kWarning =
            "QnMediaContext deserialization error: Fusion has failed.";
        //NX_ASSERT(false, Q_FUNC_INFO, kWarning);
        qWarning() << kWarning;
        return false;
    }

    if (!checkMatrixSize(intraMatrix, "intra_matrix"))
        return false;
    if (!checkMatrixSize(interMatrix, "inter_matrix"))
        return false;

    return true;
}

void QnMediaContextSerializableData::initializeFrom(const AVCodecContext* context)
{
    codecId = context->codec_id;
    codecType = context->codec_type;
    if (context->rc_eq)
    {
        rcEq = QByteArray(context->rc_eq,
            (int) strlen(context->rc_eq) + 1); //< Array should include '\0'.
    }
    if (context->extradata)
    {
        extradata = QByteArray((const char*) context->extradata,
            context->extradata_size);
    }
    if (context->intra_matrix)
    {
        intraMatrix.assign(context->intra_matrix,
            context->intra_matrix + QnAvCodecHelper::kMatrixLength);
    }
    if (context->inter_matrix)
    {
        interMatrix.assign(context->inter_matrix,
            context->inter_matrix + QnAvCodecHelper::kMatrixLength);
    }
    if (context->rc_override)
    {
        rcOverride.assign(context->rc_override,
            context->rc_override + context->rc_override_count);
    }
    channels = context->channels;
    sampleRate = context->sample_rate;
    sampleFmt = context->sample_fmt;
    bitsPerCodedSample = context->bits_per_coded_sample;
    codedWidth = context->coded_width;
    codedHeight = context->coded_height;
    width = context->width;
    height = context->height;
    bitRate = context->bit_rate;
    channelLayout = context->channel_layout;
    blockAlign = context->block_align;
}
