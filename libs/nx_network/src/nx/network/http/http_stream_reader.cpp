// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_stream_reader.h"

#include <algorithm>

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/gzip/gzip_uncompressor.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::http {

bool HttpStreamReader::parseBytes(
    const ConstBufferRefType& data,
    size_t* bytesProcessed)
{
    if (bytesProcessed)
        *bytesProcessed = 0;
    const size_t count = data.size();

    // Reading line-by-line.
    // TODO #akolesnikov automate bytesProcessed modification based on currentDataPos.
    for (size_t currentDataPos = 0; currentDataPos < count; )
    {
        switch (m_state)
        {
            case ReadState::pullingLineEndingBeforeMessageBody:
            {
                // Reading line ending of the previous line.
                // This is needed to handle rtsp servers which are allowed to use CRLF and CR or LF alone.
                // So, we allow HTTP headers to be terminated with CR or LF. Thus, if CRLF has been split to
                //   different buffers, we MUST read LF before reading message body.
                size_t bytesRead = 0;
                m_lineSplitter.finishCurrentLineEnding(
                    data.substr(currentDataPos, count - currentDataPos),
                    &bytesRead);
                currentDataPos += bytesRead;
                if (bytesProcessed)
                    *bytesProcessed = currentDataPos;
                m_state = m_nextState;
                if (m_breakAfterReadingHeaders ||
                    m_state == ReadState::messageDone)    //< MUST break parsing on message boundary so that calling entity has a chance to handle message.
                {
                    return true;
                }
                break;
            }

            case ReadState::readingMessageBody:
            {
                // If m_contentLength == 0 and m_state == readingMessageBody then content-length is unknown.
                size_t msgBodyBytesRead = 0;
                if (m_contentDecoder)
                {
                    msgBodyBytesRead = readMessageBody(
                        data.substr(currentDataPos, count - currentDataPos),
                        [this](const ConstBufferRefType& data)
                        {
                            m_codedMessageBodyBuffer.append(data);
                            m_messageBodyBytesRead += data.size();
                        });
                    // Decoding content.
                    if ((msgBodyBytesRead != (size_t)-1) && (msgBodyBytesRead > 0))
                    {
                        if (!m_codedMessageBodyBuffer.empty())
                        {
                            // NOTE: The result of the processing is appended to m_msgBodyBuffer.
                            if (!m_contentDecoder->processData(m_codedMessageBodyBuffer))
                                return false;
                            m_codedMessageBodyBuffer.clear();
                        }
                    }
                }
                else
                {
                    msgBodyBytesRead = readMessageBody(
                        data.substr(currentDataPos, count - currentDataPos),
                        [this](const ConstBufferRefType& data)
                        {
                            NX_MUTEX_LOCKER lk(&m_mutex);
                            m_msgBodyBuffer.append(data);
                            m_messageBodyBytesRead += data.size();
                        });
                }

                if (msgBodyBytesRead == (size_t) -1)
                    return false;

                currentDataPos += msgBodyBytesRead;
                if (bytesProcessed)
                    *bytesProcessed = currentDataPos;
                if ((m_contentLength &&         //< Content-Length known.
                    m_messageBodyBytesRead >= *m_contentLength) ||     //< Read whole message body.
                    (m_isChunkedTransfer && m_chunkedStreamParser.eof()))
                {
                    if (m_contentDecoder)
                        m_contentDecoder->flush();
                    m_state = ReadState::messageDone;
                    return true;
                }
                break;
            }

            case ReadState::messageDone:
            case ReadState::parseError:
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
                    data.substr(currentDataPos, count - currentDataPos),
                    &lineBuffer,
                    &bytesRead);
                currentDataPos += bytesRead;
                if (bytesProcessed)
                    *bytesProcessed = currentDataPos;
                if (!lineFound)
                    break;
                if (!parseLine(lineBuffer))
                {
                    m_state = ReadState::parseError;
                    return false;
                }

                // We can move to messageDone state in parseLine function if no message body present.
                if (m_state == ReadState::messageDone ||
                    (m_state == ReadState::readingMessageBody && m_breakAfterReadingHeaders))
                {
                    // MUST break parsing on message boundary so that calling entity has chance
                    // to handle message.
                    return true;
                }
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

std::uint64_t HttpStreamReader::messageBodyBytesRead() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_messageBodyBytesRead;
}

size_t HttpStreamReader::messageBodyBufferSize() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_msgBodyBuffer.size();
}

Buffer HttpStreamReader::fetchMessageBody()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return std::exchange(m_msgBodyBuffer, {});
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
    NX_ASSERT(m_state == ReadState::readingMessageBody);
    m_state = ReadState::messageDone;
}

void HttpStreamReader::setDecodeChunkedMessageBody(bool val)
{
    m_decodeChunked = val;
}

int HttpStreamReader::currentMessageNumber() const
{
    return m_currentMessageNumber;
}

std::optional<std::uint64_t> HttpStreamReader::contentLength() const
{
    return m_contentLength;
}

void HttpStreamReader::setBreakAfterReadingHeaders(bool val)
{
    m_breakAfterReadingHeaders = val;
}

void HttpStreamReader::setParseHeadersStrict(bool enabled)
{
    m_parseHeadersStrict = enabled;
}

bool HttpStreamReader::isEncodingSupported(const std::string_view& encoding)
{
    return encoding == "gzip"
        || encoding == "deflate"
        || encoding == "identity";
}

bool HttpStreamReader::parseLine(const ConstBufferRefType& data)
{
    for (;;)
    {
        switch (m_state)
        {
            case ReadState::messageDone:
            case ReadState::parseError:
                // Starting parsing next message.
                resetStateInternal();
                continue;

            case ReadState::waitingMessageStart:
            {
                if (data.empty())
                    return true; //< Skipping empty lines.

                // Deciding what we parsing: request line or status line.
                // Splitting to tokens separated by ' '.
                // If first token contains '/' then considering line to be status line, else it is request line
                // const size_t pos = data.find_first_of( " /\r\n", offset, sizeof(" /\r\n")-1 );
                static const char separators[] = " /\r\n";
                const auto pos = data.find_first_of(separators);
                if (pos == data.npos || data[pos] == '\r' || data[pos] == '\n')
                    return false;
                if (data[pos] == ' ')  //< Considering message to be request (first token does not contain /, so it looks like METHOD).
                {
                    m_httpMessage = Message(MessageType::request);
                    if (!m_httpMessage.request->requestLine.parse(data))
                        NX_DEBUG(this, "Error parsing '%1' as request line", data);
                }
                else    //< Response.
                {
                    m_httpMessage = Message(MessageType::response);
                    if (!m_httpMessage.response->statusLine.parse(data))
                        NX_DEBUG(this, "Error parsing '%1' as status line", data);
                }
                m_state = ReadState::readingMessageHeaders;
                return true;
            }

            case ReadState::readingMessageHeaders:
            {
                // Parsing header.
                if (data.empty())    //< Empty line received: assuming all headers read.
                {
                    if (prepareToReadMessageBody()) //< Checking message body parameters in response.
                    {
                        // TODO #akolesnikov reliably check that message body is expected
                        //   in any case allowing to read message body because it could be present
                        //   anyway (e.g., some bug on custom http server).
                        if (m_contentLength && *m_contentLength == 0)
                        {
                            // Server purposefully reported empty message body.
                            m_state = m_lineSplitter.currentLineEndingClosed()
                                ? ReadState::messageDone
                                : ReadState::pullingLineEndingBeforeMessageBody;
                            m_nextState = ReadState::messageDone;
                        }
                        else
                        {
                            m_state = m_lineSplitter.currentLineEndingClosed()
                                ? ReadState::readingMessageBody
                                : ReadState::pullingLineEndingBeforeMessageBody;
                            m_nextState = ReadState::readingMessageBody;

                            m_chunkedStreamParser.reset();
                        }
                    }
                    else
                    {
                        // Cannot read (decode) message body.
                        m_state = ReadState::parseError;
                        NX_WARNING(this, "Failed to prepare for reading HTTP message body");
                    }
                    return true;
                }

                // Parsing header.
                std::string headerName;
                std::string headerValue;
                if (!parseHeader(data, &headerName, &headerValue))
                {
                    if (m_parseHeadersStrict)
                        return false;
                    // Since we consider this data valid, proxy without dropping the data.
                    headerName = "";
                    headerValue = data;
                }
                m_httpMessage.headers().emplace(std::move(headerName), std::move(headerValue));
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

    m_contentDecoder.reset();
    auto contentEncodingIter = m_httpMessage.headers().find("Content-Encoding");
    if (contentEncodingIter != m_httpMessage.headers().end() &&
        !contentEncodingIter->second.empty() && //< Buggy servers (AWS S3) may send the header with no value.
        contentEncodingIter->second != "identity")
    {
        auto contentDecoder = createContentDecoder(contentEncodingIter->second);
        if (contentDecoder == nullptr)
            return false;   //< Cannot decode message body.
                            // All operations with m_msgBodyBuffer MUST be done with m_mutex locked.
        auto safeAppendToBufferLambda = [this](const ConstBufferRefType& data)
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            m_msgBodyBuffer.append(data);
        };
        contentDecoder->setNextFilter(
            nx::utils::bstream::makeCustomOutputStream(std::move(safeAppendToBufferLambda)));
        m_contentDecoder = std::move(contentDecoder);
    }

    // Analyzing message headers to find out if there should be message body
    //   and filling in m_contentLength.
    auto transferEncodingIter = m_httpMessage.headers().find("Transfer-Encoding");
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

    const auto contentLengthIter = m_httpMessage.headers().find("Content-Length");
    if (contentLengthIter != m_httpMessage.headers().end())
        m_contentLength = nx::utils::stoull(contentLengthIter->second);
    else
        checkIfMessageBodyIsAppropriateByDefault();

    return true;
}

void HttpStreamReader::checkIfMessageBodyIsAppropriateByDefault()
{
    auto connectionHeaderIter = m_httpMessage.headers().find("Connection");
    auto contentTypeHeaderIter = m_httpMessage.headers().find("Content-Type");

    // Assuming that "upgrade connection" requests do not have message body unless specified
    // explicitly.
    // TODO: #akolesnikov Not sure whether the following condition is conformant to the RFC.
    if (!m_contentLength &&
        contentTypeHeaderIter == m_httpMessage.headers().end() &&
        connectionHeaderIter != m_httpMessage.headers().end() &&
        nx::utils::stricmp(connectionHeaderIter->second, "upgrade") == 0)
    {
        m_contentLength = 0;
        return;
    }
    // All requests except POST, PATCH and PUT do not contain message body unless specified
    // explicitly with "Content-..." headers.
    if (m_httpMessage.type == MessageType::request)
    {
        if (m_httpMessage.request->requestLine.method != nx::network::http::Method::post &&
            m_httpMessage.request->requestLine.method != nx::network::http::Method::patch &&
            m_httpMessage.request->requestLine.method != nx::network::http::Method::put)
        {
            m_contentLength = 0;
            return;
        }
    }
    else if (m_httpMessage.type == MessageType::response)
    {
        if (!StatusCode::isMessageBodyAllowed(m_httpMessage.response->statusLine.statusCode))
        {
            m_contentLength = 0;
            return;
        }
    }
}

template<class Func>
size_t HttpStreamReader::readMessageBody(
    const ConstBufferRefType& data,
    Func func)
{
    return (m_isChunkedTransfer && m_decodeChunked)
        ? m_chunkedStreamParser.parse(data, func)
        : readIdentityStream(data, func);
}

template<class Func>
size_t HttpStreamReader::readIdentityStream(
    const ConstBufferRefType& data,
    Func func)
{
    const size_t bytesToCopy = m_contentLength
        ? std::min<size_t>(data.size(), *m_contentLength - m_messageBodyBytesRead)
        : data.size();    //< Content-Length is unknown.
    func(data.substr(0, bytesToCopy));
    return bytesToCopy;
}

std::unique_ptr<nx::utils::bstream::AbstractByteStreamFilter>
    HttpStreamReader::createContentDecoder(const std::string& encodingName)
{
    if (encodingName == "gzip" || encodingName == "deflate")
        return std::make_unique<nx::utils::bstream::gzip::Uncompressor>();
    return nullptr;
}

void HttpStreamReader::resetStateInternal()
{
    m_state = ReadState::waitingMessageStart;
    m_httpMessage.clear();
    m_lineSplitter.reset();
    m_contentLength.reset();
    m_isChunkedTransfer = false;
    m_messageBodyBytesRead = 0;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_msgBodyBuffer.clear();
    }

    m_chunkedStreamParser.reset();
}

//-------------------------------------------------------------------------------------------------

const char* toString(HttpStreamReader::ReadState value)
{
    switch (value)
    {
        case HttpStreamReader::ReadState::waitingMessageStart:
            return "waitingMessageStart";
        case HttpStreamReader::ReadState::readingMessageHeaders:
            return "readingMessageHeaders";
        case HttpStreamReader::ReadState::messageDone:
            return "messageDone";
        case HttpStreamReader::ReadState::parseError:
            return "parseError";
        case HttpStreamReader::ReadState::pullingLineEndingBeforeMessageBody:
            return "pullingLineEndingBeforeMessageBody";
        case HttpStreamReader::ReadState::readingMessageBody:
            return "readingMessageBody";
    }

    return "unknown";
}

} // namespace nx::network::http
