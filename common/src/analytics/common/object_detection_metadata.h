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
    double x = 0;
    double y = 0;
};
QN_FUSION_DECLARE_FUNCTIONS(QnPoint2D, (json)(ubjson)(metatype));
#define QnPoint2D_Fields (x)(y)

struct QnRect
{
    QnRect() {}
    QnRect(QnPoint2D tl, QnPoint2D br): topLeft(tl), bottomRight(br) {}

    QnPoint2D topLeft;
    QnPoint2D bottomRight;
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

#define QN_OBJECT_DETECTION_TYPES (QnPoint2D)(QnRect)(QnObjectFeature)(QnObjectDetectionInfo)(QnObjectDetectionMetadata)

using QnObjectDetectionMetadataPtr = std::shared_ptr<QnObjectDetectionMetadata>;
