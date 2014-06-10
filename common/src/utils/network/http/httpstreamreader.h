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
        // TODO: #Elric #enum
        enum ReadState
        {
            waitingMessageStart,
            readingMessageHeaders,
            //!Moves to this state after reading whole message body (in case content-length is known or chunk encoding is used)
            messageDone,
            parseError,
            readingMessageBody
        };

        HttpStreamReader();
        virtual ~HttpStreamReader();

        //!Parses \a count bytes from source buffer \a data as HTTP
        /*!
            \param count Bytes of \a data to parse
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
        const Message& message() const;
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
        /*!
            By default \a true.
            \param val If \a false, chunked message is not decoded and returned as-is by \a AsyncHttpClient::fetchMessageBodyBuffer
        */
        void setDecodeChunkedMessageBody( bool val );

    private:
        // TODO: #Elric #enum
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
        Message m_httpMessage;
        quint64 m_contentLength;
        bool m_isChunkedTransfer;
        quint64 m_messageBodyBytesRead;
        BufferType m_msgBodyBuffer;

        //!HTTP/1.1 chunk stream parsing
        ChunkStreamParseState m_chunkStreamParseState;
        ChunkStreamParseState m_nextState;
        size_t m_currentChunkSize;
        size_t m_currentChunkBytesRead;
        BufferType::value_type m_prevChar;
        BufferType m_codedMessageBodyBuffer;
        std::unique_ptr<AbstractByteStreamFilter> m_contentDecoder;
        int m_lineEndingOffset;
        bool m_decodeChunked;

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
            \return bytes read from \a data or -1 in case of error
        */
        template<class Func>
        size_t readMessageBody(
            const QnByteArrayConstRef& data,
            Func func );
        template<class Func>
        size_t readChunkStream(
            const QnByteArrayConstRef& data,
            Func func );
        template<class Func>
        size_t readIdentityStream(
            const QnByteArrayConstRef& data,
            Func func );
        unsigned int hexCharToInt( BufferType::value_type ch );
        //!Returns nullptr if \a encodingName is unknown
        AbstractByteStreamConverter* createContentDecoder( const nx_http::StringType& encodingName );
    };
}

#endif  //HTTPSTREAMREADER_H
