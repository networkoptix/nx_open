#pragma once

#include <set>

#include <analytics/common/abstract_metadata.h>

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

struct QnObjectFeature
{
    QString keyword;
    QString value;
};
QN_FUSION_DECLARE_FUNCTIONS(QnObjectFeature, (json)(ubjson)(metatype));
#define QnObjectFeature_Fields (keyword)(value)

bool operator< (const QnObjectFeature& f, const QnObjectFeature& s);

struct QnPoint2D
{
    QnPoint2D() {};
    QnPoint2D(double xC, double yC): x(xC), y(yC) {};
    QnPoint2D(const QPointF& point);

    double x = 0;
    double y = 0;

    QPointF toPointF() const;
};
QN_FUSION_DECLARE_FUNCTIONS(QnPoint2D, (json)(ubjson)(metatype));
#define QnPoint2D_Fields (x)(y)

struct QnRect
{
    QnRect() {}
    QnRect(QnPoint2D tl, QnPoint2D br): topLeft(tl), bottomRight(br) {}
    QnRect(const QRectF& rect);

    QnPoint2D topLeft;
    QnPoint2D bottomRight;

    QRectF toRectF() const;
};
QN_FUSION_DECLARE_FUNCTIONS(QnRect, (json)(ubjson)(metatype));
#define QnRect_Fields (topLeft)(bottomRight)

struct QnObjectDetectionInfo
{
    QnUuid objectId;
    QnRect boundingBox;
    std::set<QnObjectFeature> labels;
};
QN_FUSION_DECLARE_FUNCTIONS(QnObjectDetectionInfo, (json)(ubjson)(metatype));
#define QnObjectDetectionInfo_Fields (boundingBox)(objectId)(labels)

struct QnObjectDetectionMetadata: public QnAbstractMetadata
{
    std::vector<QnObjectDetectionInfo> detectedObjects;

    virtual QnCompressedMetadataPtr serialize() const override;
    virtual bool deserialize(const QnConstCompressedMetadataPtr& data) override;
};
QN_FUSION_DECLARE_FUNCTIONS(QnObjectDetectionMetadata, (json)(ubjson)(metatype));
#define QnObjectDetectionMetadata_Fields (detectedObjects)

struct QnObjectDetectionMetadataTrack
{
    qint64 timestampMs = 0;
    std::vector<QnObjectDetectionInfo> objects;
};
QN_FUSION_DECLARE_FUNCTIONS(QnObjectDetectionMetadataTrack, (json)(ubjson)(metatype));
#define QnObjectDetectionMetadataTrack_Fields (timestampMs)(objects)

#define QN_OBJECT_DETECTION_TYPES (QnPoint2D)(QnRect)\
    (QnObjectFeature)\
    (QnObjectDetectionInfo)\
    (QnObjectDetectionMetadata)\
    (QnObjectDetectionMetadataTrack)

using QnObjectDetectionMetadataPtr = std::shared_ptr<QnObjectDetectionMetadata>;

bool operator<(const QnObjectDetectionMetadataTrack& first,
    const QnObjectDetectionMetadataTrack& second);
bool operator<(qint64 first, const QnObjectDetectionMetadataTrack& second);
bool operator<(const QnObjectDetectionMetadataTrack& first, qint64 second);
