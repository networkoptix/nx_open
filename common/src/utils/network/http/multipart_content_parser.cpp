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
        m_contentLength( (unsigned int)-1 ),   //-1 means no content-length is given
        m_chunkParseState( waitingEndOfLine )
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
                                //m_state = m_contentLength == (unsigned int)-1 ? readingTextData : readingBinaryData;
                                const auto contentLengthIter = m_currentFrameHeaders.find( "Content-Length" );
                                if( contentLengthIter != m_currentFrameHeaders.end() )
                                {
                                    //Content-Length known
                                    m_contentLength = contentLengthIter->second.toUInt();
                                    m_state = readingSizedBinaryData;
                                }
                                else
                                {
                                    const nx_http::StringType& contentType = nx_http::getHeaderValue( m_currentFrameHeaders, "Content-Type" );
                                    if( contentType == "application/text" || contentType == "text/plain" )
                                        m_state = readingTextData;
                                    else
                                        m_state = readingUnsizedBinaryData;
                                }
                                m_currentFrameHeaders.clear();
                                continue;
                            }
                            QnByteArrayConstRef headerName;
                            QnByteArrayConstRef headerValue;
                            nx_http::parseHeader( &headerName, &headerValue, lineBuffer );  //ignoring result
                            m_currentFrameHeaders.emplace( headerName, headerValue );
                            break;
                        }

                        default:
                            break;
                    }
                    break;
                }

                case readingSizedBinaryData:
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

                case readingUnsizedBinaryData:
                    readUnsizedBinaryData( data, &offset );
                    break;
            }
        }
    }

    size_t MultipartContentParser::flush()
    {
        if( m_currentFrame.isEmpty() )
            return 0;

        m_nextFilter->processData( m_currentFrame );
        const size_t frameSizeBak = m_currentFrame.size();
        m_currentFrame.clear();
        return frameSizeBak;
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
        m_boundaryForUnsizedBinaryParsing = "\r\n"+m_startBoundaryLine+"\r\n";
        m_endBoundaryLine = "--"+m_boundary+"--" /*"\r\n"*/;
    }

    void MultipartContentParser::readUnsizedBinaryData(
        const QnByteArrayConstRef& data,
        size_t* const offset )
    {
        switch( m_chunkParseState )
        {
            case waitingEndOfLine:
            {
                const char* slashRPos = (const char*)memchr(
                    data.constData()+*offset,
                    '\r',
                    data.size()-*offset );
                if( slashRPos == NULL )
                {
                    m_currentFrame += data.mid( *offset );
                    *offset = data.size();
                    return;
                }
                //saving data up to found \r
                m_currentFrame += data.mid( *offset, (slashRPos-data.constData())-*offset );
                *offset = slashRPos - data.constData();
                m_chunkParseState = checkingForBoundaryAfterEndOfLine;
                //*offset points to \r
            }

            case checkingForBoundaryAfterEndOfLine:
            {
                if( !m_supposedBoundary.isEmpty() )
                {
                    //saving supposed boundary in local buffer
                    const size_t bytesNeeded = m_boundaryForUnsizedBinaryParsing.size() - m_supposedBoundary.size();
                    const size_t bytesToCopy = (data.size() - *offset > bytesNeeded)
                        ? bytesNeeded
                        : QnByteArrayConstRef::npos;
                    m_supposedBoundary += data.mid( *offset, bytesToCopy );
                    *offset += bytesToCopy;
                }

                QnByteArrayConstRef supposedBoundary;
                if( m_supposedBoundary.size() == m_boundaryForUnsizedBinaryParsing.size() )  //supposed boundary is in m_supposedBoundary buffer
                {
                    supposedBoundary = QnByteArrayConstRef( m_supposedBoundary );
                }
                else if( m_supposedBoundary.isEmpty() && (data.size() - *offset >= (size_t)m_boundaryForUnsizedBinaryParsing.size()) )   //supposed boundary is in the source data
                {
                    supposedBoundary = data.mid( *offset, m_boundaryForUnsizedBinaryParsing.size() );
                    *offset += m_boundaryForUnsizedBinaryParsing.size();
                }
                else
                {
                    //waiting for more data
                    m_supposedBoundary += data.mid( *offset );
                    *offset = data.size();
                    return;
                }

                //checking if boundary has been met
                if( supposedBoundary == m_boundaryForUnsizedBinaryParsing )
                {
                    //found frame delimiter
                    m_nextFilter->processData( m_currentFrame );
                    m_currentFrame.clear();

                    m_state = readingHeaders;
                }
                else
                {
                    //not a boundary, just frame data...
                    m_currentFrame += supposedBoundary;
                }

                m_supposedBoundary.clear();
                m_chunkParseState = waitingEndOfLine;
                break;
            }
        }
    }
}
