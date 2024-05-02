// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_parser.h"

#include <nx/utils/log/log_main.h>

namespace nx::network::http {

MessageParser::MessageParser()
{
    m_httpStreamReader.setBreakAfterReadingHeaders(true);
}

void MessageParser::setMessage(Message* const message)
{
    m_message = message;
}

nx::network::server::ParserState MessageParser::parse(
    const nx::ConstBufferRefType& buffer,
    size_t* bytesProcessed)
{
    if (buffer.empty())
    {
        if (m_httpStreamReader.state() != HttpStreamReader::ReadState::readingMessageBody)
            return nx::network::server::ParserState::readingMessage;

        *m_message = m_httpStreamReader.takeMessage();

        if (m_httpStreamReader.contentLength() &&
            *m_httpStreamReader.contentLength() != m_httpStreamReader.messageBodyBytesRead())
        {
            return nx::network::server::ParserState::failed;
        }
        else
        {
            m_httpStreamReader.forceEndOfMsgBody();
            return nx::network::server::ParserState::done;
        }
    }

    const auto stateBeforeParse = m_httpStreamReader.state();

    if (!m_httpStreamReader.parseBytes(buffer, bytesProcessed)
        || m_httpStreamReader.state() == HttpStreamReader::ReadState::parseError)
    {
        NX_DEBUG(this, "Failed to parse HTTP stream: state before parsing: %1",
            stateBeforeParse);
        return nx::network::server::ParserState::failed;
    }

    switch (m_httpStreamReader.state())
    {
        case HttpStreamReader::ReadState::waitingMessageStart:
        case HttpStreamReader::ReadState::readingMessageHeaders:
            return nx::network::server::ParserState::readingMessage;

        case HttpStreamReader::ReadState::pullingLineEndingBeforeMessageBody:
        case HttpStreamReader::ReadState::readingMessageBody:
        {
            provideMessageIfNeeded();
            return nx::network::server::ParserState::readingBody;
        }

        case HttpStreamReader::ReadState::messageDone:
        {
            provideMessageIfNeeded();
            return nx::network::server::ParserState::done;
        }

        case HttpStreamReader::ReadState::parseError:
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

const HttpStreamReader& MessageParser::streamReader() const
{
    return m_httpStreamReader;
}

HttpStreamReader& MessageParser::streamReader()
{
    return m_httpStreamReader;
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
    const nx::ConstBufferRefType& buffer,
    size_t* bytesProcessed)
{
    if (buffer.empty())
    {
        if (m_httpStreamReader.state() != HttpStreamReader::ReadState::readingMessageBody)
            return nx::network::server::ParserState::readingMessage;
        m_httpStreamReader.forceEndOfMsgBody();
        *m_message = m_httpStreamReader.takeMessage();
        return nx::network::server::ParserState::done;
    }

    if (!m_httpStreamReader.parseBytes(buffer, bytesProcessed))
        return nx::network::server::ParserState::failed;

    switch (m_httpStreamReader.state())
    {
        case HttpStreamReader::ReadState::messageDone:
            *m_message = m_httpStreamReader.takeMessage();
            if (m_message->type == MessageType::request)
                m_message->request->messageBody = m_httpStreamReader.fetchMessageBody();
            else if (m_message->type == MessageType::response)
                m_message->response->messageBody = m_httpStreamReader.fetchMessageBody();
            return nx::network::server::ParserState::done;

        case HttpStreamReader::ReadState::parseError:
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

} // namespace nx::network::http
