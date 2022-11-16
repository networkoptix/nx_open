// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <nx/utils/type_utils.h>

#include "detail/abstract_api_request_handler_detail.h"
#include "http_message_dispatcher.h"
#include "../http_types.h"

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
template<typename... Input>
// requires sizeof...(Input) <= 1
class AbstractApiRequestHandler:
    public detail::BaseApiRequestHandler<
        nx::utils::tuple_first_element_t<std::tuple<Input...>, void>,
        AbstractApiRequestHandler<Input...>>
{
    using base_type = detail::BaseApiRequestHandler<
        nx::utils::tuple_first_element_t<std::tuple<Input...>, void>,
        AbstractApiRequestHandler<Input...>>;

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
        Input... inputData) = 0;

    /**
     * Call this method when processed request. outputData argument is missing when Output is void.
     * This method is here just for information purpose. Defined in a base class.
     */
    //void requestCompleted(
    //    ApiRequestResult result,
    //    Output... output);
};

template<>
class AbstractApiRequestHandler<void>:
    public detail::BaseApiRequestHandler<
        void,
        AbstractApiRequestHandler<void>>
{
    using base_type = detail::BaseApiRequestHandler<
        void,
        AbstractApiRequestHandler<void>>;

public:
    using base_type::processRequest;

    /**
     * Here actual HTTP request implementation resides.
     * On request processing completion requestCompleted(ApiRequestResult) MUST be invoked.
     */
    virtual void processRequest(http::RequestContext requestContext) = 0;
};

//class NX_NETWORK_API NoInputApiRequestHandler: public AbstractApiRequestHandler<void> {};

//-------------------------------------------------------------------------------------------------

template<typename Func>
class CustomApiRequestHandler:
    public AbstractApiRequestHandler<>
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
    const std::string_view& method = kAnyMethod)
{
    using Handler = CustomApiRequestHandler<Func>;

    dispatcher->registerRequestProcessor(
        path,
        [func]() { return std::make_unique<Handler>(func); },
        method);
}

} // namespace nx::network::http
