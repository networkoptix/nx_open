#pragma once

#include <vector>

#include <QtCore/QSize>

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

class TrackSerializer
{
public:
    /**
     * NOTE: Two serialization results can be appended to each other safely.
     */
    static QByteArray serialized(const std::vector<ObjectPosition>& track);

    /**
     * Concatenated results of TrackSerializer::serialized are allowed.
     * So, all data from the buf is deserialized.
     */
    static std::vector<ObjectPosition> deserialized(const QByteArray& buf);

private:
    static void serializeTrackSequence(
        const std::vector<ObjectPosition>& track,
        QByteArray* buf);

    static void serialize(
        qint64 baseTimestamp,
        const ObjectPosition& position,
        QByteArray* buf);

    static void serialize(const QRect& rect, QByteArray* buf);

    static void deserializeTrackSequence(
        QnByteArrayConstRef* buf,
        std::vector<ObjectPosition>* track);

    static void deserialize(
        qint64 baseTimestamp,
        QnByteArrayConstRef* buf,
        ObjectPosition* position);

    static void deserialize(QnByteArrayConstRef* buf, QRect* rect);
};

//-------------------------------------------------------------------------------------------------

/**
 * Translates rect coordinates from [0; 1] to [0; resolution.width()] and [0; resolution.height()].
 */
QRect translate(const QRectF& box, const QSize& resolution);

/**
 * Translates rect coordinates from [0; resolution.width()] and [0; resolution.height()] to [0; 1].
 */
QRectF translate(const QRect& box, const QSize& resolution);

} // namespace nx::analytics::storage
