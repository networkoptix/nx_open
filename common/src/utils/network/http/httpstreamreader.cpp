/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "httpstreamreader.h"

#include <algorithm>
#include <fstream>

#include "utils/gzip/gzip_uncompressor.h"
#include "utils/media/custom_output_stream.h"


namespace nx_http
{
    HttpStreamReader::HttpStreamReader()
    :
        m_state( waitingMessageStart ),
        m_contentLength( 0 ),
        m_isChunkedTransfer( false ),
        m_messageBodyBytesRead( 0 ),
        m_chunkStreamParseState( waitingChunkStart ),
        m_nextState( undefined ),
        m_currentChunkSize( 0 ),
        m_currentChunkBytesRead( 0 ),
        m_prevChar( 0 ),
        m_lineEndingOffset( 0 ),
        m_decodeChunked( true ),
        m_currentMessageNumber( 0 )
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
                case pullingLineEndingBeforeMessageBody:
                {
                    //reading line ending of the previous line
                        //this is needed to handle buggy http/rtsp servers which do not use CRLF, but CR or LF
                        //So, we allow HTTP headers to be terminated with CR of LF. Thus, if CRLF has been split to
                        //different buffers, we MUST read LF before reading message body
                    size_t bytesRead = 0;
                    m_lineSplitter.finishCurrentLineEnding(
                        ConstBufferRefType( data, currentDataPos, count-currentDataPos ),
                        &bytesRead );
                    currentDataPos += bytesRead;
                    m_state = readingMessageBody;
                    break;
                }

                case readingMessageBody:
                {
                    //if m_contentLength == 0 and m_state == readingMessageBody then content-length is unknown
                    size_t msgBodyBytesRead = 0;
                    if( m_contentDecoder )
                    {
                        msgBodyBytesRead = readMessageBody(
                            QnByteArrayConstRef(data, currentDataPos, count-currentDataPos),
                            [this]( const QnByteArrayConstRef& data ){ m_codedMessageBodyBuffer.append(data.constData(), data.size()); } );
                        //decoding content
                        if( (msgBodyBytesRead != (size_t)-1) && (msgBodyBytesRead > 0) )
                        {
                            if( !m_codedMessageBodyBuffer.isEmpty() )
                            {
                                m_contentDecoder->processData( m_codedMessageBodyBuffer );  //result of processing is appended to m_msgBodyBuffer
                                m_codedMessageBodyBuffer.clear();
                            }
                        }
                    }
                    else
                    {
                        msgBodyBytesRead = readMessageBody(
                            QnByteArrayConstRef(data, currentDataPos, count-currentDataPos),
                            [this]( const QnByteArrayConstRef& data )
                                {
                                    std::unique_lock<std::mutex> lk( m_mutex );
                                    m_msgBodyBuffer.append( data.constData(), data.size() );
                                }
                        );
                    }

                    if( msgBodyBytesRead == (size_t)-1 )
                        return false;
                    currentDataPos += msgBodyBytesRead;
                    if( bytesProcessed )
                        *bytesProcessed = currentDataPos;
                    if( (m_contentLength > 0 &&         //Content-Length known
                         m_messageBodyBytesRead >= m_contentLength) ||     //read all message body
                        (m_isChunkedTransfer && m_chunkStreamParseState == reachedChunkStreamEnd) )
                    {
                        if( m_contentDecoder )
                            m_contentDecoder->flush();
                        m_state = messageDone;
                        return true;
                    }
                    break;
                }

                case messageDone:
                {
                    //starting next message
                    resetStateInternal();
                    ++m_currentMessageNumber;
                    continue;
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
    const Message& HttpStreamReader::message() const
    {
        return m_httpMessage;
    }

    HttpStreamReader::ReadState HttpStreamReader::state() const
    {
        return m_state;
    }

    size_t HttpStreamReader::messageBodyBufferSize() const
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        return m_msgBodyBuffer.size();
    }

    //!Returns internal message body buffer and clears internal buffer
    BufferType HttpStreamReader::fetchMessageBody()
    {
        std::unique_lock<std::mutex> lk( m_mutex );
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
        resetStateInternal();
        m_currentMessageNumber = 0;
    }

    void HttpStreamReader::flush()
    {
        if( m_contentDecoder )
            m_contentDecoder->flush();
    }

    /*!
        By default \a true.
        \param val If \a false, chunked message is not decoded and returned as-is by \a AsyncHttpClient::fetchMessageBodyBuffer
    */
    void HttpStreamReader::setDecodeChunkedMessageBody( bool val )
    {
        m_decodeChunked = val;
    }

    int HttpStreamReader::currentMessageNumber() const
    {
        return m_currentMessageNumber;
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
                {
                    std::unique_lock<std::mutex> lk( m_mutex );
                    m_msgBodyBuffer.clear();
                }

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
                    m_httpMessage = Message( MessageType::request );
                    m_httpMessage.request->requestLine.parse( data );
                }
                else    //response
                {
                    m_httpMessage = Message( MessageType::response );
                    m_httpMessage.response->statusLine.parse( data );
                }
                m_state = readingMessageHeaders;
                return true;
            }

            case readingMessageHeaders:
            {
                //parsing header
                if( data.isEmpty() )    //empty line received: assuming all headers read
                {
                    //m_state = isMessageBodyFollows() ? readingMessageBody : messageDone;
                    if( prepareToReadMessageBody() ) //checking message body parameters in response
                    {
                        //in any case allowing to read message body because it could be present 
                            //anyway (e.g., some bug on custom http server)
                        m_state = pullingLineEndingBeforeMessageBody;
                        m_chunkStreamParseState = waitingChunkStart;
                    }
                    else
                    {
                        //cannot read (decode) message body
                        m_state = parseError;
                    }
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

    bool HttpStreamReader::prepareToReadMessageBody()
    {
        Q_ASSERT( m_httpMessage.type != MessageType::none );

        HttpHeaders::const_iterator contentEncodingIter = m_httpMessage.headers().find( nx_http::StringType("Content-Encoding") );
        if( contentEncodingIter != m_httpMessage.headers().end() )
        {
            AbstractByteStreamConverter* contentDecoder = createContentDecoder( contentEncodingIter->second );
            if( contentDecoder == nullptr )
                return false;   //cannot decode message body
            //all operations with m_msgBodyBuffer MUST be done with m_mutex locked
            auto safeAppendToBufferLambda = [this]( const QnByteArrayConstRef& data )
            {
                std::unique_lock<std::mutex> lk( m_mutex );
                m_msgBodyBuffer.append( data.constData(), data.size() );
            };
            contentDecoder->setNextFilter( std::make_shared<CustomOutputStream<decltype(safeAppendToBufferLambda)>>( safeAppendToBufferLambda ) );
            m_contentDecoder.reset( contentDecoder );
        }
        
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

        if( contentLengthIter != m_httpMessage.headers().end() )
            m_contentLength = contentLengthIter->second.toULongLong();
        return true;
    }

    template<class Func>
    size_t HttpStreamReader::readMessageBody(
        const QnByteArrayConstRef& data,
        Func func )
    {
        return (m_isChunkedTransfer && m_decodeChunked)
            ? readChunkStream( data, func )
            : readIdentityStream( data, func );
    }

    template<class Func>
    size_t HttpStreamReader::readChunkStream(
        const QnByteArrayConstRef& data,
        Func func )
    {
        size_t currentOffset = 0;
        for( ; currentOffset < data.size(); )
        {
            const size_t currentOffsetBak = currentOffset;

            const BufferType::value_type currentChar = data[(unsigned int)currentOffset];
            switch( m_chunkStreamParseState )
            {
                case waitingChunkStart:
                    m_chunkStreamParseState = readingChunkSize;
                    m_currentChunkSize = 0;
                    m_currentChunkBytesRead = 0;
                    break;

                case readingChunkSize:
                    if( (currentChar >= '0' && currentChar <= '9') || 
                        (currentChar >= 'a' && currentChar <= 'f') ||
                        (currentChar >= 'A' && currentChar <= 'F') )
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
                        m_chunkStreamParseState = skippingCRLF;
                        m_nextState = readingChunkData;
                        break;
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
                    {
                        m_chunkStreamParseState = skippingCRLF;
                        m_nextState = readingChunkData;
                        break;
                    }
                    ++currentOffset;
                    break;

                case skippingCRLF:
                    //supporting CR, LF and CRLF as line-ending, since some buggy rtsp-implementation can do that
                    if( (m_lineEndingOffset < 2) &&
                        ((currentChar == '\r' && m_lineEndingOffset == 0) ||
                         (currentChar == '\n' && (m_lineEndingOffset == 0 || m_prevChar == '\r'))) )
                    {
                        ++m_lineEndingOffset;
                        ++currentOffset;    //skipping
                        break;
                    }
                    else
                    {
                        m_lineEndingOffset = 0;
                    }

                    m_chunkStreamParseState = m_nextState;
                    break;

                case readingChunkData:
                {
                    if( !m_currentChunkSize )
                    {
                        //last chunk
                        m_chunkStreamParseState = readingTrailer;   //last chunk
                        break;
                    }

                    const size_t bytesToCopy = std::min<>( m_currentChunkSize - m_currentChunkBytesRead, data.size() - currentOffset );
                    func( data.mid(currentOffset, bytesToCopy) );
                    m_messageBodyBytesRead += bytesToCopy;
                    m_currentChunkBytesRead += bytesToCopy;
                    currentOffset += bytesToCopy;
                    if( m_currentChunkBytesRead == m_currentChunkSize )
                    {
                        m_chunkStreamParseState = skippingCRLF;
                        m_nextState = waitingChunkStart;
                    }
                    break;
                }

                case readingTrailer:
                {
                    ConstBufferRefType lineBuffer;
                    size_t bytesRead = 0;
                    const bool lineFound = m_lineSplitter.parseByLines(
                        data.mid( currentOffset ),
                        &lineBuffer,
                        &bytesRead );
                    currentOffset += bytesRead;
                    if( !lineFound )
                        break;
                    if( lineBuffer.isEmpty() )
                    {
                        //reached end of HTTP/1.1 chunk stream
                        m_chunkStreamParseState = reachedChunkStreamEnd;
                        return currentOffset;
                    }
                    //TODO #ak parsing entity-header
                    break;
                }

                default:
                    Q_ASSERT( false );
                    break;
            }

            if( currentOffset != currentOffsetBak ) //moved cursor
                m_prevChar = currentChar;
        }

        return currentOffset;
    }

    template<class Func>
    size_t HttpStreamReader::readIdentityStream(
        const QnByteArrayConstRef& data,
        Func func )
    {
        const size_t bytesToCopy = m_contentLength > 0
            ? std::min<size_t>( data.size(), m_contentLength-m_messageBodyBytesRead )
            : data.size();    //Content-Length is unknown
        func( data.mid(0, bytesToCopy) );
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

    AbstractByteStreamConverter* HttpStreamReader::createContentDecoder( const nx_http::StringType& encodingName )
    {
        if( encodingName == "gzip" )
            return new GZipUncompressor();
        return nullptr;
    }

    void HttpStreamReader::resetStateInternal()
    {
        m_state = waitingMessageStart;
        m_httpMessage.clear();
        m_lineSplitter.reset();
        m_contentLength = 0;
        m_isChunkedTransfer = false;
        m_messageBodyBytesRead = 0;
        {
            std::unique_lock<std::mutex> lk( m_mutex );
            m_msgBodyBuffer.clear();
        }

        m_chunkStreamParseState = waitingChunkStart;
        m_currentChunkSize = 0;
        m_currentChunkBytesRead = 0;
        m_prevChar = 0;
    }
}
