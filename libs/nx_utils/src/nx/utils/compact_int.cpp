// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compact_int.h"

#include <algorithm>
#include <numeric>

namespace nx::utils::compact_int {

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
        ++bytesWritten;
        valueBuf[kBufSize - bytesWritten] = bitsToWrite;
    } while (uValue != 0);

    buf->append((char*) valueBuf + (kBufSize - bytesWritten), bytesWritten);
    return bytesWritten;
}

int serialize(
    const std::vector<long long>& numbers,
    QByteArray* buf)
{
    constexpr int kAverageSerializedNumberSize = 5;

    buf->reserve(buf->size() + (int) numbers.size() * kAverageSerializedNumberSize);

    return (int) std::accumulate(numbers.begin(), numbers.end(), 0,
        [buf](long long bytesWritten, long long value)
        {
            return bytesWritten + serialize(value, buf);
        });
}

//-------------------------------------------------------------------------------------------------

bool deserialize(const char** begin, const char* end, long long* value)
{
    unsigned long long uValue = 0;

    while (*begin < end)
    {
        const auto byte = (std::uint8_t) **begin;
        const bool endOfNumber = (byte & 0x80) == 0;

        uValue <<= 7;
        uValue |= byte & 0x7f;

        ++(*begin);
        if (endOfNumber)
        {
            *value = (long long)uValue;
            return true;
        }
    }

    return false;
}

bool deserialize(QnByteArrayConstRef* buf, long long* value)
{
    const char* begin = buf->data();
    if (!deserialize(&begin, buf->data() + buf->size(), value))
        return false;

    buf->pop_front(begin - buf->data());
    return true;
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

} // namespace nx::utils::compact_int
