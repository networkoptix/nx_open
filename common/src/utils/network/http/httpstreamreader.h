/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPSTREAMREADER_H
#define HTTPSTREAMREADER_H

#include <mutex>

#include "httptypes.h"
#include "linesplitter.h"

#include "utils/media/abstract_byte_stream_filter.h"


namespace nx_http
{
    //!Parses stream of bytes as http message
    /*!
        Can handle multiple subsequent messages
        \note Supports chunked stream
        \note Thread safety: only message body buffer - related functionality is thread-safe (required by \a AsyncHttpClient class). All other methods are NOT thread-safe
        \note Assumes that any message is followed by message body
        \note If message body encoding is unknown, assumes identity. If Content-Length is unknown, assumes 
            infinite content-length (even when identity encoding is used)
    */
    class HttpStreamReader
    {
    public:
        enum ReadState
        {
            waitingMessageStart,
            readingMessageHeaders,
            messageDone,
            parseError,
            readingMessageBody
        };

        HttpStreamReader();
        virtual ~HttpStreamReader();

        //!Parses \a count bytes from source buffer \a data as HTTP
        /*!
            \param bytesProcessed if not NULL, \a *bytesProcessed is set to number of bytes read from \a data
            \return true on success. false on parse error, call \a errorText() to receive error description
        */
        bool parseBytes(
            const BufferType& data,
            size_t count = nx_http::BufferNpos,
            size_t* bytesProcessed = NULL );
        /*!
            \return Actual only after state changed from \a readingMessageHeaders to \a waitingMessageStart or \a readingMessageBody
        */
        const HttpMessage& message() const;
        ReadState state() const;
        size_t messageBodyBufferSize() const;
        //!Returns internal message body buffer and clears internal buffer
        BufferType fetchMessageBody();
        //!Returns error description of previous parse error
        QString errorText() const;
        //!Makes reader ready to parse new message
        void resetState();
        //!Flush all internal buffers (if any), so that all data is available through public API
        void flush();

    private:
        enum ChunkStreamParseState
        {
            waitingChunkStart,
            readingChunkSize,
            readingChunkExtension,
            skippingCRLFBeforeChunkData,
            readingChunkData,
            skippingCRLFAfterChunkData,
            readingTrailer,
            reachedChunkStreamEnd
        };

        ReadState m_state;
        HttpMessage m_httpMessage;
        quint64 m_contentLength;
        bool m_isChunkedTransfer;
        quint64 m_messageBodyBytesRead;
        BufferType m_msgBodyBuffer;

        //!HTTP/1.1 chunk stream parsing
        ChunkStreamParseState m_chunkStreamParseState;
        size_t m_currentChunkSize;
        size_t m_currentChunkBytesRead;
        BufferType::value_type m_prevChar;
        BufferType m_codedMessageBodyBuffer;
        std::unique_ptr<AbstractByteStreamFilter> m_contentDecoder;

        LineSplitter m_lineSplitter;
        mutable std::mutex m_mutex;

        bool parseLine( const ConstBufferRefType& data );
        //!Reads message body parameters from message headers and initializes required data
        /*!
            Sets members \a m_contentLength, \a m_isChunkedTransfer, \a m_contentDecoder
            \return true If message body could be read or no message body is delared in message. false if cannot read message body (e.g., unsupported content encoding)
        */
        bool prepareToReadMessageBody();
        /*!
            \return bytes copied to \a outBuf or -1 in case of error
        */
        size_t readMessageBody(
            const BufferType& data,
            size_t offset,
            size_t count,
            BufferType* const outBuf );
        size_t readChunkStream(
            const BufferType& data,
            size_t offset,
            size_t count,
            BufferType* const outBuf );
        size_t readIdentityStream(
            const BufferType& data,
            size_t offset,
            size_t count,
            BufferType* const outBuf );
        unsigned int hexCharToInt( BufferType::value_type ch );
        //!Returns nullptr if \a encodingName is unknown
        AbstractByteStreamConverter* createContentDecoder( const nx_http::StringType& encodingName );
    };
}

#endif  //HTTPSTREAMREADER_H
