/**********************************************************
* Aug 24, 2015
* a.kolesnikov
***********************************************************/

#ifndef REPEATING_BUFFER_SENDER_H
#define REPEATING_BUFFER_SENDER_H

#include <utils/network/http/server/abstract_http_request_handler.h>


//!Sends specified buffer forever
class RepeatingBufferSender
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    RepeatingBufferSender( const nx_http::StringType& mimeType, nx::Buffer buffer );

    //!Implementation of \a nx_http::AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override;

private:
    const nx_http::StringType m_mimeType;
    nx::Buffer m_buffer;
};

#endif  //REPEATING_BUFFER_SENDER_H
