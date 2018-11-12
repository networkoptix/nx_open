#include "http_message_stream_parser.h"

namespace nx {
namespace network {
namespace http {

bool HttpMessageStreamParser::processData(const QnByteArrayConstRef& data)
{
    size_t bytesProcessed = 0;
    while (bytesProcessed < data.size())
    {
        std::size_t localBytesProcessed = 0;
        if (!m_httpStreamReader.parseBytes(data.mid(bytesProcessed), &localBytesProcessed) ||
            m_httpStreamReader.state() == HttpStreamReader::parseError)
        {
            m_httpStreamReader.resetState();
            return false;
        }

        // TODO: #ak Limiting message size since message body is aggregated in the parser.

        if (m_httpStreamReader.state() == HttpStreamReader::messageDone)
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
    if (m_httpStreamReader.state() > HttpStreamReader::readingMessageHeaders)
        m_nextFilter->processData(m_httpStreamReader.fetchMessageBody());
    return 0;
}

nx::network::http::Message HttpMessageStreamParser::currentMessage() const
{
    return m_httpStreamReader.message();
}

} // namespace nx
} // namespace network
} // namespace http
