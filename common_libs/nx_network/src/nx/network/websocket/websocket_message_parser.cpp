#include <algorithm>
#include <nx/network/websocket/websocket_message_parser.h>

namespace nx {
namespace network {
namespace websocket {

/*
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
*/

MessageParser::MessageParser(Role role, MessageParserHandler* handler):
    m_role(role),
    m_handler(handler)
{
}

MessageParser::BufferedState MessageParser::bufferDataIfNeeded(const char* data, int64_t len, int64_t neededLen)
{
    if (neededLen < len - m_pos && m_buf.isEmpty())
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

void MessageParser::processPayload(const char* data, int64_t len)
{
    int64_t outLen = std::min(len - m_pos, m_payloadLen);
    if (m_masked) 
    {
        //for (int i = m_pos; i < m_pos + outLen; i++)
        //    data[i] = data[i] ^ ((unsigned char*)(&m_mask))[i % 4];
    }
    m_handler->framePayload(data + m_pos, outLen);
    m_payloadLen -= outLen;
    m_pos == outLen;
    if (m_payloadLen == 0)
    {
        m_handler->frameEnded();
        m_state = ParseState::readingHeaderFixedPart;
    }
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
            ParseState::readingPayload,
            &MessageParser::readHeaderExtension);
        break;
    case ParseState::readingPayload:
        processPayload(data, len);
        break;
    }
}

void MessageParser::consume(const char* data, int64_t len)
{
    m_pos = 0;
    while (m_pos < len)
        parse(data, len);
}

void MessageParser::setRole(Role role)
{
    m_role = role;
}

void MessageParser::readHeaderFixed(const char* data, int64_t len)
{
    m_opCode = (FrameType)(data[0] & 0x0F);
    m_fin = (data[0] >> 7) & 0x01;
    m_masked = (data[1] >> 7) & 0x01;

    if (!m_masked && m_role == Role::client)
        m_handler->handleError(Error::noMaskBit);

    m_lengthTypeField = data[1] & (~0x80);
    unsigned int m_mask = 0;
    int m_pos = 0;

    m_headerExtLen = (m_lengthTypeField <= 125 ? 0 : m_lengthTypeField == 126 ? 2 : 8) + (m_masked ? 4 : 0);
    m_handler->frameStarted(m_opCode, m_fin);
}

void MessageParser::readHeaderExtension(const char* data, int64_t len)
{
    if (m_lengthTypeField <= 125)
    {
        m_payloadLen = m_lengthTypeField;
    }
    else if (m_lengthTypeField == 126)
    {
        m_payloadLen = ntohs(*reinterpret_cast<const unsigned short*>(data));
        data += 2;
    }
    else if (m_lengthTypeField == 127)
    {
        m_payloadLen = ntohll(*reinterpret_cast<const uint64_t*>(data));
        data += 8;
    }

    if (m_masked)
        m_mask = *((unsigned int*)(data));
}

void MessageParser::reset()
{
    m_buf.clear();
    m_state = ParseState::readingHeaderFixedPart;
    m_pos = 0;
    m_payloadLen = 0;
}

}
}
}