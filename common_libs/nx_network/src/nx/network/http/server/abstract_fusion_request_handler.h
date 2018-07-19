#pragma once

#include <type_traits>

#include "detail/abstract_fusion_request_handler_detail.h"

#include "../http_types.h"
#include "http_message_dispatcher.h"

namespace nx {
namespace network {
namespace http {

/**
 * HTTP server request handler which deserializes input/serializes output data using fusion.
 * Reads format query parameter from incoming request and deserializes request/serializes response accordingly.
 * NOTE: Input or Output can be void. If Input is void,
 *   AbstractFusionRequestHandler::processRequest does not have inputData argument.
 *   If Output is void, AbstractFusionRequestHandler::requestCompleted does not have outputData argument.
 * NOTE: GET request input data is always deserialized from url query.
 * NOTE: POST request output data is ignored.
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
     * NOTE: If Input is void, then this method does not have inputData argument.
     * NOTE: If Output is void, then requestCompleted(FusionRequestResult).
     */
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        const nx::network::http::Request& request,
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
        nx::network::http::HttpServerConnection* const connection,
        const nx::network::http::Request& request,
        nx::utils::stree::ResourceContainer authInfo ) = 0;

private:
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler) override
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

//-------------------------------------------------------------------------------------------------

template<typename Func>
class CustomFusionRequestHandler:
    public AbstractFusionRequestHandler<void, typename std::result_of<Func()>::type>
{
public:
    CustomFusionRequestHandler() = default;
    CustomFusionRequestHandler(Func func):
        m_func(std::move(func))
    {
    }

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        const nx::network::http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/)
    {
        auto data = m_func();
        this->requestCompleted(FusionRequestResult(), std::move(data));
    }

private:
    Func m_func;
};

template<typename HttpMessageDispatcher, typename Func>
// requires SerializableToJson(typename std::invoke_result<Func>::type)
void registerFusionRequestHandler(
    HttpMessageDispatcher* dispatcher,
    const char* path,
    Func func,
    Method::ValueType method = kAnyMethod)
{
    using Handler = CustomFusionRequestHandler<Func>;

    dispatcher->template registerRequestProcessor<Handler>(
        path,
        [func]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(func);
        },
        method);
}

} // namespace nx
} // namespace network
} // namespace http
