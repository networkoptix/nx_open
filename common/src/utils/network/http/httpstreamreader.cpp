/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "httpstreamreader.h"

#include <algorithm>


namespace nx_http
{
    HttpStreamReader::HttpStreamReader()
    :
        m_state( waitingMessageStart ),
        m_contentLength( 0 ),
        m_isChunkedTransfer( false ),
        m_messageBodyBytesRead( 0 ),
        m_chunkStreamParseState( waitingChunkStart ),
        m_currentChunkSize( 0 ),
        m_currentChunkBytesRead( 0 ),
        m_prevChar( 0 )
    {
    }

    HttpStreamReader::~HttpStreamReader()
    {
    }

    /*!
        \return false on parse error
    */
    bool HttpStreamReader::parseBytes( const BufferType& data, size_t count, size_t* bytesProcessed )
    {
        if( bytesProcessed )
            *bytesProcessed = 0;
        if( count == nx_http::BufferNpos )
            count = data.size();
        //reading line-by-line
        for( size_t currentDataPos = 0; currentDataPos < count; )
        {
            switch( m_state )
            {
                case readingMessageBody:
                {
                    //if m_contentLength == 0 and m_state == readingMessageBody then content-length is unknown
                    const size_t msgBodyBytesRead = m_isChunkedTransfer
                        ? readChunkStream( data, currentDataPos, count-currentDataPos )
                        : readIdentityStream( data, currentDataPos, count-currentDataPos );
                    if( msgBodyBytesRead == (size_t)-1 )
                        return false;
                    currentDataPos += msgBodyBytesRead;
                    if( bytesProcessed )
                        *bytesProcessed = currentDataPos;
                    if( (m_contentLength > 0 &&         //Content-Length known
                         m_messageBodyBytesRead >= m_contentLength) ||     //read all message body
                        (m_isChunkedTransfer && m_chunkStreamParseState == reachedChunkStreamEnd) )
                    {
                        m_state = messageDone;
                        return true;
                    }
                    break;
                }

                default:
                {
                    ConstBufferRefType lineBuffer;
                    size_t bytesRead = 0;
                    const bool lineFound = m_lineSplitter.parseByLines(
                        ConstBufferRefType( data, currentDataPos, count-currentDataPos ),
                        &lineBuffer,
                        &bytesRead );
                    currentDataPos += bytesRead;
                    if( bytesProcessed )
                        *bytesProcessed = currentDataPos;
                    if( !lineFound )
                        break;
                    if( !parseLine( lineBuffer ) )
                    {
                        m_state = parseError;
                        return false;
                    }
                    break;
                }
            }
        }

        return true;
    }

    /*!
        \return Actual only after state changed from \a readingMessageHeaders to \a waitingMessageStart or \a readingMessageBody
    */
    const HttpMessage& HttpStreamReader::message() const
    {
        return m_httpMessage;
    }

    HttpStreamReader::ReadState HttpStreamReader::state() const
    {
        return waitingMessageStart;
    }

    size_t HttpStreamReader::messageBodyBufferSize() const
    {
        return m_msgBodyBuffer.size();
    }

    //!Returns internal message body buffer and clears internal buffer
    BufferType HttpStreamReader::fetchMessageBody()
    {
        //BufferType result;
        //m_msgBodyBuffer.swap( result );
        //return result;
        BufferType result = m_msgBodyBuffer;
        m_msgBodyBuffer.clear();
        return result;
    }

    QString HttpStreamReader::errorText() const
    {
        //TODO/IMPL
        return QString();
    }

    void HttpStreamReader::resetState()
    {
        m_state = waitingMessageStart;
        m_httpMessage.clear();
        m_lineSplitter.reset();
        m_contentLength = 0;
        m_isChunkedTransfer = false;
        m_messageBodyBytesRead = 0;
        m_msgBodyBuffer.clear();

        m_chunkStreamParseState = waitingChunkStart;
        m_currentChunkSize = 0;
        m_currentChunkBytesRead = 0;
        m_prevChar = 0;
    }
 
    bool HttpStreamReader::parseLine( const ConstBufferRefType& data )
    {
        switch( m_state )
        {
            case messageDone:
            case parseError:
                m_httpMessage.clear();
                m_state = waitingMessageStart;  //starting parsing next message
                m_lineSplitter.reset();
                m_contentLength = 0;
                m_messageBodyBytesRead = 0;
                m_msgBodyBuffer.clear();

            case waitingMessageStart:
            {
                if( data.isEmpty() )
                    return true; //skipping empty lines
                //deciding what we parsing: request line or status line
                    //splitting to tokens separated by ' '
                    //if first token contains '/' then considering line to be status line, else it is request line
                //const size_t pos = data.find_first_of( " /\r\n", offset, sizeof(" /\r\n")-1 );
                static const char separators[] = " /\r\n";
                const char* pos = std::find_first_of( data.data(), data.data()+data.size(), separators, separators + sizeof(separators)-1 );
                if( pos == data.data()+data.size() || *pos == '\r' || *pos == '\n'  )
                    return false;
                if( *pos == ' ' )  //considering message to be request (first token does not contain /, so it looks like METHOD)
                {
                    m_httpMessage = HttpMessage( MessageType::request );
                    m_httpMessage.request->requestLine.parse( data );
                }
                else    //response
                {
                    m_httpMessage = HttpMessage( MessageType::response );
                    m_httpMessage.response->statusLine.parse( data );
                }
                m_state = readingMessageHeaders;
                return true;
            }

            case readingMessageHeaders:
            {
                //parsing header
                if( data.isEmpty() )
                {
                    //empty line
                    //m_state = isMessageBodyFollows() ? readingMessageBody : messageDone;
                    isMessageBodyFollows(); //checking message body parameters in response
                    m_state = readingMessageBody;
                    m_chunkStreamParseState = waitingChunkStart;
                    return true;
                }

                //parsing header
                nx_http::StringType headerName;
                nx_http::StringType headerValue;
                if( !parseHeader( &headerName, &headerValue, data ) )
                    return false;
                m_httpMessage.headers().insert( std::make_pair( headerName, headerValue ) );
                return true;
            }

            default:
                Q_ASSERT( false );
                return false;
        }
    }

    bool HttpStreamReader::isMessageBodyFollows()
    {
        Q_ASSERT( m_httpMessage.type != MessageType::none );

        //analyzing message headers to find out if there should be message body
        //and filling in m_contentLength
        HttpHeaders::const_iterator contentLengthIter = m_httpMessage.headers().find( nx_http::StringType("Content-Length") );
        HttpHeaders::const_iterator transferEncodingIter = m_httpMessage.headers().find( nx_http::StringType("Transfer-Encoding") );
        if( transferEncodingIter != m_httpMessage.headers().end() )
        {
            //parsing Transfer-Encoding
            if( transferEncodingIter->second == "chunked" )
            {
                //ignoring Content-Length header (due to rfc2616, 4.4)
                m_contentLength = 0;
                m_isChunkedTransfer = true;
                return true;
            }
        }

        m_isChunkedTransfer = false;

        if( contentLengthIter == m_httpMessage.headers().end() )
            return false;
        m_contentLength = contentLengthIter->second.toULongLong();
        return m_contentLength > 0;
    }

    size_t HttpStreamReader::readChunkStream( const BufferType& data, const size_t offset, size_t count )
    {
        if( count == BufferNpos )
            count = data.size() - offset;
        const size_t maxOffset = offset + count;

        size_t currentOffset = offset;
        for( ; currentOffset < maxOffset; )
        {
            const BufferType::value_type currentChar = data[(unsigned int)currentOffset];
            switch( m_chunkStreamParseState )
            {
                case waitingChunkStart:
                    m_chunkStreamParseState = readingChunkSize;
                    m_currentChunkSize = 0;
                    m_currentChunkBytesRead = 0;
                    break;

                case readingChunkSize:
                    if( currentChar >= '0' && currentChar <= '9' && 
                        ((currentChar >= 'a' && currentChar <= 'f') || (currentChar >= 'A' && currentChar <= 'F')) )
                    {
                        m_currentChunkSize <<= 4;
                        m_currentChunkSize += hexCharToInt(currentChar);
                    }
                    else if( currentChar == ';' )
                    {
                        m_chunkStreamParseState = readingChunkExtension;
                    }
                    else if( currentChar == ' ' )
                    {
                        //skipping whitespace
                    }
                    else if( currentChar == '\r' || currentChar == '\n' )
                    {
                        //no extension?
                        m_chunkStreamParseState = skippingCRLFBeforeChunkData;
                    }
                    else
                    {
                        //parse error
                        return size_t(-1);
                    }
                    ++currentOffset;
                    break;

                case readingChunkExtension:
                    //TODO/IMPL reading extension
                    if( currentChar == '\r' || currentChar == '\n' )
                        m_chunkStreamParseState = skippingCRLFBeforeChunkData;
                    ++currentOffset;
                    break;

                case skippingCRLFBeforeChunkData:
                    if( m_prevChar == '\r' && currentChar == '\n' ) //supporting CR, LF and CRLF as line-ending, since some buggy rtsp-implementation can do that
                        ++currentOffset;    //skipping
                    else
                        m_chunkStreamParseState = readingChunkData;
                    break;

                case readingChunkData:
                {
                    if( !m_currentChunkSize )
                    {
                        //last chunk
                        m_chunkStreamParseState = skippingCRLFAfterChunkData;
                        break;
                    }

                    const size_t bytesToCopy = std::min<>( m_currentChunkSize - m_currentChunkBytesRead, maxOffset - currentOffset );
                    m_msgBodyBuffer.append( data.data()+currentOffset, (int) bytesToCopy );
                    m_messageBodyBytesRead += bytesToCopy;
                    m_currentChunkBytesRead += bytesToCopy;
                    currentOffset += bytesToCopy;
                    if( m_currentChunkBytesRead == m_currentChunkSize )
                        m_chunkStreamParseState = skippingCRLFAfterChunkData;
                    break;
                }

                case skippingCRLFAfterChunkData:
                {
                    if( m_prevChar == '\r' && currentChar == '\n' ) //supporting CR, LF and CRLF as line-ending, since some buggy rtsp-implementation can do that
                    {
                        ++currentOffset;    //skipping
                        break;
                    }

                    if( !m_currentChunkSize )  //last chunk
                        m_chunkStreamParseState = readingTrailer;
                    else
                        m_chunkStreamParseState = waitingChunkStart;
                    break;
                }

                case readingTrailer:
                {
                    ConstBufferRefType lineBuffer;
                    size_t bytesRead = 0;
                    const bool lineFound = m_lineSplitter.parseByLines(
                        ConstBufferRefType(data, currentOffset, maxOffset-currentOffset),
                        &lineBuffer,
                        &bytesRead );
                    currentOffset += bytesRead;
                    if( !lineFound )
                        break;
                    if( lineBuffer.isEmpty() )
                    {
                        //reached end of HTTP/1.1 chunk stream
                        m_chunkStreamParseState = reachedChunkStreamEnd;
                        return currentOffset - offset;
                    }
                    //TODO/IMPL parsing entity-header
                    break;
                }

                default:
                    Q_ASSERT( false );
                    break;
            }

            m_prevChar = currentChar;
        }

        return currentOffset - offset;
    }

    size_t HttpStreamReader::readIdentityStream( const BufferType& data, size_t offset, size_t count )
    {
        Q_ASSERT( (int)offset < data.size() );

        if( count == BufferNpos )
            count = data.size() - offset;
        const size_t bytesToCopy = m_contentLength > 0
            ? std::min<size_t>( count, m_contentLength-m_messageBodyBytesRead )
            : count;    //Content-Length is unknown
        m_msgBodyBuffer.append( data.data() + offset, (int) bytesToCopy );
        m_messageBodyBytesRead += bytesToCopy;
        return bytesToCopy;
    }

    unsigned int HttpStreamReader::hexCharToInt( BufferType::value_type ch )
    {
        if( ch >= '0' && ch <= '9' )
            return ch - '0';
        if( ch >= 'a' && ch <= 'f' )
            return ch - 'a' + 10;
        if( ch >= 'A' && ch <= 'F' )
            return ch - 'A' + 10;
        return 0;
    }
}
