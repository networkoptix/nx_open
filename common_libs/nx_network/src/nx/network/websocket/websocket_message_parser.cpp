#include <algorithm>
#include <nx/network/websocket/websocket_message_parser.h>

namespace nx {
namespace network {
namespace websocket {

MessageParser::MessageParser(bool isServer, MessageParserHandler* handler):
    m_isServer(isServer),
    m_handler(handler)
{
}

MessageParser::BufferedState MessageParser::bufferDataIfNeeded(const char* data, int64_t len, int64_t neededLen)
{
    if (neededLen < len - m_pos)
    {
        m_pos += neededLen;
        return BufferedState::notNeeded;
    }

    auto appendLen = std::min(neededLen - (int64_t)m_buf.size(), len - m_pos);
    m_buf.append(data + m_pos, appendLen);
    m_pos += appendLen;

    if (m_buf.size() < neededLen)
        return BufferedState::needMore;

    return BufferedState::enough;
}

void MessageParser::processPart(
    const char* data, 
    int64_t len, 
    int64_t neededLen,
    ParseState nextState,
    void (MessageParser::*processFunc)(const char* data, int64_t len))
{
    switch (bufferDataIfNeeded(data, len, neededLen))
    {
    case BufferedState::needMore:
        break;
    case BufferedState::notNeeded:
        (this->*processFunc)(data, len);
        m_state = nextState;
        break;
    case BufferedState::enough:
        (this->*processFunc)(m_buf.constData(), m_buf.size());
        m_state = nextState;
        m_buf.clear();
        break;
    }
}

MessageParser::ParseState MessageParser::stateByPacketType() const
{
}

void MessageParser::parse(const char* data, int64_t len)
{
    switch (m_state)
    {
    case ParseState::readingHeaderFixedPart:
        processPart(
            data, 
            len, 
            kFixedHeaderLen, 
            ParseState::readingHeaderExtension,
            &MessageParser::readHeaderFixed);
        break;
    case ParseState::readingHeaderExtension:
        processPart(
            data,
            len,
            m_headerExtLen,
            stateByPacketType(),
            &MessageParser::readHeaderExtension);
        break;
    }
}

void MessageParser::consume(const char* data, int64_t len)
{
    m_pos = 0;
    while (m_pos < len)
        parse(data, len);

    switch (m_state)
    {

    case ParseState::readingHeaderFixedPart:
    {
        int appendLen = std::min(kFixedHeaderLen - (int64_t)m_buf.size(), len);
        m_buf.append(data, appendLen);
        if (m_buf.size() < kFixedHeaderLen)
            return;
        readHeaderFixed(m_buf.data(), m_buf.size());
        return;
    }
    case ParseState::readingHeaderExtension:
    {
        readHeaderExtension(data, len);
        return;
    }
    case ParseState::readingPingPayload:
    case ParseState::readingPongPayload:
    {
        int appendLen = std::min(m_payloadLen - m_buf.size(), len);
        m_buf.append(data, appendLen);
        m_payloadLen -= appendLen;
        if (m_payloadLen != 0)
            return;
        if (m_state == ParseState::readingPingPayload)
            m_handler->pingReceived(m_buf.constData(), m_buf.size());
        else
            m_handler->pongReceived(m_buf.constData(), m_buf.size());
        reset();
        readHeaderFixed(data + appendLen, len - appendLen);
        return;
    }
    case ParseState::readingApplicationPayload:
    {
        int toRead = std::min(m_payloadLen - m_pos, len);
        m_payloadLen -= toRead;
        m_handler->payloadReceived(data, toRead, m_payloadLen == 0);
        if (m_payloadLen != 0)
            return;
        reset();
        readHeaderFixed(data + toRead, len - toRead);
        return;
    }
    }
}

bool MessageParser::readHeaderFixed(const char* data, int64_t len)
{
    m_opCode = data[0] & 0x0F;
    m_fin = (data[0] >> 7) & 0x01;
    m_masked = (data[1] >> 7) & 0x01;

    m_lengthTypeField = data[1] & (~0x80);
    unsigned int m_mask = 0;
    int m_pos = 0;

    m_headerLen = kFixedHeaderLen + (m_lengthTypeField <= 125 ? 0 : m_lengthTypeField == 126 ? 2 : 8) + (m_masked ? 4 : 0);
    m_state = ParseState::readingHeaderExtension;

    if (len < m_headerLen)
        return;

    readHeaderExtension(data, len);

    if (m_lengthTypeField <= 125) 
    {
        m_payloadLen = m_lengthTypeField;
    }
    else if (m_lengthTypeField == 126) 
    { 
        m_payloadLen = data[2] + (data[3] << 8);
        pos += 2;
    }
    else if (lengthTypeField == 127) 
    { 
        m_payloadLen = data[2] + (data[3] << 8);
        pos += 8;
    }

    /** If we ever need masking readHeaderFixed() and previous function interfaces should be changed to 
        accept char* to enable in place unmasking. */
#if (0)
    if (masked) 
    {
        mask = *((unsigned int*)(data + pos));
        pos += 4;

        char* c = data + pos;
        for (int i = 0; i < m_payloadLen; i++)
            c[i] = c[i] ^ ((unsigned char*)(&mask))[i % 4];
    }
#endif

    if (msg_opcode == 0x0) return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME; // continuation frame ?
    if (msg_opcode == 0x1) return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME;
    if (msg_opcode == 0x2) return (msg_fin) ? BINARY_FRAME : INCOMPLETE_BINARY_FRAME;
    if (msg_opcode == 0x9) return PING_FRAME;
    if (msg_opcode == 0xA) return PONG_FRAME;

    return ERROR_FRAME;
}

bool MessageParser::readHeaderExtension(const char* data, int64_t len)
{
}

void MessageParser::reset()
{
    m_buf.clear();
    m_state = ParseState::waitingHeader;
    m_pos = 0;
    m_payloadLen = 0;
}

}
}
}