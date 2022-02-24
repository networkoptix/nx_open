// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "../abstract_api_request_handler.h"

namespace nx::network::http::server::handler {

/**
 * HTTP-handler that wraps a `ResultType func()` functor into a HTTP GET hander.
 */
template<typename ResultType>
class GetHandler:
    public nx::network::http::AbstractApiRequestHandler<>
{
public:
    using FunctorType = nx::utils::MoveOnlyFunc<ResultType()>;

    GetHandler(FunctorType func):
        m_func(std::move(func))
    {
    }

private:
    virtual void processRequest(
        nx::network::http::RequestContext /*requestContext*/) override
    {
        auto data = m_func();
        this->requestCompleted(
            nx::network::http::ApiRequestResult(),
            std::move(data));
    }

private:
    FunctorType m_func;
};

//-------------------------------------------------------------------------------------------------

/**
 * HTTP-handler that wraps a `void func(nx::utils::MoveOnlyFunc<ResultType()> completionHandler)`
 * functor into a HTTP GET hander.
 */
template<typename ResultType>
class GetAsyncHandler:
    public nx::network::http::AbstractApiRequestHandler<>
{
public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(ResultType)>;
    using FunctorType = nx::utils::MoveOnlyFunc<void(CompletionHandler)>;

    GetAsyncHandler(FunctorType func):
        m_func(std::move(func))
    {
    }

private:
    virtual void processRequest(
        nx::network::http::RequestContext /*requestContext*/) override
    {
        m_func(
            [this](ResultType result)
            {
                this->requestCompleted(
                    nx::network::http::ApiRequestResult(),
                    std::move(result));
            });
    }

private:
    FunctorType m_func;
};

} // namespace nx::network::http::server::handler
