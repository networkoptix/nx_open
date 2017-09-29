#include "object_detection_metadata.h"

#include <tuple>

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_OBJECT_DETECTION_TYPES, (json)(ubjson), _Fields)

bool operator< (const QnObjectFeature& f, const QnObjectFeature& s)
{
    return std::tie(f.keyword, f.value) < std::tie(s.keyword, s.value);
}

QnCompressedMetadataPtr QnObjectDetectionMetadata::serialize() const
{
    QnCompressedMetadataPtr compressed = std::make_shared<QnCompressedMetadata>(MetadataType::ObjectDetection);

    auto serialized = QnUbjson::serialized(*this);
    compressed->setData(serialized.data(), serialized.size());

    return compressed;
}

bool QnObjectDetectionMetadata::deserialize(const QnConstCompressedMetadataPtr& data)
{
    bool success = false;

    *this = QnUbjson::deserialized<QnObjectDetectionMetadata>(
        QByteArray(data->data(), data->dataSize()),
        QnObjectDetectionMetadata(),
        &success);

    return success;
}
