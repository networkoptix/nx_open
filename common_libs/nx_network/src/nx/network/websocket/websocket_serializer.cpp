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

static int fillHeader(char* data, bool fin, int opCode, int payloadLenType, int payloadLen, bool masked, unsigned int mask)
{
    char * pdata = data;

    *pdata |= (int)fin << 7;
    *pdata |= opCode & 0xf;
    pdata++;

    *pdata |= (int)masked << 7;
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

    if (masked)
    {
        *reinterpret_cast<unsigned int*>(pdata) = mask;
        pdata += 4;
    }

    return pdata - data;
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

static int prepareFrame(const char* payload, int payloadLen, FrameType type,
    bool fin, bool masked, unsigned int mask, char* out, int outLen)
{
    int payloadLenType = payloadLenTypeByLen(payloadLen);
    int neededOutLen = payloadLen + calcHeaderSize(masked, payloadLenType);
    if (outLen < neededOutLen)
        return neededOutLen;

    int headerOffset = fillHeader(out, fin, type, payloadLenType, payloadLen, masked, mask);
    memcpy(out + headerOffset, payload, payloadLen);

    if (masked)
    {
        for (int i = 0; i < payloadLen; i++)
            out[i + headerOffset] = out[i + headerOffset] ^ ((unsigned char*)(&mask))[i % 4];
    }

    return neededOutLen;
}

} // namespace <anonymous>

Serializer::Serializer(bool masked, unsigned mask)
{
    setMasked(masked, mask);
}

void Serializer::prepareMessage(const nx::Buffer& payload, FrameType type, nx::Buffer* outBuffer)
{
    prepareFrame(payload, type, true, outBuffer);
}

int Serializer::prepareFrame(
    const char* payload, int payloadLen,
    FrameType type, bool fin, char* out, int outLen)
{
    return websocket::prepareFrame(payload, payloadLen, type, fin, m_masked, m_mask, out, outLen);
}

void Serializer::prepareFrame(
    const nx::Buffer& payload,
    FrameType type,
    bool fin,
    nx::Buffer* outBuffer)
{
    outBuffer->resize(
        websocket::prepareFrame(
            nullptr, payload.size(), type, fin, m_masked, m_mask, nullptr, 0));
    outBuffer->fill(0);
    prepareFrame(
        payload.constData(),
        payload.size(), type, fin,
        outBuffer->data(), outBuffer->size());
}

void Serializer::setMasked(bool masked, unsigned mask)
{
    m_masked = masked;
    m_mask = mask;

    if (m_masked && m_mask == 0)
        m_mask = nx::utils::random::number<unsigned>(1, std::numeric_limits<unsigned>::max());
}

} // namespace websocket
} // namespace network
} // namespace nx
