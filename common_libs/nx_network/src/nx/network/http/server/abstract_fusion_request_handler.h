#pragma once

#include "detail/abstract_fusion_request_handler_detail.h"

#include "../http_types.h"

namespace nx_http {

/**
 * HTTP server request handler which deserializes input/serializes output data using fusion.
 * Reads format query parameter from incoming request and deserializes request/serializes response accordingly.
 * @note Input or Output can be void. If Input is void,
 *   AbstractFusionRequestHandler::processRequest does not have inputData argument.
 *   If Output is void, AbstractFusionRequestHandler::requestCompleted does not have outputData argument.
 * @note GET request input data is always deserialized from url query.
 * @note POST request output data is ignored.
 */
template<typename Input = void, typename Output = void>
class AbstractFusionRequestHandler:
    public detail::BaseFusionRequestHandlerWithInput<
        Input,
        Output,
        AbstractFusionRequestHandler<Input, Output>>
{
public:
    /**
     * Here actual HTTP request implementation resides.
     * On request processing completion requestCompleted(FusionRequestResult, Output) MUST be invoked.
     * @note If Input is void, then this method does not have inputData argument.
     * @note If Output is void, then requestCompleted(FusionRequestResult).
     */
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo,
        Input inputData) = 0;

    /**
     * Call this method when processed request. outputData argument is missing when Output is void.
     * This method is here just for information purpose. Defined in a base class.
     */
    //void requestCompleted(
    //    FusionRequestResult result,
    //    Output outputData);
};

/**
 * Partial specialization with no input.
 */
template<typename Output>
class AbstractFusionRequestHandler<void, Output>:
    public detail::BaseFusionRequestHandlerWithOutput<Output>
{
public:
    /**
     * Here actual HTTP request implementation resides.
     * On request processing completion requestCompleted(FusionRequestResult) MUST be invoked.
     */
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo ) = 0;

private:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override
    {
        this->m_completionHandler = std::move(completionHandler);
        this->m_requestMethod = request.requestLine.method;

        FusionRequestResult errorDescription;
        if (!this->getDataFormat(request, nullptr, &errorDescription))
        {
            this->requestCompleted(std::move(errorDescription));
            return;
        }

        processRequest(
            connection,
            request,
            std::move(authInfo));
    }
};

} // namespace nx_http
