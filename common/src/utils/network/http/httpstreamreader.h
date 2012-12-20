/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPSTREAMREADER_H
#define HTTPSTREAMREADER_H

#include "httptypes.h"
#include "linesplitter.h"


namespace nx_http
{
    //!Parses stream of bytes as http message
    /*!
        Can handle multiple subsequent messages
        \note Supports chunked stream
        \note Class methods are not thread-safe
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
        Message m_httpMessage;
        quint64 m_contentLength;
        bool m_isChunkedTransfer;
        quint64 m_messageBodyBytesRead;
        BufferType m_msgBodyBuffer;

        //!HTTP/1.1 chunk stream parsing
        ChunkStreamParseState m_chunkStreamParseState;
        size_t m_currentChunkSize;
        size_t m_currentChunkBytesRead;
        BufferType::value_type m_prevChar;

        LineSplitter m_lineSplitter;

        bool parseLine( const ConstBufferRefType& data );
        //!Determines, whether Message-Body is available and fills \a m_contentLength and \a m_isChunkedTransfer
        bool isMessageBodyFollows();
        size_t readChunkStream( const BufferType& data, size_t offset = 0, size_t count = BufferNpos );
        size_t readIdentityStream( const BufferType& data, size_t offset = 0, size_t count = BufferNpos );
        unsigned int hexCharToInt( BufferType::value_type ch );
    };
}

#endif  //HTTPSTREAMREADER_H
