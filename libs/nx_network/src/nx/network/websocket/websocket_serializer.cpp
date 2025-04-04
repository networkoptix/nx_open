// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_serializer.h"

#include <stdint.h>
#include <zlib.h>

#include <nx/network/socket_common.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/random.h>

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

struct Serializer::Private
{
    z_stream stream{};
    const int initError;

    Private():
        initError(deflateInit2(
            &stream,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -MAX_WBITS,
            MAX_MEM_LEVEL,
            Z_DEFAULT_STRATEGY))
    {
    }

    ~Private()
    {
        if (initError == Z_OK)
            deflateEnd(&stream);
    }

    nx::Buffer compress(nx::Buffer payload)
    {
        if (initError != Z_OK)
            return payload;

        static constexpr size_t kSyncTailLength = 4;
        nx::Buffer output;
        output.resize(deflateBound(&stream, (uLong) payload.size()) + kSyncTailLength);
        stream.next_in = (Bytef*) payload.data();
        stream.avail_in = (uInt) payload.size();
        stream.next_out = (Bytef*) output.data();
        stream.avail_out = (uInt) output.size();
        if (deflate(&stream, Z_SYNC_FLUSH) != Z_OK)
            return payload;

        if (stream.avail_in == 0)
        {
            output.resize(output.size() - (size_t) stream.avail_out - kSyncTailLength);
            return output;
        }
        return payload;
    }
};

Serializer::Serializer(bool masked, unsigned mask)
{
    setMasked(masked, mask);
}

Serializer::~Serializer() = default;

nx::Buffer Serializer::prepareMessage(
    const nx::Buffer& payload, FrameType type, std::optional<Compression> compression)
{
    return prepareFrame(payload, type, /*fin*/ true, std::move(compression));
}

nx::Buffer Serializer::prepareFrame(
    nx::Buffer payload, FrameType type, bool fin, std::optional<Compression> compression)
{
    if (isDataFrame(type))
        m_statistics.totalIn += payload.size();
    else
        compression.reset();

    if (compression)
    {
        switch (*compression)
        {
            case Compression::gzip:
            {
                constexpr int kCompressionMessageThreshold = 64;
                if (payload.size() <= kCompressionMessageThreshold)
                {
                    compression.reset();
                }
                else
                {
                    payload =
                        nx::utils::bstream::gzip::Compressor::deflateData(std::move(payload));
                    /* https://datatracker.ietf.org/doc/html/rfc7692#section-7.2.3.4
                     * deflateData() generates DEFLATE block with BFINAL bit set. Such payload
                     * should be flushed with empty uncompressed DEFLATE block with BFINAL set to 0.
                     */
                    payload.append('\x00');
                }
                break;
            }
            case Compression::deflate:
                if (payload.empty())
                {
                    compression.reset();
                }
                else
                {
                    if (!d)
                        d = std::make_unique<Private>();
                    payload = d->compress(std::move(payload));
                }
                break;
            default:
                NX_ASSERT(false, "Unsupported compression %1", (int) *compression);
                compression.reset();
                break;
        }
    }

    nx::Buffer header;
    int payloadLenType = payloadLenTypeByLen(payload.size());
    header.resize(calcHeaderSize(m_masked, payloadLenType));
    memset(header.data(), 0, header.size());
    fillHeader(header.data(), fin, type, payloadLenType, payload.size(), compression.has_value());

    if (m_masked)
    {
        for (std::size_t i = 0; i < payload.size(); ++i)
            payload[i] = payload[i] ^ ((unsigned char*) (&m_mask))[i % 4];
    }

    if (isDataFrame(type))
        m_statistics.totalOut += payload.size();
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
    char* data, bool fin, FrameType opCode, int payloadLenType, int payloadLen, bool compression)
{
    char* pdata = data;

    if (fin)
        *pdata |= 1 << 7;

    if (compression && (opCode == FrameType::binary || opCode == FrameType::text))
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
