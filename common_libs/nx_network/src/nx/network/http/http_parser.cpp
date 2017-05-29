#include "http_parser.h"

namespace nx_http {

MessageParser::MessageParser():
    m_msg(nullptr)
{
}

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
            return nx::network::server::ParserState::inProgress;
        m_httpStreamReader.forceEndOfMsgBody();
        *m_msg = m_httpStreamReader.takeMessage();
        return nx::network::server::ParserState::done;
    }

    if (!m_httpStreamReader.parseBytes(buf, nx_http::BufferNpos, bytesProcessed))
        return nx::network::server::ParserState::failed;

    switch (m_httpStreamReader.state())
    {
        // TODO: #ak Currently, always reading full message before going futher.
        //  Have to add support for infinite request message body to async server
        //case HttpStreamReader::pullingLineEndingBeforeMessageBody:
        //case HttpStreamReader::readingMessageBody:
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
            return nx::network::server::ParserState::inProgress;
    }
}

void MessageParser::reset()
{
    m_httpStreamReader.resetState();
}

} // namespace nx_http
