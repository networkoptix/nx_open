#include "serializers.h"

#include <cmath>

#include <nx/utils/compact_int.h>

#include "config.h"

namespace nx::analytics::storage {

static const QSize kResolution(kCoordinatesPrecision, kCoordinatesPrecision);

void TrackSerializer::serialize(
    const std::vector<ObjectPosition>& track,
    QByteArray* buf)
{
    serializeTrackSequence(track, buf);
}

void TrackSerializer::serializeTrackSequence(
    const std::vector<ObjectPosition>& track,
    QByteArray* buf)
{
    if (track.empty())
        return;

    buf->reserve(buf->size() + 9 + 2 + track.size() * 11);

    // Writing base timestamp. All timestamps will be saved as an offset from this base.
    nx::utils::compact_int::serialize(track.front().timestampUsec, buf);

    nx::utils::compact_int::serialize((long long)track.size(), buf);

    for (const auto& position: track)
        serialize(track.front().timestampUsec, position, buf);
}

void TrackSerializer::serialize(
    qint64 baseTimestamp,
    const ObjectPosition& position,
    QByteArray* buf)
{
    nx::utils::compact_int::serialize(position.timestampUsec - baseTimestamp, buf);
    nx::utils::compact_int::serialize(position.durationUsec, buf);
    serialize(translate(position.boundingBox, kResolution), buf);

    // TODO: attributes?
}

void TrackSerializer::serialize(const QRect& rect, QByteArray* buf)
{
    nx::utils::compact_int::serialize(rect.x(), buf);
    nx::utils::compact_int::serialize(rect.y(), buf);
    nx::utils::compact_int::serialize(rect.width(), buf);
    nx::utils::compact_int::serialize(rect.height(), buf);
}

void TrackSerializer::deserialize(
    QnByteArrayConstRef* buf,
    std::vector<ObjectPosition>* track)
{
    while (!buf->isEmpty())
        deserializeTrackSequence(buf, track);
}

void TrackSerializer::deserializeTrackSequence(
    QnByteArrayConstRef* buf,
    std::vector<ObjectPosition>* track)
{
    long long baseTimestamp = 0;
    nx::utils::compact_int::deserialize(buf, &baseTimestamp);

    long long trackSize = 0;
    nx::utils::compact_int::deserialize(buf, &trackSize);

    track->reserve(track->size() + trackSize);
    for (long long i = 0; i < trackSize; ++i)
    {
        track->push_back(ObjectPosition());
        deserialize(baseTimestamp, buf, &track->back());
    }
}

void TrackSerializer::deserialize(
    qint64 baseTimestamp,
    QnByteArrayConstRef* buf,
    ObjectPosition* position)
{
    nx::utils::compact_int::deserialize(buf, &position->timestampUsec);
    position->timestampUsec += baseTimestamp;

    nx::utils::compact_int::deserialize(buf, &position->durationUsec);

    QRect translatedRect;
    deserialize(buf, &translatedRect);
    position->boundingBox = translate(translatedRect, kResolution);

    // TODO: attributes?
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

//-------------------------------------------------------------------------------------------------

QRect translate(const QRectF& box, const QSize& resolution)
{
    return QRect(
        QPoint(
            box.x() * resolution.width(),
            box.y() * resolution.height()),
        QPoint(
            round(box.bottomRight().x() * resolution.width()),
            round(box.bottomRight().y() * resolution.height()))
    );
}

QRectF translate(const QRect& box, const QSize& resolution)
{
    return QRectF(
        box.x() / (double) resolution.width(), box.y() / (double) resolution.height(),
        box.width() / (double) resolution.width(), box.height() / (double) resolution.height());
}

} // namespace nx::analytics::storage
