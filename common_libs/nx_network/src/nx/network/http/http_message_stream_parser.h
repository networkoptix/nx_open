#pragma once 

#include <nx/utils/abstract_byte_stream_filter.h>

#include "httptypes.h"
#include "httpstreamreader.h"

namespace nx_http {

/*!
    Pushes message of parsed request to the next filter
*/
class NX_NETWORK_API HttpMessageStreamParser:
    public nx::utils::bsf::AbstractByteStreamFilter
{
public:
    HttpMessageStreamParser();
    virtual ~HttpMessageStreamParser();

    virtual bool processData( const QnByteArrayConstRef& data ) override;
    virtual size_t flush() override;

    //!Returns previous http message
    /*!
        Message is available only within nx::utils::bsf::AbstractByteStreamFilter::processData call of the next filter
    */
    nx_http::Message currentMessage() const;

private:
    nx_http::HttpStreamReader m_httpStreamReader;
};

} // namespace nx_http
