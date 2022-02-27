// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/mutex.h>

#include "chunked_stream_parser.h"
#include "http_types.h"
#include "line_splitter.h"

namespace nx::network::http {

/**
 * Parses stream of bytes as HTTP messages. Able to handle multiple subsequent messages.
 * Increases HttpStreamReader::currentMessageNumber() after successfully reading each message.
 * Supports chunked stream.
 * NOTE: Thread safety: only message body buffer - related functionality is thread-safe
 *   (required by AsyncHttpClient class). All other methods are NOT thread-safe.
 * NOTE: If message body encoding is unknown, assumes identity. If Content-Length is unknown,
 *   assumes infinite content-length (even when identity encoding is used).
 */
class NX_NETWORK_API HttpStreamReader
{
public:
    enum class ReadState
    {
        waitingMessageStart,
        readingMessageHeaders,
        /**
         * Moves to this state after reading whole message body
         * (in case content-length is known or chunk encoding is used).
         */
        messageDone,
        parseError,
        pullingLineEndingBeforeMessageBody,
        readingMessageBody,
    };

    virtual ~HttpStreamReader() = default;

    /**
     * Parses count bytes from source buffer data as HTTP.
     * @param bytesProcessed if not NULL, *bytesProcessed is set to number of bytes read from data.
     * @return true on success. false on parse error, call errorText() to receive error description
     */
    bool parseBytes(
        const ConstBufferRefType& data,
        size_t* bytesProcessed = nullptr);

    /**
     * @return Actual only after state changed from readingMessageHeaders
     * to waitingMessageStart or readingMessageBody.
     */
    const Message& message() const;

    /**
     * Moves message out of parser.
     */
    Message takeMessage();
    ReadState state() const;
    std::uint64_t messageBodyBytesRead() const;
    size_t messageBodyBufferSize() const;

    /**
     * Returns internal message body buffer and clears internal buffer.
     */
    Buffer fetchMessageBody();

    /**
     * Forces stream reader state.
     */
    void setState(ReadState state);

    /**
     * Makes reader ready to parse new message.
     */
    void resetState();

    /**
     * Flush all internal buffers (if any), so that all data is available through public API.
     */
    void flush();

    /**
     * Force HttpStreamReader think that end of message body has been met.
     */
    void forceEndOfMsgBody();

    /**
     * By default true.
     * @param val If false, chunked message is not decoded and returned as-is by AsyncHttpClient::fetchMessageBodyBuffer.
     */
    void setDecodeChunkedMessageBody(bool val);

    /**
     * Returns sequential HTTP message number.
     */
    int currentMessageNumber() const;

    std::optional<std::uint64_t> contentLength() const;

    /** If true, then parseBytes always returns after reading http headers and trailing CRLF have been read. */
    void setBreakAfterReadingHeaders(bool val);

    /** If true, then parseBytes skips invalid HTTP headers instead of failing. */
    void setParseHeadersStrict(bool enabled);

    static bool isEncodingSupported(const std::string_view& encoding);

private:
    ReadState m_state = ReadState::waitingMessageStart;
    ReadState m_nextState = ReadState::waitingMessageStart;
    Message m_httpMessage;
    std::optional<std::uint64_t> m_contentLength;
    bool m_isChunkedTransfer = false;
    std::uint64_t m_messageBodyBytesRead = 0;
    nx::Buffer m_msgBodyBuffer;

    ChunkedStreamParser m_chunkedStreamParser;

    nx::Buffer m_codedMessageBodyBuffer;
    std::unique_ptr<nx::utils::bstream::AbstractByteStreamFilter> m_contentDecoder;
    bool m_decodeChunked = true;
    int m_currentMessageNumber = 0;
    bool m_breakAfterReadingHeaders = false;
    bool m_parseHeadersStrict = true;

    LineSplitter m_lineSplitter;
    mutable nx::Mutex m_mutex;

    bool parseLine(const ConstBufferRefType& data);

    /**
     * Reads message body parameters from message headers and initializes required data.
     * Sets members m_contentLength, m_isChunkedTransfer, a m_contentDecoder.
     * @return true If message body could be read or no message body is delared in message.
     *   false if cannot read message body (e.g., unsupported content encoding)
     */
    bool prepareToReadMessageBody();

    void checkIfMessageBodyIsAppropriateByDefault();

    /**
     * @return bytes read from data or -1 in case of error.
     */
    template<class Func>
    size_t readMessageBody(
        const ConstBufferRefType& data,
        Func func);

    template<class Func>
    size_t readChunkStream(
        const ConstBufferRefType& data,
        Func func);

    template<class Func>
    size_t readIdentityStream(
        const ConstBufferRefType& data,
        Func func);

    /**
     * @return nullptr if encodingName is unknown.
     */
    std::unique_ptr<nx::utils::bstream::AbstractByteStreamFilter> createContentDecoder(
        const std::string& encodingName);

    void resetStateInternal();
};

NX_NETWORK_API const char* toString(HttpStreamReader::ReadState value);

} // namespace nx::network::http
