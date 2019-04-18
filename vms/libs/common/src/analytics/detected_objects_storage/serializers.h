#pragma once

#include <nx/utils/qnbytearrayref.h>

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

/**
 * Contains functions for serializing integers into the following format:
 * {code}
 * long long value = 0;
 * do {
 *     end_of_number = readBits(1);
 *     value << 7;
 *     value |= readBits(7);
 * } while (!end_of_number)
 * {code}
 *
 * NOTE: No conversion to the network byte order is done so that small numbers consume small number of bytes.
 * But, the serialization format is still architecture-independent.
 */
namespace compact_int {

/**
 * @return Bytes written.
 */
int serialize(long long value, QByteArray* buf);

/**
 * Serializes the array of numbers into buf. No array size is written!
 * @return Bytes written.
 */
int serialize(
    const std::vector<long long>& numbers,
    QByteArray* buf);

template<typename T>
QByteArray serialized(const T& value)
{
    QByteArray buf;
    buf.reserve(256);
    serialize(value, &buf);
    return buf;
}

//-------------------------------------------------------------------------------------------------

/**
 * NOTE: Deserialized bytes are removed from the buf.
 */
void deserialize(QnByteArrayConstRef* buf, long long* value);

/**
 * Deserializes the whole buf as a sequence of compressed integers.
 * So, end of the buffer == end of the sequence.
 * NOTE: Deserialized bytes are removed from the buf.
 */
void deserialize(QnByteArrayConstRef* buf, std::vector<long long>* numbers);

template<typename T>
T deserialized(const QByteArray& buf)
{
    T value;
    QnByteArrayConstRef localBuf(buf);
    deserialize(&localBuf, &value);
    return value;
}

} // namespace compact_int

//-------------------------------------------------------------------------------------------------

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

    static QRect translate(const QRectF& box);
    static QRectF translate(const QRect& box);
};

} // namespace nx::analytics::storage
