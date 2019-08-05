#include "websocket_serializer.h"
#include <nx/utils/random.h>
#include <nx/network/socket_common.h>
#include <nx/utils/gzip/gzip_compressor.h>

#include <cstdint>

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

Serializer::Serializer(bool masked, unsigned mask)
{
    setMasked(masked, mask);
}

nx::Buffer Serializer::prepareMessage(
    nx::Buffer payload, FrameType type, CompressionType compressionType)
{
    m_doCompress = shouldMessageBeCompressed(type, compressionType, payload.size());
    return prepareFrame(payload, type, /*fin*/true);
}

nx::Buffer Serializer::prepareFrame(nx::Buffer payload, FrameType type, bool fin)
{
    if (m_doCompress)
        payload = nx::utils::bstream::gzip::Compressor::compressData(std::move(payload), false);

    nx::Buffer header;
    int payloadLenType = payloadLenTypeByLen(payload.size());
    header.resize(calcHeaderSize(m_masked, payloadLenType));
    header.fill(0);
    fillHeader(header.data(), fin, type, payloadLenType, payload.size());

    if (m_masked)
    {
        for (int i = 0; i < payload.size(); i++)
            payload[i] = payload[i] ^ ((unsigned char*) (&m_mask))[i % 4];
    }

    return header + payload;
}

void Serializer::setMasked(bool masked, unsigned mask)
{
    m_masked = masked;
    m_mask = mask;

    if (m_masked && m_mask == 0)
        m_mask = nx::utils::random::number<unsigned>(1, std::numeric_limits<unsigned>::max());
}

int Serializer::fillHeader(
    char* data, bool fin, FrameType opCode, int payloadLenType, int payloadLen)
{
    char* pdata = data;

    *pdata = static_cast<uint8_t>(fin) << 7;
    if (m_doCompress)
        *pdata |= 1 << 6; //< Setting the RSV1 bit

    *pdata |= opCode & 0xf;
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
