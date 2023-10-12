// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <nx/network/debug/object_instance_counter.h>

#include "request_processing_types.h"
#include "../abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Interface of an HTTP request handler class.
 * The implementation may choose to make the AbstractRequestHandler::serve re-enterable.
 */
class NX_NETWORK_API AbstractRequestHandler
{
public:
    virtual ~AbstractRequestHandler() = default;

    /**
     * Processes request, generates response and reports it to the completionHandler callback.
     * Note: requestContext.connection is guaranteed to be alive in this call only.
     * It can be closed at any moment. So, it cannot be considered valid after executing any
     * asynchronous operation.
     */
    virtual void serve(
        network::http::RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(network::http::RequestResult)> completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Request handler that passes request to another handler after processing.
 * It may choose to generate reply itself and avoid passing request further.
 */
class NX_NETWORK_API IntermediaryHandler:
    public AbstractRequestHandler
{
public:
    IntermediaryHandler(AbstractRequestHandler* nextHandler):
        m_nextHandler(nextHandler)
    {
    }

    void setNextHandler(AbstractRequestHandler* nextHandler)
    {
        m_nextHandler = nextHandler;
    }

    const AbstractRequestHandler* nextHandler() const { return m_nextHandler; }
    AbstractRequestHandler* nextHandler() { return m_nextHandler; }

private:
    AbstractRequestHandler* m_nextHandler = nullptr;
};

//-------------------------------------------------------------------------------------------------

/**
 * Base class for all HTTP request processors.
 * NOTE: Class methods are not thread-safe.
 */
class NX_NETWORK_API RequestHandlerWithContext:
    public AbstractRequestHandler
{
public:
    /**
     * The received request is passed here. It will be propagated to processRequest.
     * @param connection This object is valid only in this method.
     *     One cannot rely on its availability after return of this method.
     * @param completionHandler Functor to be invoked to send response.
     */
    virtual void serve(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) override;

    /**
     * By default, it is MessageBodyDeliveryType::buffer.
     * Note: setting to MessageBodyDeliveryType::stream means transparent forwarding the
     * message body from serve() to processRequest().
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
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) = 0;

    // TODO: #akolesnikov Remove this method.
    http::Response* response();

private:
    RequestContext m_requestContext;
    std::unique_ptr<AbstractMsgBodySourceWithCache> m_requestBodySource;
    RequestProcessedHandler m_completionHandler;
    http::Message m_responseMsg;
    MessageBodyDeliveryType m_messageBodyDeliveryType = MessageBodyDeliveryType::buffer;
    RequestPathParams m_requestPathParams;
    debug::ObjectInstanceCounter<RequestHandlerWithContext> m_instanceCounter;
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
