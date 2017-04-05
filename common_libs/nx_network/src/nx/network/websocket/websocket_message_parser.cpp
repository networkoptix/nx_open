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

bool MessageParser::parseHeader(const char* data, int len)
{
    char opCode = data[0] & 0x0F;
    bool fin = (data[0] >> 7) & 0x01;
    bool masked = (data[1] >> 7) & 0x01;

    int lengthTypeField = data[1] & (~0x80);
    unsigned int mask = 0;
    int pos = 0;

    if (lengthTypeField <= 125) 
    {
        m_payloadLen = lengthTypeField;
    }
    else if (lengthTypeField == 126) 
    { 
        m_payloadLen = data[2] + (data[3] << 8);
        pos += 2;
    }
    else if (lengthTypeField == 127) { 
        m_payloadLen = data[2] + (data[3] << 8);
        pos += 8;
    }

    if (msg_masked) {
        mask = *((unsigned int*)(in_buffer + pos));
        //printf("MASK: %08x\n", mask);
        pos += 4;

        // unmask data:
        unsigned char* c = in_buffer + pos;
        for (int i = 0; i < payload_length; i++) {
            c[i] = c[i] ^ ((unsigned char*)(&mask))[i % 4];
        }
    }

    if (payload_length > out_size) {
        //TODO: if output buffer is too small -- ERROR or resize(free and allocate bigger one) the buffer ?
    }

    memcpy((void*)out_buffer, (void*)(in_buffer + pos), payload_length);
    out_buffer[payload_length] = 0;
    *out_length = payload_length + 1;

    //printf("TEXT: %s\n", out_buffer);

    if (msg_opcode == 0x0) return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME; // continuation frame ?
    if (msg_opcode == 0x1) return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME;
    if (msg_opcode == 0x2) return (msg_fin) ? BINARY_FRAME : INCOMPLETE_BINARY_FRAME;
    if (msg_opcode == 0x9) return PING_FRAME;
    if (msg_opcode == 0xA) return PONG_FRAME;

    return ERROR_FRAME;
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