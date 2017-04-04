#include <nx/network/websocket/websocket_message_parser.h>

namespace nx {
namespace network {
namespace websocket {

MessageParser::MessageParser(bool isServer):
    m_processor(
        new ProcessorHybi13Type(
            true,
            isServer,
            ConnectionMessageManagerTypePtr(new ConnectionMessageManagerType),
            m_randomGen)),
    m_isServer(isServer)
{
}

void MessageParser::setMessage(Message* const msg)
{
    m_msg = msg;
}

nx_api::ParserState MessageParser::parse(const nx::Buffer& buf, size_t* bytesProcessed)
{
    lib::error_code ec;
    auto processed = m_processor->consume(buf.data(), buf.size(), ec);
    if (bytesProcessed)
        *bytesProcessed = processed;

    if (ec)
        return ParserState::failed;

    if (m_processor->ready())
        return ParserState::done;
    
    return ParserState::inProgress;
}

nx_api::ParserState MessageParser::processEof()
{
}

void MessageParser::reset()
{
    m_processor.reset(
        new ProcessorHybi13Type(
            true,
            m_isServer,
            ConnectionMessageManagerTypePtr(new ConnectionMessageManagerType),
            m_randomGen));
}

}
}
}