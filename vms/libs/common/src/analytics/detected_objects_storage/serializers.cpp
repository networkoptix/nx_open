#include "serializers.h"

#include "analytics_events_storage_types.h"

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
