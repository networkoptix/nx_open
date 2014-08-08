/**********************************************************
* 12 dec 2013
* a.kolesnikov
***********************************************************/

#include "multipart_content_parser.h"


namespace nx_http
{
    MultipartContentParser::MultipartContentParser()
    :
        m_state( waitingBoundary ),
        m_contentLength( (unsigned int)-1 )   //-1 means no content-length is given
    {
    }

    MultipartContentParser::~MultipartContentParser()
    {
    }

    void MultipartContentParser::processData( const QnByteArrayConstRef& data )
    {
        for( size_t offset = 0; offset < data.size(); )
        {
            switch( m_state )
            {
                case waitingBoundary:
                case readingHeaders:
                case readingTextData:
                {
                    ConstBufferRefType lineBuffer;
                    size_t bytesRead = 0;
                    const bool lineFound = m_lineSplitter.parseByLines(
                        data.mid( offset ),
                        &lineBuffer,
                        &bytesRead );
                    offset += bytesRead;
                    if( !lineFound )
                        continue;

                    switch( m_state )
                    {
                        case waitingBoundary:
                            if( lineBuffer == m_startBoundaryLine || lineBuffer == m_endBoundaryLine )
                                m_state = readingHeaders;
                            continue;

                        case readingTextData:
                            if( lineBuffer == m_startBoundaryLine || lineBuffer == m_endBoundaryLine )
                            {
                                m_state = readingHeaders;
                                m_nextFilter->processData( m_currentFrame );
                                m_currentFrame.clear();
                                continue;
                            }
                            m_currentFrame += lineBuffer;
                            break;

                        case readingHeaders:
                        {
                            if( lineBuffer.isEmpty() )
                            {
                                m_state = m_contentLength == (unsigned int)-1 ? readingTextData : readingBinaryData;
                                continue;
                            }
                            QnByteArrayConstRef headerName;
                            QnByteArrayConstRef headerValue;
                            nx_http::parseHeader( &headerName, &headerValue, lineBuffer );  //ignoring result
                            if( headerName.isEqualCaseInsensitive("Content-Length") )
                                m_contentLength = headerValue.toUInt();
                            break;
                        }

                        default:
                            break;
                    }
                    break;
                }

                case readingBinaryData:
                {
                    assert( m_contentLength != (unsigned int)-1 );
                    const size_t bytesToRead = std::min<size_t>( m_contentLength - m_currentFrame.size(), data.size()-offset );
                    m_currentFrame += data.mid( offset, bytesToRead );
                    offset += bytesToRead;
                    if( (size_t)m_currentFrame.size() == m_contentLength )
                    {
                        m_state = waitingBoundary;
                        m_nextFilter->processData( m_currentFrame );
                        m_currentFrame.clear();
                        m_contentLength = (unsigned int)-1;
                    }
                    break;
                }
            }
        }
    }

    size_t MultipartContentParser::flush()
    {
        if( m_currentFrame.isEmpty() )
            return 0;

        //TODO/IMPL: #ak pushing forward incomplete frame
        return 0;
    }

    bool MultipartContentParser::setContentType( const StringType& contentType )
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        //analyzing response headers (if needed)
        const nx_http::StringType::value_type* sepPos = std::find( contentType.constData(), contentType.constData()+contentType.size(), ';' );
        if( sepPos == contentType.constData()+contentType.size() ||
            nx_http::ConstBufferRefType(contentType, 0, sepPos-contentType.constData()) != multipartContentType )
        {
            //unexpected content type
            return false;
        }

        const nx_http::StringType::value_type* boundaryStart = std::find_if(
            sepPos+1,
            contentType.constData()+contentType.size(),
            std::not1( std::bind1st( std::equal_to<nx_http::StringType::value_type>(), ' ' ) ) );   //searching first non-space
        if( boundaryStart == contentType.constData()+contentType.size() )
        {
            //failed to read boundary marker
            return false;
        }
        if( !nx_http::ConstBufferRefType(contentType, boundaryStart-contentType.constData()).startsWith("boundary=") )
        {
            //failed to read boundary marker
            return false;
        }
        boundaryStart += sizeof("boundary=")-1;
        setBoundary( contentType.mid( boundaryStart-contentType.constData() ) );

        return true;
    }

    void MultipartContentParser::setBoundary( const StringType& boundary )
    {
        //boundary can contain starting -- (depends on implementation. e.g. axis P1344 does so)
        m_boundary = boundary.startsWith( "--" ) ? boundary.mid(2, boundary.size()-2) : boundary;
        //dropping starting and trailing quotes
        while( !m_boundary.isEmpty() && m_boundary[0] == '"' )
            m_boundary.remove( 0, 1 );
        while( !m_boundary.isEmpty() && m_boundary[m_boundary.size() - 1] == '"' )
            m_boundary.remove( m_boundary.size() - 1, 1 );
        m_startBoundaryLine = "--" + m_boundary/*+"\r\n"*/; //--boundary\r\n
        m_endBoundaryLine = "--"+m_boundary+"--" /*"\r\n"*/;
    }
}
