#include "client.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "request_paths.h"

namespace nx::cloud::storage::service::api {

using namespace nx::network::http;

Client::Client(const nx::utils::Url& cloudStorageServiceUrl):
    base_type(cloudStorageServiceUrl)
{
}

void Client::addStorage(
    const AddStorageRequest& request,
    nx::utils::MoveOnlyFunc<void(ResultCode, AddStorageResponse)> handler)
{
    base_type::template makeAsyncCall<AddStorageResponse>(
        Method::put,
        api::kStorages,
        std::move(handler),
        request);
}

void Client::readStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(ResultCode, ReadStorageResponse)> handler)
{
    base_type::template makeAsyncCall<ReadStorageResponse>(
        Method::get,
        rest::substituteParameters(api::kStorageId, {storageId}),
        std::move(handler));
}

void Client::removeStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        Method::delete_,
        rest::substituteParameters(api::kStorageId, {storageId}),
        std::move(handler));
}

Client::ResultCode Client::toResultCode(SystemError::ErrorCode errorCode)
{
    switch (errorCode)
    {
        case SystemError::noError:
            return api::ResultCode::ok;
        case SystemError::timedOut:
            return api::ResultCode::timedOut;
        default:
            return api::ResultCode::networkError;
    }
}

Client::ResultCode Client::toResultCode(const network::http::Response* response)
{
    if (StatusCode::isSuccessCode(response->statusLine.statusCode))
        return api::ResultCode::ok;

    switch (response->statusLine.statusCode)
    {
        case StatusCode::unauthorized:
            return api::ResultCode::unauthorized;
        case StatusCode::badRequest:
            return api::ResultCode::badRequest;
        case StatusCode::notFound:
            return api::ResultCode::notFound;
        default:
            return api::ResultCode::unknownError;
    }
}

} // namespace nx::cloud::storage::service::api