#include "object_detection_metadata.h"

#include <tuple>

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_OBJECT_DETECTION_TYPES,
    (json)(ubjson),
    _Fields,
    (brief, true))

bool operator< (const QnObjectFeature& f, const QnObjectFeature& s)
{
    return std::tie(f.keyword, f.value) < std::tie(s.keyword, s.value);
}

QnPoint2D::QnPoint2D(const QPointF& point):
    x(point.x()),
    y(point.y())
{
}

QPointF QnPoint2D::toPointF() const
{
    return QPointF(x, y);
}

QnRect::QnRect(const QRectF& rect):
    topLeft(rect.topLeft()),
    bottomRight(rect.bottomRight())
{
}

QRectF QnRect::toRectF() const
{
    return QRectF(topLeft.toPointF(), bottomRight.toPointF());
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
        QByteArray(data->data(), (int) data->dataSize()),
        QnObjectDetectionMetadata(),
        &success);

    return success;
}

bool operator<(const QnObjectDetectionMetadataTrack& first,
    const QnObjectDetectionMetadataTrack& other)
{
    return first.timestampMs < other.timestampMs;
}

bool operator<(qint64 first, const QnObjectDetectionMetadataTrack& second)
{
    return first < second.timestampMs;
}

bool operator<(const QnObjectDetectionMetadataTrack& first, qint64 second)
{
    return first.timestampMs < second;
}
