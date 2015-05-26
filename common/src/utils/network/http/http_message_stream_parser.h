/**********************************************************
* 24 apr 2015
* a.kolesnikov
***********************************************************/

#ifndef HTTP_MESSAGE_STREAM_PARSER_H
#define HTTP_MESSAGE_STREAM_PARSER_H

#include <utils/media/abstract_byte_stream_filter.h>

#include "httptypes.h"
#include "httpstreamreader.h"


namespace nx_http
{
    /*!
        Pushes message of parsed request to the next filter
    */
    class HttpMessageStreamParser
    :
        public AbstractByteStreamFilter
    {
    public:
        HttpMessageStreamParser();
        virtual ~HttpMessageStreamParser();

        //!Implementation of AbstractByteStreamFilter::processData
        virtual void processData( const QnByteArrayConstRef& data ) override;
        //!Implementation of AbstractByteStreamFilter::flush
        virtual size_t flush() override;

        //!Returns previous http message
        /*!
            Message is available only within \a AbstractByteStreamFilter::processData call of the next filter
        */
        nx_http::Message currentMessage() const;

    private:
        nx_http::HttpStreamReader m_httpStreamReader;
    };
}

#endif  //HTTP_MESSAGE_STREAM_PARSER_H
