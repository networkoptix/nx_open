#ifndef MULTIPART_CONTENT_PARSER_H
#define MULTIPART_CONTENT_PARSER_H

#include "utils/media/abstract_byte_stream_filter.h"
#include "linesplitter.h"

namespace nx_http
{
    /**
        Input: http multipart content stream.
        Output: separate content frames
    */
    class NX_NETWORK_API MultipartContentParser
    :
        public AbstractByteStreamFilter
    {
    public:
        MultipartContentParser();
        virtual ~MultipartContentParser();

        /** Implementation of AbstractByteStreamFilter::processData */
        virtual bool processData( const QnByteArrayConstRef& data ) override;
        /** Implementation of AbstractByteStreamFilter::flush */
        virtual size_t flush() override;

        /**
            \return \a false, if \a contentType does not specify multipart content
            \note After this method has been called, no \a MultipartContentParser::setBoundary call is needed
        */
        bool setContentType( const StringType& contentType );
        void setBoundary( const StringType& boundary );
        /** Returns headers of last read frame */
        const nx_http::HttpHeaders& prevFrameHeaders() const;
        /** Returns \a true if epilogue haws been received */
        bool eof() const;

    private:
        enum ParsingState
        {
            none,
            waitingBoundary,
            readingHeaders,
            readingTextData,
            /** reading trailing CR of LF before binary data */
            depleteLineFeedBeforeBinaryData,
            /** reading data with Content-Length known */
            readingSizedBinaryData,
            /** reading data with Content-Length not known: searching for boundary */
            readingUnsizedBinaryData,
            /** epilogue has been received */
            eofReached
        };

        enum ChunkParseState
        {
            waitingEndOfLine,
            checkingForBoundaryAfterEndOfLine
        };

        ParsingState m_state;
        ParsingState m_nextState;
        LineSplitter m_lineSplitter;
        nx_http::BufferType m_currentFrame;
        StringType m_boundary;
        StringType m_startBoundaryLine;
        StringType m_endBoundaryLine;
        StringType m_endBoundaryForUnsizedBinaryParsing;
        StringType m_startBoundaryForUnsizedBinaryParsing;
        StringType m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF;
        unsigned int m_contentLength;
        ChunkParseState m_chunkParseState;
        nx::Buffer m_supposedBoundary;
        nx_http::HttpHeaders m_currentFrameHeaders;

        bool processLine(const ConstBufferRefType& lineBuffer);
        bool readUnsizedBinaryData(
            QnByteArrayConstRef data,
            size_t* const offset );
    };
}

#endif  //MULTIPART_CONTENT_PARSER_H
