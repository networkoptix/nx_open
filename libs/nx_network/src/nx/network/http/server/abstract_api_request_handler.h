// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/http_message_dispatcher.h>

#include "detail/abstract_api_request_handler_detail.h"

namespace nx::network::http {

/**
 * HTTP server request handler which deserializes input/serializes output data using nx::reflect.
 * Reads format query parameter from incoming request and deserializes request/serializes response accordingly.
 * Request that returns some data should pass it to requestCompleted function.
 * NOTE: Input  may be void. If Input is void, AbstractApiRequestHandler::processRequest have no
 * inputData argument.
 * NOTE: GET request input data is always deserialized from url query.
 * NOTE: POST request output data is ignored.
 */
template<typename Input>
class AbstractApiRequestHandler:
    public detail::BaseApiRequestHandler<Input, AbstractApiRequestHandler<Input>>
{
    using base_type = typename AbstractApiRequestHandler::BaseApiRequestHandler;

public:
    using base_type::processRequest;

    /**
     * Here actual HTTP request implementation resides.
     * On request processing completion requestCompleted(ApiRequestResult, Output) MUST be invoked.
     * NOTE: If Input is void, then this method does not have inputData argument.
     * NOTE: If Output is void, then requestCompleted(ApiRequestResult).
     */
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        Input inputData) = 0;
};

// NX_NETWORK_API export/import macro is needed to avoid Multiple Definitions errors, since this is
// a full template specialization (an actual class, not a template).
template<>
class NX_NETWORK_API AbstractApiRequestHandler<void>:
    public detail::BaseApiRequestHandler<void, AbstractApiRequestHandler<void>>
{
    using base_type = BaseApiRequestHandler;

public:
    using base_type::processRequest;

    /**
     * Here actual HTTP request implementation resides.
     * On request processing completion requestCompleted(ApiRequestResult) MUST be invoked.
     */
    virtual void processRequest(http::RequestContext requestContext) = 0;
};

//-------------------------------------------------------------------------------------------------

template<typename Func>
class CustomApiRequestHandler:
    public AbstractApiRequestHandler<void>
{
public:
    CustomApiRequestHandler() = default;
    CustomApiRequestHandler(Func func):
        m_func(std::move(func))
    {
    }

    virtual void processRequest(
        http::RequestContext /*requestContext*/)
    {
        this->requestCompleted(ApiRequestResult(), m_func());
    }

private:
    Func m_func;
};

template<typename HttpMessageDispatcher, typename Func>
// requires SerializableToJson(typename std::invoke_result_t<Func>)
void registerApiRequestHandler(
    HttpMessageDispatcher* dispatcher,
    const char* path,
    Func func,
    std::string_view method = kAnyMethod)
{
    using Handler = CustomApiRequestHandler<Func>;

    dispatcher->registerRequestProcessor(
        path,
        [func]() { return std::make_unique<Handler>(func); },
        method);
}

} // namespace nx::network::http
