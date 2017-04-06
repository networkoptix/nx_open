#pragma once

#include <cstdint>
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
        readingHeaderFixedPart,
        readingHeaderExtension,
        readingApplicationPayload,
        readingPingPayload,
        readingPongPayload,
    };

    enum class BufferedState
    {
        notNeeded,
        enough,
        needMore
    };

    const int kFixedHeaderLen = 3;
public:
    MessageParser(bool isServer, MessageParserHandler* handler);
    void consume(const char* data, int64_t len);
    void parse(const char* data, int64_t len);
    void processPart(
        const char* data, 
        int64_t len, 
        int64_t neededLen, 
        ParseState nextState,
        void (MessageParser::*processFunc)(const char* data, int64_t len));
    void reset();

private:
    void readHeaderFixed(const char* data, int64_t len);
    void readHeaderExtension(const char* data, int64_t len);
    BufferedState bufferDataIfNeeded(const char* data, int64_t len, int64_t neededLen);
    ParseState stateByPacketType() const;

private:
    MessageParserHandler* m_handler;
    nx::Buffer m_buf;
    ParseState m_state = ParseState::readingHeaderFixedPart;
    int64_t m_pos = 0;
    int64_t m_payloadLen = 0;

    int m_headerExtLen;
    char m_opCode = 0;
    bool m_fin = false;
    bool m_masked = false;

    int m_lengthTypeField = 0;
    unsigned int m_mask = 0;
    int m_headerPos = 0;

};

}
}
}