/**********************************************************
* 12 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef MULTIPART_CONTENT_PARSER_H
#define MULTIPART_CONTENT_PARSER_H

#include "../../media/abstract_byte_stream_filter.h"
#include "linesplitter.h"


namespace nx_http
{
    class MultipartContentParser
    :
        public AbstractByteStreamConverter
    {
    public:
        MultipartContentParser();
        virtual ~MultipartContentParser();

        //!Implementation of AbstractByteStreamFilter::processData
        virtual void processData( const QnByteArrayConstRef& data ) override;
        //!Implementation of AbstractByteStreamFilter::flush
        virtual size_t flush() override;

        bool setContentType( const StringType& contentType );
        void setBoundary( const StringType& boundary );

    private:
        // TODO: #Elric #enum
        enum ParsingState
        {
            waitingBoundary,
            readingHeaders,
            readingTextData,
            readingBinaryData
        };

        ParsingState m_state;
        LineSplitter m_lineSplitter;
        nx_http::BufferType m_currentFrame;
        StringType m_boundary;
        StringType m_startBoundaryLine;
        StringType m_endBoundaryLine;
        unsigned int m_contentLength;
    };
}

#endif  //MULTIPART_CONTENT_PARSER_H
