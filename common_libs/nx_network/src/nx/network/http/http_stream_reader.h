#pragma once

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/mutex.h>

#include "http_types.h"
#include "line_splitter.h"

namespace nx_http {

/**
 * Parses stream of bytes as http messages.
 * Can handle multiple subsequent messages.
 * Increases HttpStreamReader::currentMessageNumber() after successfully reading each message.
 * Supports chunked stream.
 * NOTE: Thread safety: only message body buffer - related functionality is thread-safe (required by AsyncHttpClient class).
 *   All other methods are NOT thread-safe.
 * NOTE: Assumes that any message is followed by message body.
 * NOTE: If message body encoding is unknown, assumes identity. If Content-Length is unknown, assumes
 *   infinite content-length (even when identity encoding is used)
 */
class NX_NETWORK_API HttpStreamReader
{
public:
    enum ReadState
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
        readingMessageBody
    };

    HttpStreamReader();
    virtual ~HttpStreamReader() = default;

    /**
     * Parses count bytes from source buffer data as HTTP.
     * @param count Bytes of data to parse.
     * @param bytesProcessed if not NULL, *bytesProcessed is set to number of bytes read from data.
     * @return true on success. false on parse error, call errorText() to receive error description
     */
    bool parseBytes(
        const BufferType& data,
        size_t count = nx::utils::BufferNpos,
        size_t* bytesProcessed = NULL);
    bool parseBytes(
        const QnByteArrayConstRef& data,
        size_t* bytesProcessed = NULL);
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
    quint64 messageBodyBytesRead() const;
    size_t messageBodyBufferSize() const;
    /**
     * Returns internal message body buffer and clears internal buffer.
     */
    BufferType fetchMessageBody();
    /**
     * Returns error description of previous parse error.
     */
    QString errorText() const;
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
    boost::optional<quint64> contentLength() const;

    /** If true, then parseBytes always returns after reading http headers and trailing CRLF have been read. */
    void setBreakAfterReadingHeaders(bool val);

private:
    enum ChunkStreamParseState
    {
        waitingChunkStart,
        readingChunkSize,
        readingChunkExtension,
        skippingCRLF,
        readingChunkData,
        readingTrailer,
        reachedChunkStreamEnd,
        undefined
    };

    ReadState m_state;
    ReadState m_nextState;
    Message m_httpMessage;
    boost::optional<quint64> m_contentLength;
    bool m_isChunkedTransfer;
    quint64 m_messageBodyBytesRead;
    BufferType m_msgBodyBuffer;

    //!HTTP/1.1 chunk stream parsing
    ChunkStreamParseState m_chunkStreamParseState;
    ChunkStreamParseState m_nextChunkStreamParseState;
    size_t m_currentChunkSize;
    size_t m_currentChunkBytesRead;
    BufferType::value_type m_prevChar;
    BufferType m_codedMessageBodyBuffer;
    std::unique_ptr<nx::utils::bstream::AbstractByteStreamFilter> m_contentDecoder;
    int m_lineEndingOffset;
    bool m_decodeChunked;
    int m_currentMessageNumber;
    bool m_breakAfterReadingHeaders;

    LineSplitter m_lineSplitter;
    mutable QnMutex m_mutex;

    bool parseLine(const ConstBufferRefType& data);
    /**
     * Reads message body parameters from message headers and initializes required data.
     * Sets members m_contentLength, m_isChunkedTransfer, a m_contentDecoder.
     * @return true If message body could be read or no message body is delared in message.
     *   false if cannot read message body (e.g., unsupported content encoding)
     */
    bool prepareToReadMessageBody();
    /**
     * @return bytes read from data or -1 in case of error.
     */
    template<class Func>
    size_t readMessageBody(
        const QnByteArrayConstRef& data,
        Func func);
    template<class Func>
    size_t readChunkStream(
        const QnByteArrayConstRef& data,
        Func func);
    template<class Func>
    size_t readIdentityStream(
        const QnByteArrayConstRef& data,
        Func func);
    unsigned int hexCharToInt(BufferType::value_type ch);
    /**
     * Returns nullptr if encodingName is unknown.
     */
    std::unique_ptr<nx::utils::bstream::AbstractByteStreamFilter> createContentDecoder(
        const nx_http::StringType& encodingName);
    void resetStateInternal();
};

} // namespace nx_http
