#include "serializers.h"

#include <cmath>

#include <nx/utils/compact_int.h>

#include <analytics/db/config.h>
#include <motion/motion_detection.h>

namespace nx::analytics::db {

static const QSize kResolution(kCoordinatesPrecision, kCoordinatesPrecision);

void TrackSerializer::serialize(const QRect& rect, QByteArray* buf)
{
    nx::utils::compact_int::serialize(rect.x(), buf);
    nx::utils::compact_int::serialize(rect.y(), buf);
    nx::utils::compact_int::serialize(rect.width(), buf);
    nx::utils::compact_int::serialize(rect.height(), buf);
}


void TrackSerializer::deserialize(QnByteArrayConstRef* buf, QRect* rect)
{
    long long value = 0;

    nx::utils::compact_int::deserialize(buf, &value);
    rect->setX(value);

    nx::utils::compact_int::deserialize(buf, &value);
    rect->setY(value);

    nx::utils::compact_int::deserialize(buf, &value);
    rect->setWidth(value);

    nx::utils::compact_int::deserialize(buf, &value);
    rect->setHeight(value);
}

void TrackSerializer::serialize(const QRectF& rect, QByteArray* buf)
{
    serialize(
        translate(rect, kResolution),
        buf);
}

void TrackSerializer::deserialize(QnByteArrayConstRef* buf, QRectF* rect)
{
    QRect translated;
    deserialize(buf, &translated);
    *rect = translate(translated, kResolution);
}

void TrackSerializer::serialize(const ObjectRegion& data, QByteArray* buf)
{
    *buf = data.boundingBoxGrid;
}

void TrackSerializer::deserialize(QnByteArrayConstRef* buf, ObjectRegion* region)
{
    region->boundingBoxGrid.clear();
    if (buf->size() == Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8)
        region->boundingBoxGrid = *buf;
}

//-------------------------------------------------------------------------------------------------

QRect translate(const QRectF& box, const QSize& resolution)
{
    return QRect(
        QPoint(
            box.x() * resolution.width(),
            box.y() * resolution.height()),
        QPoint(
            lround(box.bottomRight().x() * resolution.width()),
            lround(box.bottomRight().y() * resolution.height()))
    );
}

QRect restore(const QRectF& box, const QSize& resolution)
{
    return QRect(
        QPoint(
            lround(box.x() * resolution.width()),
            lround(box.y() * resolution.height())),
        QPoint(
            lround(box.bottomRight().x() * resolution.width()),
            lround(box.bottomRight().y() * resolution.height()))
    );
}

QRectF translate(const QRect& box, const QSize& resolution)
{
    return QRectF(
        QPointF(
            box.x() / (double) resolution.width(),
            box.y() / (double) resolution.height()),
        QPointF(
            box.bottomRight().x() / (double) resolution.width(),
            box.bottomRight().y() / (double) resolution.height()));
}

} // namespace nx::analytics::db
