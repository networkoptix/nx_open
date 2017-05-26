#include "http_parser.h"

namespace nx_http {

void MessageParser::setMessage(Message* const msg)
{
    m_msg = msg;
}

nx::network::server::ParserState MessageParser::parse(
    const nx::Buffer& buf,
    size_t* bytesProcessed)
{
    if (buf.isEmpty())
    {
        if (m_httpStreamReader.state() != HttpStreamReader::readingMessageBody)
            return nx::network::server::ParserState::readingMessage;
        m_httpStreamReader.forceEndOfMsgBody();
        *m_msg = m_httpStreamReader.takeMessage();
        return nx::network::server::ParserState::done;
    }

    if (!m_httpStreamReader.parseBytes(buf, nx_http::BufferNpos, bytesProcessed))
        return nx::network::server::ParserState::failed;

    if (m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders)
        return nx::network::server::ParserState::readingMessage;

    switch (m_httpStreamReader.state())
    {
        // TODO: #ak Currently, always reading full message before going futher.
        //  Have to add support for infinite request message body to async server
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
    *m_msg = m_httpStreamReader.takeMessage();
    m_messageTaken = true;
}

//-------------------------------------------------------------------------------------------------

namespace deprecated {

void MessageParser::setMessage(Message* const msg)
{
    m_msg = msg;
}

nx::network::server::ParserState MessageParser::parse(
    const nx::Buffer& buf,
    size_t* bytesProcessed)
{
    if (buf.isEmpty())
    {
        if (m_httpStreamReader.state() != HttpStreamReader::readingMessageBody)
            return nx::network::server::ParserState::readingMessage;
        m_httpStreamReader.forceEndOfMsgBody();
        *m_msg = m_httpStreamReader.takeMessage();
        return nx::network::server::ParserState::done;
    }

    if (!m_httpStreamReader.parseBytes(buf, nx_http::BufferNpos, bytesProcessed))
        return nx::network::server::ParserState::failed;

    switch (m_httpStreamReader.state())
    {
        case HttpStreamReader::messageDone:
            *m_msg = m_httpStreamReader.takeMessage();
            if (m_msg->type == MessageType::request)
                m_msg->request->messageBody = m_httpStreamReader.fetchMessageBody();
            else if (m_msg->type == MessageType::response)
                m_msg->response->messageBody = m_httpStreamReader.fetchMessageBody();
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

} // namespace nx_http
