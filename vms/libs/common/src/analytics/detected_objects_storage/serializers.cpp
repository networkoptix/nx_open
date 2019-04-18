#include "serializers.h"

#include "config.h"

namespace nx::analytics::storage {

namespace compact_int {

int serialize(long long value, QByteArray* buf)
{
    // NOTE: Cannot shift negative numbers.
    auto uValue = (unsigned long long) value;

    static constexpr int kBufSize = 10;
    std::uint8_t valueBuf[kBufSize];
    int bytesWritten = 0;
    do
    {
        auto bitsToWrite = (std::uint8_t)(uValue & 0x7f);
        uValue >>= 7;

        // NOTE: Filling valueBuf in reverse order so that the most significant bits are in the beginning.
        if (bytesWritten != 0)
            bitsToWrite |= 0x80; //< One more byte is needed.
        valueBuf[kBufSize - (++bytesWritten)] = bitsToWrite;
    } while (uValue != 0);

    buf->append((char*) valueBuf + (kBufSize - bytesWritten), bytesWritten);
    return bytesWritten;
}

int serialize(
    const std::vector<long long>& numbers,
    QByteArray* buf)
{
    return (int) std::accumulate(numbers.begin(), numbers.end(), 0,
        [buf](long long bytesWritten, long long value)
        {
            return bytesWritten + serialize(value, buf);
        });
}

//-------------------------------------------------------------------------------------------------

void deserialize(QnByteArrayConstRef* buf, long long* value)
{
    unsigned long long uValue = 0;

    while (!buf->isEmpty())
    {
        const auto byte = (std::uint8_t) *buf->data();
        const bool endOfNumber = (byte & 0x80) == 0;

        uValue <<= 7;
        uValue |= byte & 0x7f;

        buf->pop_front();
        if (endOfNumber)
            break;
    }

    *value = (long long) uValue;
}

void deserialize(QnByteArrayConstRef* buf, std::vector<long long>* numbers)
{
    while (!buf->isEmpty())
    {
        long long value = 0;
        deserialize(buf, &value);
        numbers->push_back(value);
    }
}

} // namespace compact_int

//-------------------------------------------------------------------------------------------------

QByteArray TrackSerializer::serialized(const std::vector<ObjectPosition>& track)
{
    QByteArray buf;
    serializeTrackSequence(track, &buf);
    return buf;
}

std::vector<ObjectPosition> TrackSerializer::deserialized(
    const QByteArray& serializedData)
{
    QnByteArrayConstRef buf(serializedData);

    std::vector<ObjectPosition> track;

    while (!buf.isEmpty())
        deserializeTrackSequence(&buf, &track);

    return track;
}

void TrackSerializer::serializeTrackSequence(
    const std::vector<ObjectPosition>& track,
    QByteArray* buf)
{
    if (track.empty())
        return;

    buf->reserve(buf->size() + 9 + 2 + track.size() * 11);

    // Writing base timestamp. All timestamps will be saved as an offset from this base.
    compact_int::serialize(track.front().timestampUsec, buf);

    compact_int::serialize((long long)track.size(), buf);

    for (const auto& position: track)
        serialize(track.front().timestampUsec, position, buf);
}

void TrackSerializer::serialize(
    qint64 baseTimestamp,
    const ObjectPosition& position,
    QByteArray* buf)
{
    compact_int::serialize(position.timestampUsec - baseTimestamp, buf);
    compact_int::serialize(position.durationUsec, buf);
    serialize(translate(position.boundingBox), buf);

    // TODO: attributes?
}

void TrackSerializer::serialize(const QRect& rect, QByteArray* buf)
{
    compact_int::serialize(rect.x(), buf);
    compact_int::serialize(rect.y(), buf);
    compact_int::serialize(rect.width(), buf);
    compact_int::serialize(rect.height(), buf);
}

void TrackSerializer::deserializeTrackSequence(
    QnByteArrayConstRef* buf,
    std::vector<ObjectPosition>* track)
{
    long long baseTimestamp = 0;
    compact_int::deserialize(buf, &baseTimestamp);

    long long trackSize = 0;
    compact_int::deserialize(buf, &trackSize);

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
    compact_int::deserialize(buf, &position->timestampUsec);
    position->timestampUsec += baseTimestamp;

    compact_int::deserialize(buf, &position->durationUsec);

    QRect translatedRect;
    deserialize(buf, &translatedRect);
    position->boundingBox = translate(translatedRect);

    // TODO: attributes?
}

void TrackSerializer::deserialize(QnByteArrayConstRef* buf, QRect* rect)
{
    long long value = 0;

    compact_int::deserialize(buf, &value);
    rect->setX(value);

    compact_int::deserialize(buf, &value);
    rect->setY(value);

    compact_int::deserialize(buf, &value);
    rect->setWidth(value);

    compact_int::deserialize(buf, &value);
    rect->setHeight(value);
}

QRect TrackSerializer::translate(const QRectF& box)
{
    return QRect(
        box.x() * kCoordinatesPrecision, box.y() * kCoordinatesPrecision,
        box.width() * kCoordinatesPrecision, box.height() * kCoordinatesPrecision);
}

QRectF TrackSerializer::translate(const QRect& box)
{
    return QRectF(
        box.x() / (double)kCoordinatesPrecision, box.y() / (double)kCoordinatesPrecision,
        box.width() / (double)kCoordinatesPrecision, box.height() / (double)kCoordinatesPrecision);
}

} // namespace nx::analytics::storage
