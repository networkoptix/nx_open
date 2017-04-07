#pragma once

#include <cstdint>
#include <nx/network/buffer.h>
#include <nx/network/websocket/websocket_types.h>

namespace nx {
namespace network {
namespace websocket {

class MessageParserHandler
{
public:
    virtual void frameStarted(FrameType type, bool fin) = 0;
    virtual void framePayload(const char* data, int64_t len) = 0;
    virtual void frameEnded() = 0;
    virtual void messageEnded() = 0;
    virtual void handleError(Error err) = 0;
};

class MessageParser
{
    enum class ParseState
    {
        readingHeaderFixedPart,
        readingHeaderExtension,
        readingPayload,
    };

    enum class BufferedState
    {
        notNeeded,
        enough,
        needMore
    };

    const int kFixedHeaderLen = 2;
public:
    MessageParser(Role role, MessageParserHandler* handler);
    void consume(const char* data, int64_t len);
    void setRole(Role role);

private:
    void parse(const char* data, int64_t len);
    void processPart(
        const char* data, 
        int64_t len, 
        int64_t neededLen, 
        ParseState nextState,
        void (MessageParser::*processFunc)(const char* data, int64_t len));
    void processPayload(const char* data, int64_t len);
    void reset();
    void readHeaderFixed(const char* data, int64_t len);
    void readHeaderExtension(const char* data, int64_t len);
    BufferedState bufferDataIfNeeded(const char* data, int64_t len, int64_t neededLen);

private:
    MessageParserHandler* m_handler;
    nx::Buffer m_buf;
    ParseState m_state = ParseState::readingHeaderFixedPart;
    int64_t m_pos = 0;
    int64_t m_payloadLen = 0;
    Role m_role = Role::undefined;

    int m_headerExtLen;
    FrameType m_opCode;
    bool m_fin = false;
    bool m_masked = false;

    int m_lengthTypeField = 0;
    unsigned int m_mask = 0;
    int m_headerPos = 0;

};

}
}
}