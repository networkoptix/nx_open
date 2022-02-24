// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <nx/network/debug/object_instance_counter.h>

#include "http_server_connection.h"
#include "request_processing_types.h"
#include "../abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Base class for all HTTP request processors.
 * NOTE: Class methods are not thread-safe.
 */
class NX_NETWORK_API AbstractHttpRequestHandler
{
public:
    virtual ~AbstractHttpRequestHandler() = default;

    /**
     * The received request is passed here. It will be propagated to processRequest.
     * @param connection This object is valid only in this method.
     *     One cannot rely on its availability after return of this method.
     * @param completionHandler Functor to be invoked to send response.
     */
    void handleRequest(
        RequestContext requestContext,
        ResponseIsReadyHandler completionHandler);

    /**
     * By default, it is MessageBodyDeliveryType::buffer.
     */
    void setRequestBodyDeliveryType(MessageBodyDeliveryType value);

    void setRequestPathParams(RequestPathParams params);

protected:
    /**
     * Implement this method to process the request.
     * @param response Implementation is allowed to modify response as it wishes
     * WARNING: This object can be removed in completionHandler
     */
    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) = 0;

    http::Response* response();

private:
    RequestContext m_requestContext;
    std::unique_ptr<AbstractMsgBodySourceWithCache> m_requestBodySource;
    ResponseIsReadyHandler m_completionHandler;
    http::Message m_responseMsg;
    MessageBodyDeliveryType m_messageBodyDeliveryType = MessageBodyDeliveryType::buffer;
    RequestPathParams m_requestPathParams;
    debug::ObjectInstanceCounter<AbstractHttpRequestHandler> m_instanceCounter;
    std::string m_pathTemplate;

    void readMessageBodyAsync();

    /**
     * @return true if message body has been read to the end.
     * false if another read has been scheduled.
     */
    bool processMessageBodyBuffer(
        SystemError::ErrorCode resultCode, nx::Buffer buffer);

    void propagateRequest();
    void sendResponse(RequestResult requestResult);
};

} // namespace nx::network::http
