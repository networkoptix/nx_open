#pragma once

#include <nx/network/websocket/websocket_types.h>

namespace nx {
namespace network {
namespace websocket {

class MessageParser
{
public:
    MessageParser(bool isServer);
    void setMessage(Message* const msg);
    nx_api::ParserState parse(const nx::Buffer& buf, size_t* bytesProcessed);
    nx_api::ParserState processEof();
    void reset();

private:
    ProcessorBaseTypePtr m_processor;
    RandomGenType m_randomGen;
    Message* const m_msg;
    bool m_isServer;
};

}
}
}