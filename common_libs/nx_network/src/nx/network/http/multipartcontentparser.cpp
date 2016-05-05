/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#include "multipartcontentparser.h"


namespace nx_http
{
    MultipartContentParserHelper::MultipartContentParserHelper( const StringType& boundary )
    :
        m_boundary( boundary ),
        m_startBoundaryLine( "--"+boundary+"\r\n" ), //--boundary\r\n
        m_endBoundaryLine( "--"+boundary+"--\r\n" ),
        m_state( readingBoundary )
    {
    }

    MultipartContentParserHelper::~MultipartContentParserHelper()
    {
    }

    MultipartContentParserHelper::ResultCode MultipartContentParserHelper::parseBytes(
        const ConstBufferRefType& data,
        size_t* bytesProcessed )
    {
        for( size_t offset = 0; offset < data.size(); )
        {
            //TODO/IMPL for now, parsing only text
            ConstBufferRefType lineBuffer;
            size_t bytesRead = 0;
            const bool lineFound = m_lineSplitter.parseByLines(
                data.mid( offset ),
                &lineBuffer,
                &bytesRead );
            offset += bytesRead;
            *bytesProcessed += bytesRead;

            switch( m_state )
            {
                case readingBoundary:
                    if( lineBuffer == m_startBoundaryLine )
                        m_state = readingHeaders;

                    ////we need to find boundary in the source stream
                    //if( (data.size()-offset) < m_startBoundaryLine.size()-m_searchBoundaryCache.size() || !m_searchBoundaryCache.isEmpty() )
                    //{
                    //    //not enough bytes in source buffer
                    //    const size_t bytesToCopy = std::min<size_t>(m_startBoundaryLine.size()-m_searchBoundaryCache.size(), data.size()-offset);
                    //    m_searchBoundaryCache.append( data.data()+offset, bytesToCopy );
                    //    offset += bytesToCopy;
                    //    *bytesProcessed += bytesToCopy;
                    //    if( m_searchBoundaryCache.size() < m_startBoundaryLine.size() )
                    //        return needMoreData;
                    //}
        
                    //m_state = skippingCurrentLine;
                    //if( !m_searchBoundaryCache.isEmpty() )
                    //{
                    //    if( m_searchBoundaryCache == m_startBoundaryLine )
                    //        m_state = readingHeaders;
                    //}
                    //else
                    //{
                    //    NX_ASSERT( data.size()-offset >= m_startBoundaryLine.size() );
                    //    if( data.mid(offset) == m_startBoundaryLine )
                    //        m_state = readingHeaders;
                    //    offset += m_startBoundaryLine.size();
                    //}
                    break;

                case skippingCurrentLine:
                    //TODO/IMPL
                    break;

                case readingData:
                    if( lineBuffer == "--"+m_boundary )
                    {
                        m_state = readingHeaders;
                        return partDataDone;
                    }
                    else
                    {
                        m_prevFoundData = lineBuffer;
                        return someDataAvailable;
                    }
                    break;

                case readingHeaders:
                {
                    if( !lineFound )
                        return needMoreData;
                    if( lineBuffer.isEmpty() )
                    {
                        m_state = readingData;
                        return headersAvailable;    //end of headers mark
                    }
                    nx_http::StringType headerName;
                    nx_http::StringType headerValue;
                    if( !parseHeader( &headerName, &headerValue, lineBuffer ) )
                        return parseError;
                    m_headers.insert( std::make_pair( headerName, headerValue ) );
                    break;
                }
            }
        }

        return needMoreData;
    }

    ConstBufferRefType MultipartContentParserHelper::prevFoundData() const
    {
        return m_prevFoundData;
    }

    /*!
        \note Only valid after \a parseBytes returned \a partRead. Next \a parseBytes call can invalidate it
    */
    StringType MultipartContentParserHelper::partContentType() const
    {
        return m_partContentType;
    }

    /*!
        \note Only valid after \a parseBytes returned \a partRead. Next \a parseBytes call can invalidate it
    */
    const HttpHeaders& MultipartContentParserHelper::partHeaders() const
    {
        return m_headers;
    }

    void MultipartContentParserHelper::setBoundary( const StringType& boundary )
    {
        //boundary can contain starting -- (depends on implementation. e.g. axis P1344 does so)
        m_boundary = boundary.startsWith( "--" ) ? boundary.mid(2, boundary.size()-2) : boundary;
        m_startBoundaryLine = "--"+m_boundary/*+"\r\n"*/; //--boundary\r\n
        m_endBoundaryLine = "--"+m_boundary+"--" /*"\r\n"*/;
    }

    bool MultipartContentParserHelper::setContentType(const StringType& contentType)
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
}
