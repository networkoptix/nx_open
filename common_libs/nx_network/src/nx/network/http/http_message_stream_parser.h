#pragma once 

#include <nx/utils/abstract_byte_stream_filter.h>

#include "httptypes.h"
#include "httpstreamreader.h"

namespace nx_http {

/*!
    Pushes message of parsed request to the next filter
*/
class NX_NETWORK_API HttpMessageStreamParser:
    public AbstractByteStreamFilter
{
public:
    HttpMessageStreamParser();
    virtual ~HttpMessageStreamParser();

    //!Implementation of AbstractByteStreamFilter::processData
    virtual bool processData( const QnByteArrayConstRef& data ) override;
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

} // namespace nx_http
