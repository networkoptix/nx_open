#include "http_parser.h"

namespace nx {
namespace network {
namespace http {

MessageParser::MessageParser()
{
    m_httpStreamReader.setBreakAfterReadingHeaders(true);
}

void MessageParser::setMessage(Message* const message)
{
    m_message = message;
}

nx::network::server::ParserState MessageParser::parse(
    const nx::Buffer& buffer,
    size_t* bytesProcessed)
{
    if (buffer.isEmpty())
    {
        if (m_httpStreamReader.state() != HttpStreamReader::readingMessageBody)
            return nx::network::server::ParserState::readingMessage;
        m_httpStreamReader.forceEndOfMsgBody();
        *m_message = m_httpStreamReader.takeMessage();
        return nx::network::server::ParserState::done;
    }

    if (!m_httpStreamReader.parseBytes(buffer, nx::utils::BufferNpos, bytesProcessed))
        return nx::network::server::ParserState::failed;

    switch (m_httpStreamReader.state())
    {
        case HttpStreamReader::waitingMessageStart:
        case HttpStreamReader::readingMessageHeaders:
            return nx::network::server::ParserState::readingMessage;

        case HttpStreamReader::pullingLineEndingBeforeMessageBody:
        case HttpStreamReader::readingMessageBody:
        {
            provideMessageIfNeeded();
            return nx::network::server::ParserState::readingBody;
        }

        case HttpStreamReader::messageDone:
        {
            provideMessageIfNeeded();
            return nx::network::server::ParserState::done;
        }

        case HttpStreamReader::parseError:
            return nx::network::server::ParserState::failed;
    }

    return nx::network::server::ParserState::failed;
}

nx::Buffer MessageParser::fetchMessageBody()
{
    return m_httpStreamReader.fetchMessageBody();
}

void MessageParser::reset()
{
    m_httpStreamReader.resetState();
    m_messageTaken = false;
}

void MessageParser::provideMessageIfNeeded()
{
    if (m_messageTaken)
        return;
    *m_message = m_httpStreamReader.takeMessage();
    m_messageTaken = true;
}

//-------------------------------------------------------------------------------------------------

namespace deprecated {

void MessageParser::setMessage(Message* const message)
{
    m_message = message;
}

nx::network::server::ParserState MessageParser::parse(
    const nx::Buffer& buffer,
    size_t* bytesProcessed)
{
    if (buffer.isEmpty())
    {
        if (m_httpStreamReader.state() != HttpStreamReader::readingMessageBody)
            return nx::network::server::ParserState::readingMessage;
        m_httpStreamReader.forceEndOfMsgBody();
        *m_message = m_httpStreamReader.takeMessage();
        return nx::network::server::ParserState::done;
    }

    if (!m_httpStreamReader.parseBytes(buffer, nx::utils::BufferNpos, bytesProcessed))
        return nx::network::server::ParserState::failed;

    switch (m_httpStreamReader.state())
    {
        case HttpStreamReader::messageDone:
            *m_message = m_httpStreamReader.takeMessage();
            if (m_message->type == MessageType::request)
                m_message->request->messageBody = m_httpStreamReader.fetchMessageBody();
            else if (m_message->type == MessageType::response)
                m_message->response->messageBody = m_httpStreamReader.fetchMessageBody();
            return nx::network::server::ParserState::done;

        case HttpStreamReader::parseError:
            return nx::network::server::ParserState::failed;

        default:
            return nx::network::server::ParserState::readingMessage;
    }
}

void MessageParser::reset()
{
    m_httpStreamReader.resetState();
}

} // namespace deprecated

} // namespace nx
} // namespace network
} // namespace http
