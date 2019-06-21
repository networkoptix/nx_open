#include <cstdint>
#include <nx/utils/random.h>
#include <nx/network/socket_common.h>
#include "websocket_serializer.h"

namespace nx {
namespace network {
namespace websocket {

namespace {

static int calcHeaderSize(bool masked, int payloadLenType)
{
    int result = 2;
    result += masked ? 4 : 0;
    result += payloadLenType <= 125 ? 0 : payloadLenType == 126 ? 2 : 8;

    return result;
}

static int payloadLenTypeByLen(int64_t len)
{
    if (len <= 125)
        return len;
    else if (len < 65536)
        return 126;
    else
        return 127;
}

} // namespace <anonymous>

Serializer::Serializer(bool masked, CompressionType compressionType, unsigned mask):
    m_compressionType(compressionType)
{
    setMasked(masked, mask);
}

void Serializer::prepareMessage(const nx::Buffer& payload, FrameType type, nx::Buffer* outBuffer)
{
    prepareFrame(payload, type, true, outBuffer);
}

int Serializer::prepareFrame(
    const char* payload, int payloadLen, FrameType type, bool fin, char* out, int outLen)
{
    int payloadLenType = payloadLenTypeByLen(payloadLen);
    int neededOutLen = payloadLen + calcHeaderSize(m_masked, payloadLenType);
    if (outLen < neededOutLen)
        return neededOutLen;

    if (!payload)
        return -1;

    int headerOffset = fillHeader(out, fin, type, payloadLenType, payloadLen);
    memcpy(out + headerOffset, payload, payloadLen);

    if (m_masked)
    {
        for (int i = 0; i < payloadLen; i++)
            out[i + headerOffset] = out[i + headerOffset] ^ ((unsigned char*) (&m_mask))[i % 4];
    }

    return neededOutLen;
}

void Serializer::prepareFrame(
    const nx::Buffer& payload, FrameType type, bool fin, nx::Buffer* outBuffer)
{
    outBuffer->resize(prepareFrame(nullptr, payload.size(), type, fin, nullptr, 0));
    outBuffer->fill(0);
    prepareFrame(
        payload.constData(), payload.size(), type, fin, outBuffer->data(), outBuffer->size());
}

void Serializer::setMasked(bool masked, unsigned mask)
{
    m_masked = masked;
    m_mask = mask;

    if (m_masked && m_mask == 0)
        m_mask = nx::utils::random::number<unsigned>(1, std::numeric_limits<unsigned>::max());
}

int Serializer::fillHeader(char* data, bool fin, int opCode, int payloadLenType, int payloadLen)
{
    char* pdata = data;

    *pdata = static_cast<uint8_t>(fin) << 7;
    firstByte |= static_cast<int>(m_compressionType == CompressionType::perMessageInflate) << 6;
    firstByte |= opCode & 0xf;
    pdata++;

    *pdata |= static_cast<int>(m_masked) << 7;
    *pdata |= payloadLenType & 0x7f;
    pdata++;

    if (payloadLenType == 126)
    {
        *reinterpret_cast<unsigned short*>(pdata) = htons(payloadLen);
        pdata += 2;
    }
    else if (payloadLenType == 127)
    {
        *reinterpret_cast<uint64_t*>(pdata) = htonll((int64_t)payloadLen);
        pdata += 8;
    }

    if (m_masked)
    {
        *reinterpret_cast<unsigned int*>(pdata) = m_mask;
        pdata += 4;
    }

    return pdata - data;
}

} // namespace websocket
} // namespace network
} // namespace nx
