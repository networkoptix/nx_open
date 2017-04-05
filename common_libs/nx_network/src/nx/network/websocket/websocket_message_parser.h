#pragma once

#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace websocket {

class MessageParserHandler
{
public:
    virtual void payloadReceived(const char* data, int len, bool done) = 0;
    virtual void pingReceived(const char* data, int len) = 0;
    virtual void pongReceived(const char* data, int len) = 0;
    virtual void handleError() = 0;
};

class MessageParser
{
    enum class ParseState
    {
        waitingHeader,
        readingHeader,
        readingApplicationPayload,
        readingPingPayload,
        readingPongPayload,
    };

    const int kHeaderLen = 3;
public:
    MessageParser(bool isServer, MessageParserHandler* handler);
    void consume(const char* data, int len);
    void reset();

private:
    bool parseHeader(const char* data, int len);
private:
    bool m_isServer;
    MessageParserHandler* m_handler;
    nx::Buffer m_buf;
    ParseState m_state = ParseState::waitingHeader;
    int m_pos = 0;
    int m_payloadLen = 0;
};

}
}
}