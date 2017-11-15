#include "http_stream_reader.h"

#include <algorithm>

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/gzip/gzip_uncompressor.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/cpp14.h>

namespace nx_http {

HttpStreamReader::HttpStreamReader():
    m_state(waitingMessageStart),
    m_nextState(waitingMessageStart),
    m_isChunkedTransfer(false),
    m_messageBodyBytesRead(0),
    m_chunkStreamParseState(waitingChunkStart),
    m_nextChunkStreamParseState(undefined),
    m_currentChunkSize(0),
    m_currentChunkBytesRead(0),
    m_prevChar(0),
    m_lineEndingOffset(0),
    m_decodeChunked(true),
    m_currentMessageNumber(0),
    m_breakAfterReadingHeaders(false)
{
}

bool HttpStreamReader::parseBytes(
    const BufferType& data,
    size_t count,
    size_t* bytesProcessed)
{
    return parseBytes(
        QnByteArrayConstRef(data, 0, count),
        bytesProcessed);
}

bool HttpStreamReader::parseBytes(
    const QnByteArrayConstRef& data,
    size_t* bytesProcessed)
{
    if (bytesProcessed)
        *bytesProcessed = 0;
    const size_t count = data.size();

    // Reading line-by-line.
    // TODO #ak automate bytesProcessed modification based on currentDataPos.
    for (size_t currentDataPos = 0; currentDataPos < count; )
    {
        switch (m_state)
        {
            case pullingLineEndingBeforeMessageBody:
            {
                // Reading line ending of the previous line.
                // This is needed to handle rtsp servers which are allowed to use CRLF and CR or LF alone.
                // So, we allow HTTP headers to be terminated with CR or LF. Thus, if CRLF has been split to
                //   different buffers, we MUST read LF before reading message body.
                size_t bytesRead = 0;
                m_lineSplitter.finishCurrentLineEnding(
                    data.mid(currentDataPos, count - currentDataPos),
                    &bytesRead);
                currentDataPos += bytesRead;
                if (bytesProcessed)
                    *bytesProcessed = currentDataPos;
                m_state = m_nextState;
                if (m_breakAfterReadingHeaders ||
                    m_state == messageDone)    //< MUST break parsing on message boundary so that calling entity has a chance to handle message.
                {
                    return true;
                }
                break;
            }

            case readingMessageBody:
            {
                // If m_contentLength == 0 and m_state == readingMessageBody then content-length is unknown.
                size_t msgBodyBytesRead = 0;
                if (m_contentDecoder)
                {
                    msgBodyBytesRead = readMessageBody(
                        data.mid(currentDataPos, count - currentDataPos),
                        [this](const QnByteArrayConstRef& data)
                        {
                            m_codedMessageBodyBuffer.append(data.constData(), data.size());
                        });
                    // Decoding content.
                    if ((msgBodyBytesRead != (size_t)-1) && (msgBodyBytesRead > 0))
                    {
                        if (!m_codedMessageBodyBuffer.isEmpty())
                        {
                            if (!m_contentDecoder->processData(m_codedMessageBodyBuffer))  //< Result of processing is appended to m_msgBodyBuffer.
                                return false;
                            m_codedMessageBodyBuffer.clear();
                        }
                    }
                }
                else
                {
                    msgBodyBytesRead = readMessageBody(
                        data.mid(currentDataPos, count - currentDataPos),
                        [this](const QnByteArrayConstRef& data)
                        {
                            QnMutexLocker lk(&m_mutex);
                            m_msgBodyBuffer.append(data.constData(), data.size());
                        });
                }

                if (msgBodyBytesRead == (size_t)-1)
                    return false;
                currentDataPos += msgBodyBytesRead;
                if (bytesProcessed)
                    *bytesProcessed = currentDataPos;
                if ((m_contentLength &&         //< Content-Length known.
                    m_messageBodyBytesRead >= m_contentLength.get()) ||     //< Read whole message body.
                    (m_isChunkedTransfer && m_chunkStreamParseState == reachedChunkStreamEnd))
                {
                    if (m_contentDecoder)
                        m_contentDecoder->flush();
                    m_state = messageDone;
                    return true;
                }
                break;
            }

            case messageDone:
            {
                // Starting next message.
                resetStateInternal();
                ++m_currentMessageNumber;
                continue;
            }

            default:
            {
                ConstBufferRefType lineBuffer;
                size_t bytesRead = 0;
                const bool lineFound = m_lineSplitter.parseByLines(
                    data.mid(currentDataPos, count - currentDataPos),
                    &lineBuffer,
                    &bytesRead);
                currentDataPos += bytesRead;
                if (bytesProcessed)
                    *bytesProcessed = currentDataPos;
                if (!lineFound)
                    break;
                if (!parseLine(lineBuffer))
                {
                    m_state = parseError;
                    return false;
                }
                // We can move to messageDone state in parseLine function if no message body present.
                if (m_state == messageDone)
                    return true;    //< MUST break parsing on message boundary so that calling entity has chance to handle message.
                break;
            }
        }
    }

    return true;
}

const Message& HttpStreamReader::message() const
{
    return m_httpMessage;
}

Message HttpStreamReader::takeMessage()
{
    return std::move(m_httpMessage);
}

HttpStreamReader::ReadState HttpStreamReader::state() const
{
    return m_state;
}

quint64 HttpStreamReader::messageBodyBytesRead() const
{
    QnMutexLocker lk(&m_mutex);
    return m_messageBodyBytesRead;
}

size_t HttpStreamReader::messageBodyBufferSize() const
{
    QnMutexLocker lk(&m_mutex);
    return m_msgBodyBuffer.size();
}

BufferType HttpStreamReader::fetchMessageBody()
{
    QnMutexLocker lk(&m_mutex);
    BufferType result;
    m_msgBodyBuffer.swap(result);
    return result;
}

QString HttpStreamReader::errorText() const
{
    // TODO: #ak
    return QString();
}

void HttpStreamReader::setState(ReadState state)
{
    m_state = state;
}

void HttpStreamReader::resetState()
{
    resetStateInternal();
    m_currentMessageNumber = 0;
}

void HttpStreamReader::flush()
{
    if (m_contentDecoder)
        m_contentDecoder->flush();
}

void HttpStreamReader::forceEndOfMsgBody()
{
    NX_ASSERT(m_state == readingMessageBody);
    m_state = messageDone;
}

void HttpStreamReader::setDecodeChunkedMessageBody(bool val)
{
    m_decodeChunked = val;
}

int HttpStreamReader::currentMessageNumber() const
{
    return m_currentMessageNumber;
}

boost::optional<quint64> HttpStreamReader::contentLength() const
{
    return m_contentLength;
}

void HttpStreamReader::setBreakAfterReadingHeaders(bool val)
{
    m_breakAfterReadingHeaders = val;
}

bool HttpStreamReader::parseLine(const ConstBufferRefType& data)
{
    for (;;)
    {
        switch (m_state)
        {
            case messageDone:
            case parseError:
                // Starting parsing next message.
                resetStateInternal();
                continue;

            case waitingMessageStart:
            {
                if (data.isEmpty())
                    return true; //skipping empty lines
                                //deciding what we parsing: request line or status line
                                //splitting to tokens separated by ' '
                                //if first token contains '/' then considering line to be status line, else it is request line
                                //const size_t pos = data.find_first_of( " /\r\n", offset, sizeof(" /\r\n")-1 );
                static const char separators[] = " /\r\n";
                const char* pos = std::find_first_of(data.data(), data.data() + data.size(), separators, separators + sizeof(separators) - 1);
                if (pos == data.data() + data.size() || *pos == '\r' || *pos == '\n')
                    return false;
                if (*pos == ' ')  //< Considering message to be request (first token does not contain /, so it looks like METHOD).
                {
                    m_httpMessage = Message(MessageType::request);
                    m_httpMessage.request->requestLine.parse(data);
                }
                else    //< Response.
                {
                    m_httpMessage = Message(MessageType::response);
                    m_httpMessage.response->statusLine.parse(data);
                }
                m_state = readingMessageHeaders;
                return true;
            }

            case readingMessageHeaders:
            {
                // Parsing header.
                if (data.isEmpty())    //< Empty line received: assuming all headers read.
                {
                    if (prepareToReadMessageBody()) //< Checking message body parameters in response.
                    {
                        // TODO #ak reliably check that message body is expected
                        //   in any case allowing to read message body because it could be present
                        //   anyway (e.g., some bug on custom http server).
                        if (m_contentLength && m_contentLength.get() == 0)
                        {
                            // Server purposefully reported empty message body.
                            m_state = m_lineSplitter.currentLineEndingClosed()
                                ? messageDone
                                : pullingLineEndingBeforeMessageBody;
                            m_nextState = messageDone;
                        }
                        else
                        {
                            m_state = pullingLineEndingBeforeMessageBody;
                            m_nextState = readingMessageBody;
                            m_chunkStreamParseState = waitingChunkStart;
                        }
                    }
                    else
                    {
                        // Cannot read (decode) message body.
                        m_state = parseError;
                    }
                    return true;
                }

                // Parsing header.
                nx_http::StringType headerName;
                nx_http::StringType headerValue;
                if (!parseHeader(&headerName, &headerValue, data))
                    return false;
                m_httpMessage.headers().insert(std::make_pair(headerName, headerValue));
                return true;
            }

            default:
                NX_ASSERT(false);
                return false;
        }
    }
}

bool HttpStreamReader::prepareToReadMessageBody()
{
    NX_ASSERT(m_httpMessage.type != MessageType::none);

    if (m_httpMessage.type == MessageType::request)
    {
        if (m_httpMessage.request->requestLine.method != nx_http::Method::post &&
            m_httpMessage.request->requestLine.method != nx_http::Method::put)
        {
            // Only POST and PUT are allowed to have message body.
            m_contentLength = 0;
            return true;
        }
    }
    else if (m_httpMessage.type == MessageType::response)
    {
        if (!StatusCode::isMessageBodyAllowed(m_httpMessage.response->statusLine.statusCode))
        {
            m_contentLength = 0;
            return true;
        }
    }

    m_contentDecoder.reset();
    HttpHeaders::const_iterator contentEncodingIter =
        m_httpMessage.headers().find(nx_http::StringType("Content-Encoding"));
    if (contentEncodingIter != m_httpMessage.headers().end() &&
        contentEncodingIter->second != "identity")
    {
        auto contentDecoder = createContentDecoder(contentEncodingIter->second);
        if (contentDecoder == nullptr)
            return false;   //< Cannot decode message body.
                            // All operations with m_msgBodyBuffer MUST be done with m_mutex locked.
        auto safeAppendToBufferLambda = [this](const QnByteArrayConstRef& data)
        {
            QnMutexLocker lk(&m_mutex);
            m_msgBodyBuffer.append(data.constData(), data.size());
        };
        contentDecoder->setNextFilter(
            nx::utils::bstream::makeCustomOutputStream(std::move(safeAppendToBufferLambda)));
        m_contentDecoder = std::move(contentDecoder);
    }

    // Analyzing message headers to find out if there should be message body
    //   and filling in m_contentLength.
    HttpHeaders::const_iterator transferEncodingIter =
        m_httpMessage.headers().find(nx_http::StringType("Transfer-Encoding"));
    if (transferEncodingIter != m_httpMessage.headers().end())
    {
        // Parsing Transfer-Encoding.
        if (transferEncodingIter->second == "chunked")
        {
            // Ignoring Content-Length header (due to rfc2616, 4.4).
            m_contentLength.reset();
            m_isChunkedTransfer = true;
            return true;
        }
    }

    m_isChunkedTransfer = false;

    HttpHeaders::const_iterator contentLengthIter =
        m_httpMessage.headers().find(nx_http::StringType("Content-Length"));
    if (contentLengthIter != m_httpMessage.headers().end())
        m_contentLength = contentLengthIter->second.toULongLong();

    // TODO: #ak Not sure whether following condition is true with rfc.
    auto connectionHeaderIter = m_httpMessage.headers().find("Connection");
    auto contentTypeHeaderIter = m_httpMessage.headers().find("Content-Type");

    if (!m_contentLength &&
        contentTypeHeaderIter == m_httpMessage.headers().end() &&
        connectionHeaderIter != m_httpMessage.headers().end() &&
        connectionHeaderIter->second.toLower() == "upgrade")
    {
        m_contentLength = 0;
    }

    return true;
}

template<class Func>
size_t HttpStreamReader::readMessageBody(
    const QnByteArrayConstRef& data,
    Func func)
{
    return (m_isChunkedTransfer && m_decodeChunked)
        ? readChunkStream(data, func)
        : readIdentityStream(data, func);
}

template<class Func>
size_t HttpStreamReader::readChunkStream(
    const QnByteArrayConstRef& data,
    Func func)
{
    size_t currentOffset = 0;
    for (; currentOffset < data.size(); )
    {
        const size_t currentOffsetBak = currentOffset;

        const BufferType::value_type currentChar = data[(unsigned int)currentOffset];
        switch (m_chunkStreamParseState)
        {
            case waitingChunkStart:
                m_chunkStreamParseState = readingChunkSize;
                m_currentChunkSize = 0;
                m_currentChunkBytesRead = 0;
                break;

            case readingChunkSize:
                if ((currentChar >= '0' && currentChar <= '9') ||
                    (currentChar >= 'a' && currentChar <= 'f') ||
                    (currentChar >= 'A' && currentChar <= 'F'))
                {
                    m_currentChunkSize <<= 4;
                    m_currentChunkSize += hexCharToInt(currentChar);
                }
                else if (currentChar == ';')
                {
                    m_chunkStreamParseState = readingChunkExtension;
                }
                else if (currentChar == ' ')
                {
                    // Skipping whitespace.
                }
                else if (currentChar == '\r' || currentChar == '\n')
                {
                    // No extension?
                    m_chunkStreamParseState = skippingCRLF;
                    m_nextChunkStreamParseState = readingChunkData;
                    break;
                }
                else
                {
                    // Parse error.
                    return size_t(-1);
                }
                ++currentOffset;
                break;

            case readingChunkExtension:
                // TODO: #ak Reading extension.
                if (currentChar == '\r' || currentChar == '\n')
                {
                    m_chunkStreamParseState = skippingCRLF;
                    m_nextChunkStreamParseState = readingChunkData;
                    break;
                }
                ++currentOffset;
                break;

            case skippingCRLF:
                // Supporting CR, LF and CRLF as line-ending, since some buggy rtsp-implementation can do that.
                if ((m_lineEndingOffset < 2) &&
                    ((currentChar == '\r' && m_lineEndingOffset == 0) ||
                    (currentChar == '\n' && (m_lineEndingOffset == 0 || m_prevChar == '\r'))))
                {
                    ++m_lineEndingOffset;
                    ++currentOffset;    //< Skipping.
                    break;
                }
                else
                {
                    m_lineEndingOffset = 0;
                }

                m_chunkStreamParseState = m_nextChunkStreamParseState;
                break;

            case readingChunkData:
            {
                if (!m_currentChunkSize)
                {
                    // The last chunk.
                    m_chunkStreamParseState = readingTrailer;
                    break;
                }

                const size_t bytesToCopy = std::min<>(
                    m_currentChunkSize - m_currentChunkBytesRead,
                    data.size() - currentOffset);
                func(data.mid(currentOffset, bytesToCopy));
                m_messageBodyBytesRead += bytesToCopy;
                m_currentChunkBytesRead += bytesToCopy;
                currentOffset += bytesToCopy;
                if (m_currentChunkBytesRead == m_currentChunkSize)
                {
                    m_chunkStreamParseState = skippingCRLF;
                    m_nextChunkStreamParseState = waitingChunkStart;
                }
                break;
            }

            case readingTrailer:
            {
                ConstBufferRefType lineBuffer;
                size_t bytesRead = 0;
                const bool lineFound = m_lineSplitter.parseByLines(
                    data.mid(currentOffset),
                    &lineBuffer,
                    &bytesRead);
                currentOffset += bytesRead;
                if (!lineFound)
                    break;
                if (lineBuffer.isEmpty())
                {
                    // Reached end of HTTP/1.1 chunk stream.
                    m_chunkStreamParseState = reachedChunkStreamEnd;
                    return currentOffset;
                }
                // TODO #ak Parsing entity-header.
                break;
            }

            default:
                NX_ASSERT(false);
                break;
        }

        if (currentOffset != currentOffsetBak) //< Moved cursor.
            m_prevChar = currentChar;
    }

    return currentOffset;
}

template<class Func>
size_t HttpStreamReader::readIdentityStream(
    const QnByteArrayConstRef& data,
    Func func)
{
    const size_t bytesToCopy = m_contentLength
        ? std::min<size_t>(data.size(), m_contentLength.get() - m_messageBodyBytesRead)
        : data.size();    //< Content-Length is unknown.
    func(data.mid(0, bytesToCopy));
    m_messageBodyBytesRead += bytesToCopy;
    return bytesToCopy;
}

unsigned int HttpStreamReader::hexCharToInt(BufferType::value_type ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return 0;
}

std::unique_ptr<nx::utils::bstream::AbstractByteStreamFilter>
    HttpStreamReader::createContentDecoder(
        const nx_http::StringType& encodingName)
{
    if (encodingName == "gzip")
        return std::make_unique<nx::utils::bstream::gzip::Uncompressor>();
    return nullptr;
}

void HttpStreamReader::resetStateInternal()
{
    m_state = waitingMessageStart;
    m_httpMessage.clear();
    m_lineSplitter.reset();
    m_contentLength.reset();
    m_isChunkedTransfer = false;
    m_messageBodyBytesRead = 0;
    {
        QnMutexLocker lk(&m_mutex);
        m_msgBodyBuffer.clear();
    }

    m_chunkStreamParseState = waitingChunkStart;
    m_currentChunkSize = 0;
    m_currentChunkBytesRead = 0;
    m_prevChar = 0;
}

} // namespace nx_http
