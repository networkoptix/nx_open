#pragma once

#include <nx/network/http/server/rest/base_request_handler.h>

#include <nx/network/cloud/storage/service/api/result.h>

namespace nx::cloud::storage::service::view::http {

template<typename Input, typename Output, typename... RestArgFetchers>
class RequestHandler:
    public nx::network::http::server::rest::BaseRequestHandler<
        RequestHandler<Input, Output, RestArgFetchers...>,
        api::Result,
        Input, Output, RestArgFetchers...>
{
    using base_type = nx::network::http::server::rest::BaseRequestHandler<
        RequestHandler<Input, Output, RestArgFetchers...>,
        api::Result,
        Input, Output, RestArgFetchers...>;

public:
    RequestHandler(typename base_type::BusinessFunc businessFunc):
        base_type(std::move(businessFunc))
    {
    }

    template<typename... OutputData>
    void processResponseInternal(
        api::Result result, OutputData... output);

private:
    static network::http::StatusCode::Value toHttpStatusCode(api::ResultCode resultCode)
    {
        using namespace nx::network::http;
        switch (resultCode)
        {
            case api::ResultCode::ok:
                return StatusCode::ok;
            case api::ResultCode::badRequest:
            case api::ResultCode::awsApiError:
                return StatusCode::badRequest;
            case api::ResultCode::unauthorized:
                return StatusCode::unauthorized;
            case api::ResultCode::notFound:
                return StatusCode::notFound;
            case api::ResultCode::internalError:
            default:
                return StatusCode::internalServerError;
        }
    }
};

template<typename Input, typename Output, typename... RestArgFetchers>
template<typename... OutputData>
void RequestHandler<Input, Output, RestArgFetchers...>::processResponseInternal(
    api::Result result, OutputData... output)
{
    if (result.resultCode != api::ResultCode::ok)
    {
        network::http::FusionRequestResult error;
        error.setHttpStatusCode(toHttpStatusCode(result.resultCode));
        error.resultCode = toString(result.resultCode);
        error.errorText = result.error.c_str();
        return this->requestCompleted(std::move(error), std::move(output)...);
    }

    // OK case, forcing invocation of requestCompleted overload with FusionRequestResult parameter
    // instead of http::StatusCode::Value, which is undefined if constructed by default
    this->requestCompleted(network::http::FusionRequestResult(), std::move(output)...);
}

} // namespace nx::cloud::storage::service::view::http
