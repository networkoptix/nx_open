#pragma once

#include <vector>

#include <QtCore/QByteArray>

#include "qnbytearrayref.h"

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
namespace nx::utils::compact_int {

/**
 * @return Bytes written.
 */
NX_UTILS_API int serialize(long long value, QByteArray* buf);

/**
 * Serializes the array of numbers into buf. No array size is written!
 * @return Bytes written.
 */
NX_UTILS_API int serialize(
    const std::vector<long long>& numbers,
    QByteArray* buf);

template<typename T>
QByteArray serialized(const T& value)
{
    QByteArray buf;
    serialize(value, &buf);
    return buf;
}

//-------------------------------------------------------------------------------------------------

/**
 * NOTE: Deserialized bytes are removed from the buf.
 */
NX_UTILS_API void deserialize(QnByteArrayConstRef* buf, long long* value);

/**
 * Deserializes the whole buf as a sequence of compressed integers.
 * So, end of the buffer == end of the sequence.
 * NOTE: Deserialized bytes are removed from the buf.
 */
NX_UTILS_API void deserialize(QnByteArrayConstRef* buf, std::vector<long long>* numbers);

template<typename T>
T deserialized(const QByteArray& buf)
{
    T value;
    QnByteArrayConstRef localBuf(buf);
    deserialize(&localBuf, &value);
    return value;
}

} // namespace nx::utils::compact_int
