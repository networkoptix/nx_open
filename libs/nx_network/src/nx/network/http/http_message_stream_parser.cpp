// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_message_stream_parser.h"

namespace nx::network::http {

bool HttpMessageStreamParser::processData(const nx::ConstBufferRefType& data)
{
    size_t bytesProcessed = 0;
    while (bytesProcessed < data.size())
    {
        std::size_t localBytesProcessed = 0;
        if (!m_httpStreamReader.parseBytes(data.substr(bytesProcessed), &localBytesProcessed) ||
            m_httpStreamReader.state() == HttpStreamReader::ReadState::parseError)
        {
            m_httpStreamReader.resetState();
            return false;
        }

        // TODO: #akolesnikov Limiting message size since message body is aggregated in the parser.

        if (m_httpStreamReader.state() == HttpStreamReader::ReadState::messageDone)
        {
            if (!m_nextFilter->processData(m_httpStreamReader.fetchMessageBody()))
                return false;
            m_httpStreamReader.resetState();
        }
        bytesProcessed += localBytesProcessed;
    }

    return true;
}

size_t HttpMessageStreamParser::flush()
{
    if (m_httpStreamReader.state() > HttpStreamReader::ReadState::readingMessageHeaders)
        m_nextFilter->processData(m_httpStreamReader.fetchMessageBody());
    return 0;
}

nx::network::http::Message HttpMessageStreamParser::currentMessage() const
{
    return m_httpStreamReader.message();
}

} // namespace nx::network::http
