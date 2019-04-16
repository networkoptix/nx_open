#include "serializers.h"

namespace nx::analytics::storage {

namespace compact_int {

int serialize(long long value, QByteArray* buf)
{
    // NOTE: Cannot shift negative numbers.
    auto uValue = (unsigned long long) value;

    static constexpr int kBufSize = 10;
    std::uint8_t valueBuf[kBufSize];
    int bytesWritten = 0;
    while (uValue != 0)
    {
        auto bitsToWrite = (std::uint8_t)(uValue & 0x7f);
        uValue >>= 7;

        // NOTE: Filling valueBuf in reverse order so that the most significant bits are in the beginning.
        if (bytesWritten != 0)
            bitsToWrite |= 0x80; //< One more byte is needed.
        valueBuf[kBufSize - (++bytesWritten)] = bitsToWrite;
    }

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

int deserialize(const QByteArray& buf, long long* value)
{
    unsigned long long uValue = 0;

    const char* bufPos = buf.data();
    while (bufPos - buf.data() < buf.size())
    {
        const auto byte = (std::uint8_t) *bufPos;
        const bool endOfNumber = (*bufPos & 0x80) == 0;

        uValue <<= 7;
        uValue |= byte & 0x7f;

        ++bufPos;
        if (endOfNumber)
            break;
    }

    *value = (long long) uValue;

    return bufPos - buf.data();
}

int deserialize(const QByteArray& buf, std::vector<long long>* numbers)
{
    int bytesRead = 0;
    while (bytesRead < buf.size())
    {
        long long value = 0;
        bytesRead += deserialize(
            QByteArray::fromRawData(buf.data() + bytesRead, buf.size() - bytesRead),
            &value);
        numbers->push_back(value);
    }

    return bytesRead;
}

} // namespace compact_int

//-------------------------------------------------------------------------------------------------

QByteArray TrackSerializer::serialize(const std::vector<ObjectPosition>& /*track*/)
{
    NX_ASSERT(false);
    // TODO
    return QByteArray();
}

std::vector<ObjectPosition> TrackSerializer::deserialize(
    const QByteArray& /*serializedData*/)
{
    // TODO
    return {};
}

} // namespace nx::analytics::storage
