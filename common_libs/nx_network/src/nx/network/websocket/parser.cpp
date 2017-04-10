#include <algorithm>
#include <nx/network/websocket/parser.h>

namespace nx {
namespace network {
namespace websocket {

/*
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
*/

Parser::Parser(Role role, ParserHandler* handler):
    m_role(role),
    m_handler(handler)
{
}

Parser::BufferedState Parser::bufferDataIfNeeded(const char* data, int64_t len, int64_t neededLen)
{
    if (neededLen < len - m_pos && m_buf.isEmpty())
        return BufferedState::notNeeded;

    auto appendLen = std::min(neededLen - (int64_t)m_buf.size(), len - m_pos);
    m_buf.append(data + m_pos, appendLen);
    m_pos += appendLen;

    if (m_buf.size() < neededLen)
        return BufferedState::needMore;

    return BufferedState::enough;
}

void Parser::processPayload(char* data, int64_t len)
{
    int64_t outLen = std::min(len - m_pos, m_payloadLen);
    if (m_masked) 
    {
        for (int i = m_pos; i < m_pos + outLen; i++)
            data[i] = data[i] ^ ((unsigned char*)(&m_mask))[i % 4];
    }
    m_handler->framePayload(data + m_pos, outLen);
    m_payloadLen -= outLen;
    m_pos += outLen;
    if (m_payloadLen == 0)
    {
        m_handler->frameEnded();
        if (m_fin)
            m_handler->messageEnded();
        m_state = ParseState::readingHeaderFixedPart;
    }
}

void Parser::processPart(
    char* data, 
    int64_t len, 
    int64_t neededLen,
    ParseState nextState,
    void (Parser::*processFunc)(char* data))
{
    switch (bufferDataIfNeeded(data, len, neededLen))
    {
    case BufferedState::needMore:
        break;
    case BufferedState::notNeeded:
        (this->*processFunc)(data);
        m_state = nextState;
        m_pos += neededLen;
        break;
    case BufferedState::enough:
        (this->*processFunc)(m_buf.data());
        m_state = nextState;
        m_buf.clear();
        break;
    }
}

void Parser::parse(char* data, int64_t len)
{
    switch (m_state)
    {
    case ParseState::readingHeaderFixedPart:
        processPart(
            data, 
            len, 
            kFixedHeaderLen, 
            ParseState::readingHeaderExtension,
            &Parser::readHeaderFixed);
        break;
    case ParseState::readingHeaderExtension:
        processPart(
            data,
            len,
            m_headerExtLen,
            ParseState::readingPayload,
            &Parser::readHeaderExtension);
        break;
    case ParseState::readingPayload:
        processPayload(data, len);
        break;
    }
}

void Parser::consume(char* data, int64_t len)
{
    m_pos = 0;
    while (m_pos < len)
        parse(data, len);
}

void Parser::setRole(Role role)
{
    m_role = role;
}

void Parser::readHeaderFixed(char* data)
{
    m_opCode = (FrameType)(data[0] & 0x0F);
    m_fin = (data[0] >> 7) & 0x01;
    m_masked = (data[1] >> 7) & 0x01;
    if (!m_masked && m_role == Role::server)
        m_handler->handleError(Error::noMaskBit);

    m_lengthTypeField = data[1] & (~0x80);
    m_headerExtLen = (m_lengthTypeField <= 125 ? 0 : m_lengthTypeField == 126 ? 2 : 8) + (m_masked ? 4 : 0);
    m_handler->frameStarted(m_opCode, m_fin);
}

void Parser::readHeaderExtension(char* data)
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

void Parser::reset()
{
    m_buf.clear();
    m_state = ParseState::readingHeaderFixedPart;
    m_pos = 0;
    m_payloadLen = 0;
}

}
}
}