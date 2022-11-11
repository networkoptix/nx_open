// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_serializer.h"

#include <nx/network/socket_common.h>
#include <nx/utils/random.h>
#include <nx/utils/gzip/gzip_compressor.h>

#include <stdint.h>

namespace nx::network::websocket {

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
    const nx::Buffer& payload, FrameType type, CompressionType compressionType)
{
    m_doCompress = shouldMessageBeCompressed(type, compressionType, payload.size());
    return prepareFrame(payload, type, /*fin*/true);
}

nx::Buffer Serializer::prepareFrame(nx::Buffer payload, FrameType type, bool fin)
{
    if (m_doCompress)
    {
        payload = nx::utils::bstream::gzip::Compressor::deflateData(std::move(payload));
        /* https://datatracker.ietf.org/doc/html/rfc7692#section-7.2.3.4
         * deflateData() generates DEFLATE block with BFINAL bit set.
         * Such payload should be flushed with empty uncompressed DEFLATE block with BFINAL set to 0.
         * */
        payload.append("\x0", 1);
    }

    nx::Buffer header;
    int payloadLenType = payloadLenTypeByLen(payload.size());
    header.resize(calcHeaderSize(m_masked, payloadLenType));
    memset(header.data(), 0, header.size());
    fillHeader(header.data(), fin, type, payloadLenType, payload.size());

    if (m_masked)
    {
        for (std::size_t i = 0; i < payload.size(); ++i)
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

    if (fin)
        *pdata |= 1 << 7;

    if (m_doCompress)
        *pdata |= 1 << 6; //< Setting the RSV1 bit

    *pdata |= opCode & 0xf;
    pdata++;

    if (m_masked)
        *pdata |= 1 << 7;

    *pdata |= payloadLenType & 0x7f;
    pdata++;

    if (payloadLenType == 126)
    {
        unsigned short nPayloadLen = htons(payloadLen);
        memcpy(pdata, &nPayloadLen, 2);
        static_assert(sizeof(unsigned short) == 2);
        pdata += 2;
    }
    else if (payloadLenType == 127)
    {
        int64_t nPayloadLen = htonll(payloadLen);
        memcpy(pdata, &nPayloadLen, 8);
        static_assert(sizeof(int64_t) == 8);
        pdata += 8;
    }

    if (m_masked)
    {
        memcpy(pdata, &m_mask, 4);
        static_assert(sizeof(m_mask) == 4);
        pdata += 4;
    }

    return pdata - data;
}

} // nx::network::websocket
