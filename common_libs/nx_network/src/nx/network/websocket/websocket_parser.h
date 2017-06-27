#pragma once

#include <nx/network/buffer.h>
#include "websocket_common_types.h"

namespace nx {
namespace network {
namespace websocket {

class ParserHandler
{
public:
    virtual void frameStarted(FrameType type, bool fin) = 0;
    virtual void framePayload(const char* data, int len) = 0;
    virtual void frameEnded() = 0;
    virtual void messageEnded() = 0;
    virtual void handleError(Error err) = 0;

    virtual ~ParserHandler() {}
};

class NX_NETWORK_API Parser
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
    Parser(Role role, ParserHandler* handler);
    void consume(char* data, int len);
    void consume(nx::Buffer& buf);
    void setRole(Role role);
    FrameType frameType() const;
    int frameSize() const;

private:
    void parse(char* data, int len);
    void processPart(
        char* data,
        int len,
        int neededLen,
        ParseState (Parser::*processFunc)(char* data));
    ParseState processPayload(char* data, int len);
    void reset();
    ParseState readHeaderFixed(char* data);
    ParseState readHeaderExtension(char* data);
    BufferedState bufferDataIfNeeded(const char* data, int len, int neededLen);

private:
    Role m_role = Role::undefined;
    ParserHandler* m_handler;
    nx::Buffer m_buf;
    ParseState m_state = ParseState::readingHeaderFixedPart;
    int m_pos = 0;
    int m_payloadLen = 0;

    int m_headerExtLen;
    FrameType m_opCode;
    bool m_fin = false;
    bool m_masked = false;

    unsigned int m_mask = 0;
    int m_maskPos;
};

} // namespace websocket
} // namespace network
} // namespace nx
