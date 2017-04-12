#include "http_message_stream_parser.h"

namespace nx_http {

HttpMessageStreamParser::HttpMessageStreamParser()
{
}
    
HttpMessageStreamParser::~HttpMessageStreamParser()
{
}

bool HttpMessageStreamParser::processData( const QnByteArrayConstRef& data )
{
    size_t bytesProcessed = 0;
    while( bytesProcessed < data.size() )
    {
        size_t localBytesProcessed = 0;
        if( !m_httpStreamReader.parseBytes( data.mid(bytesProcessed), &localBytesProcessed ) ||
            m_httpStreamReader.state() == HttpStreamReader::parseError )
        {
            m_httpStreamReader.resetState();
            //reporting error
            return false;
        }

        //TODO #ak limiting message size since message body is aggregated in parser

        if( m_httpStreamReader.state() == HttpStreamReader::messageDone )
        {
            if( !m_nextFilter->processData( m_httpStreamReader.fetchMessageBody() ) )
                return false;
            m_httpStreamReader.resetState();
        }
        bytesProcessed += localBytesProcessed;
    }

    return true;
}

size_t HttpMessageStreamParser::flush()
{
    if( m_httpStreamReader.state() > HttpStreamReader::readingMessageHeaders )
        m_nextFilter->processData( m_httpStreamReader.fetchMessageBody() );
    return 0;
}

nx_http::Message HttpMessageStreamParser::currentMessage() const
{
    return m_httpStreamReader.message();
}

} // namespace nx_http
