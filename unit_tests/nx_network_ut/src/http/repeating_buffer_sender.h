/**********************************************************
* Aug 24, 2015
* a.kolesnikov
***********************************************************/

#ifndef REPEATING_BUFFER_SENDER_H
#define REPEATING_BUFFER_SENDER_H

#include <nx/network/http/server/abstract_http_request_handler.h>


//!Sends specified buffer forever
class RepeatingBufferSender
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    RepeatingBufferSender(const nx_http::StringType& mimeType, nx::Buffer buffer);

    //!Implementation of nx_http::AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

private:
    const nx_http::StringType m_mimeType;
    nx::Buffer m_buffer;
};

#endif  //REPEATING_BUFFER_SENDER_H
