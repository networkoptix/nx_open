#include <algorithm>
#include <nx/network/websocket/websocket_message_parser.h>

namespace nx {
namespace network {
namespace websocket {


void MessageParser::consume(const char* data, int len)
{
    switch (m_state)
    {
    case ParseState::waitingHeader:
        if (len < kHeaderLen)
        {
            m_buf.append(data, len);
            if (m_buf.size() < kHeaderLen)
            {
                m_state = ParseState::readingHeader;
                return;
            }

            parseHeader(m_buf.constData(), m_buf.size());
        }
        else
        {
            parseHeader(data, len);
        }
        return;
    case ParseState::readingHeader:
    {
        int appendLen = std::min(kHeaderLen - m_buf.size(), len);
        m_buf.append(data, appendLen);
        if (m_buf.size() < kHeaderLen)
            return;
        parseHeader(m_buf.constData()), m_buf.size());
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
        parseHeader(data + appendLen, len - appendLen);
        return;
    }
    case ParseState::readingApplicationPayload:
    {
        int toRead = std::min(m_payloadLen - m_pos, len);
        m_handler->payloadReceived(data, toRead);
        m_payloadLen -= toRead;
        if (m_payloadLen != 0)
            return;
        reset();
        parseHeader(data + toRead, len - toRead);
        return;
    }
    }
}

bool parseHeader(const char* data, int len)
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