/**********************************************************
* 24 apr 2015
* a.kolesnikov
***********************************************************/

#include "http_message_stream_parser.h"


namespace nx_http
{
    HttpMessageStreamParser::HttpMessageStreamParser()
    {
    }
    
    HttpMessageStreamParser::~HttpMessageStreamParser()
    {
    }

    void HttpMessageStreamParser::processData( const QnByteArrayConstRef& data )
    {
        size_t bytesProcessed = 0;
        while( bytesProcessed < data.size() )
        {
            size_t localBytesProcessed = 0;
            if( !m_httpStreamReader.parseBytes( data.mid(bytesProcessed), &localBytesProcessed ) ||
                m_httpStreamReader.state() == HttpStreamReader::parseError )
            {
                m_httpStreamReader.resetState();
                //TODO #ak reporting error
            }

            //TODO #ak limiting message size since message body is aggregated in parser

            if( m_httpStreamReader.state() == HttpStreamReader::messageDone )
            {
                m_nextFilter->processData( m_httpStreamReader.fetchMessageBody() );
                m_httpStreamReader.resetState();
            }
            bytesProcessed += localBytesProcessed;
        }
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
}
