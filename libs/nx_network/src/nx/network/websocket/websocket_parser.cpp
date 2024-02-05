// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_parser.h"

#include <algorithm>

#include <stdint.h>

#include <nx/network/socket_common.h>
#include <nx/utils/gzip/gzip_compressor.h>

namespace nx::network::websocket {

Parser::Parser(Role role, GotFrameHandler handler):
    m_role(role),
    m_frameHandler(std::move(handler)),
    m_uncompressor(nx::utils::bstream::makeCustomOutputStream(
        [this] (const nx::Buffer& data)
        {
            this->m_uncompressed += data;
        }),
        nx::utils::bstream::gzip::Uncompressor::Method::deflate)
{
}

Parser::BufferedState Parser::bufferDataIfNeeded(const char* data, int len, int neededLen)
{
    if (neededLen < len && m_buf.empty())
        return BufferedState::notNeeded;

    auto appendLen = std::min(neededLen - (int)m_buf.size(), len);
    m_buf.append(data, appendLen);
    m_pos += appendLen;

    if ((int) m_buf.size() < neededLen)
        return BufferedState::needMore;

    return BufferedState::enough;
}

Parser::ParseState Parser::processPayload(char* data, int len)
{
    int outLen = std::min(len, m_payloadLen);
    if (m_masked)
    {
        for (int i = 0; i < outLen; i++)
        {
            data[i] = data[i] ^ ((unsigned char*)(&m_mask))[m_maskPos % 4];
            m_maskPos++;
        }
    }
    m_frameBuffer.append(data, outLen);
    m_payloadLen -= outLen;
    m_pos += outLen;
    if (m_payloadLen == 0)
    {
        handleFrame();
        return ParseState::readingHeaderFixedPart;
    }
    return ParseState::readingPayload;
}

void Parser::processPart(
    char* data, int len, int neededLen, ParseState (Parser::*processFunc)(char* data))
{
    switch (bufferDataIfNeeded(data, len, neededLen))
    {
    case BufferedState::needMore:
        break;
    case BufferedState::notNeeded:
        m_state = (this->*processFunc)(data);
        m_pos += neededLen;
        break;
    case BufferedState::enough:
        m_state = (this->*processFunc)(m_buf.data());
        m_buf.clear();
        break;
    }
}

void Parser::parse(char* data, int len)
{
    switch (m_state)
    {
    case ParseState::readingHeaderFixedPart:
        processPart(
            data,
            len,
            kFixedHeaderLen,
            &Parser::readHeaderFixed);
        break;
    case ParseState::readingHeaderExtension:
        processPart(
            data,
            len,
            m_headerExtLen,
            &Parser::readHeaderExtension);
        break;
    case ParseState::readingPayload:
        m_state = processPayload(data, len);
        break;
    }
}

void Parser::consume(char* data, int len)
{
    m_pos = 0;
    while (m_pos < len)
        parse(data + m_pos, len - m_pos);
}

void Parser::consume(nx::Buffer& buf)
{
    consume(buf.data(), buf.size());
}

void Parser::setRole(Role role)
{
    m_role = role;
}

Parser::ParseState Parser::readHeaderFixed(char* data)
{
    m_opCode = (FrameType)(*data & 0x0F);
    m_fin = (*data >> 7) & 0x01;

    if (static_cast<bool>((*data >> 6) & 0x01)) //< RSV1 bit set
        m_doUncompress = true;

    data++;
    m_masked = (*data >> 7) & 0x01;

    m_payloadLen = (unsigned char)(*data & (~0x80));
    m_headerExtLen = (m_payloadLen <= 125 ? 0 : m_payloadLen == 126 ? 2 : 8) + (m_masked ? 4 : 0);
    if (m_headerExtLen == 0 && m_payloadLen == 0)
    {
        handleFrame();
        return ParseState::readingHeaderFixedPart;
    }
    return m_headerExtLen == 0 ? ParseState::readingPayload: ParseState::readingHeaderExtension;
}

void Parser::handleFrame()
{
    if (m_doUncompress)
    {
        if (nx::utils::bstream::gzip::Compressor::isZlibCompressed(m_frameBuffer))
        {
            // Fallback for vms 5.0 and older.
            // TODO: Remove this code when the backward compatibility is no longer needed.
            m_frameBuffer = nx::utils::bstream::gzip::Compressor::uncompressData(m_frameBuffer);
        }
        else
        {
            m_uncompressor.processData(m_frameBuffer);
            m_frameBuffer = m_uncompressed;

            if (m_fin)
            {
                // https://datatracker.ietf.org/doc/html/rfc7692#section-7.2.2
                // Block with "\x00\x00\xff\xff" should be inflated in separate call of 'inflate()',
                // this logic is copied from Chromium sources.
                static const nx::Buffer buf("\x00\x00\xff\xff", 4);
                m_uncompressor.processData(buf);
            }

            m_uncompressed.clear();
        }
    }

    m_frameHandler(m_firstFrame ? m_opCode : FrameType::continuation, std::move(m_frameBuffer), m_fin);
    m_frameBuffer.clear();

    if (m_firstFrame)
        m_firstFrame = false;

    if (m_fin)
    {
        m_firstFrame = true;
        m_doUncompress = false;
    }
}

Parser::ParseState Parser::readHeaderExtension(char* data)
{
    if (m_payloadLen == 126)
    {
        m_payloadLen = ntohs(*reinterpret_cast<const unsigned short*>(data));
        data += 2;
    }
    else if (m_payloadLen == 127)
    {
        m_payloadLen = ntohll(*reinterpret_cast<const uint64_t*>(data));
        data += 8;
    }

    if (m_masked)
    {
        m_mask = *((unsigned int*)(data));
        m_maskPos = 0;
    }

    if (m_payloadLen == 0)
    {
        handleFrame();
        return ParseState::readingHeaderFixedPart;
    }

    return ParseState::readingPayload;
}

void Parser::reset()
{
    m_buf.clear();
    m_state = ParseState::readingHeaderFixedPart;
    m_payloadLen = 0;
    m_pos = 0;
}

FrameType Parser::frameType() const
{
    return m_opCode;
}

int Parser::frameSize() const
{
    return m_payloadLen;
}

} // namespace nx::network::websocket
