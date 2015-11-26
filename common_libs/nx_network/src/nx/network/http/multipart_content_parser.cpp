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
        m_nextState( none ),
        m_contentLength( (unsigned int)-1 ),   //-1 means no content-length is given
        m_chunkParseState( waitingEndOfLine )
    {
    }

    MultipartContentParser::~MultipartContentParser()
    {
    }

    bool MultipartContentParser::processData( const QnByteArrayConstRef& data )
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
                            {
                                m_state = readingHeaders;
                                m_currentFrameHeaders.clear();
                            }
                            continue;

                        case readingTextData:
                            if( lineBuffer == m_startBoundaryLine || lineBuffer == m_endBoundaryLine )
                            {
                                if( !m_nextFilter->processData( m_currentFrame ) )
                                    return false;
                                m_currentFrame.clear();

                                m_state = readingHeaders;
                                m_currentFrameHeaders.clear();
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
                                    m_state = depleteLineFeedBeforeBinaryData;
                                    m_nextState = readingSizedBinaryData;
                                }
                                else
                                {
                                    const nx_http::StringType& contentType = nx_http::getHeaderValue( m_currentFrameHeaders, "Content-Type" );
                                    if( contentType == "application/text" || contentType == "text/plain" )
                                    {
                                        m_state = readingTextData;
                                    }
                                    else
                                    {
                                        m_state = depleteLineFeedBeforeBinaryData;
                                        m_nextState = readingUnsizedBinaryData;
                                    }
                                }
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

                case depleteLineFeedBeforeBinaryData:
                {
                    assert( offset < data.size() );
                    size_t bytesRead = 0;
                    m_lineSplitter.finishCurrentLineEnding( data.mid( offset ), &bytesRead );
                    offset += bytesRead;
                    m_state = m_nextState;
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
                        if( !m_nextFilter->processData( m_currentFrame ) )
                            return false;
                        m_currentFrame.clear();
                        m_contentLength = (unsigned int)-1;
                    }
                    break;
                }

                case readingUnsizedBinaryData:
                {
                    size_t localOffset = 0;
                    readUnsizedBinaryData( data.mid(offset), &localOffset );
                    offset += localOffset;
                    break;
                }

                default:
                    return false;
            }
        }

        return true;
    }

    size_t MultipartContentParser::flush()
    {
        if( m_currentFrame.isEmpty() )
            return 0;

        m_currentFrame += std::move(m_supposedBoundary);
        m_supposedBoundary.clear();
        m_nextFilter->processData( m_currentFrame );
        const size_t frameSizeBak = m_currentFrame.size();
        m_currentFrame.clear();
        return frameSizeBak;
    }

    bool MultipartContentParser::setContentType( const StringType& contentType )
    {
        static const char multipartContentType[] = "multipart/";

        //analyzing response headers (if needed)
        const nx_http::StringType::value_type* sepPos = std::find( contentType.constData(), contentType.constData()+contentType.size(), ';' );
        if( sepPos == contentType.constData()+contentType.size() )
            return false;   //unexpected content type

        if( nx_http::ConstBufferRefType(contentType, 0, sizeof(multipartContentType)-1) != multipartContentType )
            return false;   //unexpected content type

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
        m_boundaryForUnsizedBinaryParsing = "\r\n" + m_startBoundaryLine + "\r\n";
        m_boundaryForUnsizedBinaryParsingWOTrailingCRLF = "\r\n"+m_startBoundaryLine;
        m_endBoundaryLine = "--"+m_boundary+"--" /*"\r\n"*/;
    }

    const nx_http::HttpHeaders& MultipartContentParser::prevFrameHeaders() const
    {
        return m_currentFrameHeaders;
    }

    bool MultipartContentParser::readUnsizedBinaryData(
        QnByteArrayConstRef data,
        size_t* const offset )
    {
        //TODO #ak move this function (splitting byte stream by a pattern) to a separate file

        switch( m_chunkParseState )
        {
            case waitingEndOfLine:
            {
                const auto slashRPos = data.indexOf('\r');
                if( slashRPos == -1 )
                {
                    m_currentFrame += data;
                    *offset = data.size();
                    return true;
                }
                //saving data up to found \r
                m_currentFrame += data.mid( 0, slashRPos );
                *offset = slashRPos;
                m_chunkParseState = checkingForBoundaryAfterEndOfLine;
                data.pop_front( slashRPos );
                m_supposedBoundary += '\r';
                data.pop_front();
                *offset += 1;
                //*offset points to \r
            }

            case checkingForBoundaryAfterEndOfLine:
            {
                if( !m_supposedBoundary.isEmpty() )
                {
                    //if we are here, then m_supposedBoundary does not contain full boundary yet

                    //saving supposed boundary in local buffer
                    const size_t bytesNeeded = m_boundaryForUnsizedBinaryParsing.size() - m_supposedBoundary.size();
                    const int slashRPos = data.indexOf( '\r' );
                    if( (slashRPos != -1) &&
                        (slashRPos < bytesNeeded) &&    //checking within potential boundary
                        ((m_supposedBoundary + data.mid( 0, slashRPos )) != m_boundaryForUnsizedBinaryParsingWOTrailingCRLF) )
                    {
                        //boundary not found, resetting boundary check
                        m_currentFrame += m_supposedBoundary;
                        m_supposedBoundary.clear();
                        m_chunkParseState = waitingEndOfLine;
                        return true;
                    }

                    const size_t bytesToCopy = (data.size() > bytesNeeded)
                        ? bytesNeeded
                        : data.size();
                    m_supposedBoundary += data.mid( 0, bytesToCopy );
                    *offset += bytesToCopy;
                    data.pop_front( bytesToCopy );
                }

                QnByteArrayConstRef supposedBoundary;
                if( m_supposedBoundary.size() == m_boundaryForUnsizedBinaryParsing.size() )  //supposed boundary is in m_supposedBoundary buffer
                {
                    supposedBoundary = QnByteArrayConstRef( m_supposedBoundary );
                }
                else if( m_supposedBoundary.isEmpty() && (data.size() >= (size_t)m_boundaryForUnsizedBinaryParsing.size()) )   //supposed boundary is in the source data
                {
                    supposedBoundary = data.mid( 0, m_boundaryForUnsizedBinaryParsing.size() );
                    *offset += m_boundaryForUnsizedBinaryParsing.size();
                    data.pop_front( m_boundaryForUnsizedBinaryParsing.size() );
                }
                else
                {
                    //waiting for more data
                    m_supposedBoundary += data;
                    *offset += data.size();
                    return true;
                }

                //checking if boundary has been met
                if( supposedBoundary == m_boundaryForUnsizedBinaryParsing )
                {
                    //found frame delimiter
                    if( !m_nextFilter->processData( m_currentFrame ) )
                        return false;
                    m_currentFrame.clear();

                    m_state = readingHeaders;
                    m_currentFrameHeaders.clear();
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

        return true;
    }
}
