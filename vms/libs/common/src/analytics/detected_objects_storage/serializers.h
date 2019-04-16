#pragma once

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
 * @return bytesRead.
 */
int deserialize(const QByteArray& buf, long long* value);

/**
 * Deserializes the whole buf as a sequence of compressed integers.
 * So, end of the buffer == end of the sequence.
 * @return bytesRead.
 */
int deserialize(const QByteArray& buf, std::vector<long long>* numbers);

template<typename T>
T deserialized(const QByteArray& buf)
{
    T value;
    deserialize(buf, &value);
    return value;
}

} // namespace compact_int

//-------------------------------------------------------------------------------------------------

class TrackSerializer
{
public:
    /**
     * Two serialization results can be appended to each other safely.
     */
    static QByteArray serialize(const std::vector<ObjectPosition>& /*track*/);

    static std::vector<ObjectPosition> deserialize(const QByteArray& /*serializedData*/);
};

} // namespace nx::analytics::storage
