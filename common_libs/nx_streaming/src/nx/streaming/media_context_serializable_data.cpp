#include "media_context_serializable_data.h"

#include <nx/utils/log/assert.h>

#include <utils/common/model_functions.h>
#include <utils/media/av_codec_helper.h>

#define QnMediaContextSerializableData_Fields \
    (codecId)(codecType)(rcEq)(extradata)(intraMatrix)(interMatrix)(rcOverride)\
    (channels)(sampleRate)(sampleFmt)(bitsPerCodedSample)(codedWidth)\
    (codedHeight)(width)(height)(bitRate)(channelLayout)(blockAlign)

QN_FUSION_DECLARE_FUNCTIONS(QnMediaContextSerializableData, (ubjson))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CodecID)
QN_FUSION_DECLARE_FUNCTIONS(CodecID, (numeric))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AVMediaType)
QN_FUSION_DECLARE_FUNCTIONS(AVMediaType, (numeric))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AVSampleFormat)
QN_FUSION_DECLARE_FUNCTIONS(AVSampleFormat, (numeric))

#define RcOverride_Fields (start_frame)(end_frame)(qscale)(quality_factor)
QN_FUSION_DECLARE_FUNCTIONS(RcOverride, (ubjson))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(\
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
