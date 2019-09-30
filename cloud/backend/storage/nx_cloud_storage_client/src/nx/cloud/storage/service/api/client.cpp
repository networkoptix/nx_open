#include "client.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/switch.h>

#include "request_paths.h"

namespace nx::cloud::storage::service::api {

using namespace nx::network::http;

Client::Client(const nx::utils::Url& cloudStorageServiceUrl):
    base_type(cloudStorageServiceUrl)
{
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::addStorage(
    const AddStorageRequest& request,
    nx::utils::MoveOnlyFunc<void(ResultCode, Storage)> handler)
{
    base_type::template makeAsyncCall<Storage>(
        Method::put,
        api::kStorages,
        std::move(handler),
        request);
}

void Client::readStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(ResultCode, Storage)> handler)
{
    base_type::template makeAsyncCall<Storage>(
        Method::get,
        rest::substituteParameters(api::kStorage, {storageId}),
        std::move(handler));
}

void Client::removeStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        Method::delete_,
        rest::substituteParameters(api::kStorage, {storageId}),
        std::move(handler));
}

void Client::addSystem(
    const std::string& storageId,
    const AddSystemRequest& request,
    nx::utils::MoveOnlyFunc<void(ResultCode, System)> handler)
{
    base_type::template makeAsyncCall<System>(
        Method::put,
        rest::substituteParameters(api::kSystems, {storageId}),
        std::move(handler),
        request);
}

void Client::removeSystem(
    const std::string& storageId,
    const std::string& systemId,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        Method::delete_,
        rest::substituteParameters(api::kSystem, {storageId, systemId}),
        std::move(handler));
}

void Client::listCameras(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<std::string>)> handler)
{
    base_type::template makeAsyncCall<std::vector<std::string>>(
        rest::substituteParameters(api::kCameras, {storageId}),
        std::move(handler));
}

void Client::addBucket(
    const AddBucketRequest& request,
    nx::utils::MoveOnlyFunc<void(ResultCode, Bucket)> handler)
{
    base_type::template makeAsyncCall<Bucket>(
        Method::put,
        api::kAwsBuckets,
        std::move(handler),
        request);
}

void Client::removeBucket(
    const std::string& bucketName,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        Method::delete_,
        rest::substituteParameters(api::kAwsBucket, {bucketName}),
        std::move(handler));
}

void Client::listBuckets(
    nx::utils::MoveOnlyFunc<void(ResultCode, Buckets)> handler)
{
    base_type::template makeAsyncCall<Buckets>(
        Method::get,
        api::kAwsBuckets,
        std::move(handler));
}

void Client::getCredentials(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(ResultCode, StorageCredentials)> handler)
{
    base_type::template makeAsyncCall<StorageCredentials>(
        Method::get,
        rest::substituteParameters(api::kStorageCredentials, {storageId}),
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
        case StatusCode::forbidden:
            return api::ResultCode::forbidden;
        case StatusCode::badRequest:
            return api::ResultCode::badRequest;
        case StatusCode::notFound:
            return api::ResultCode::notFound;
        default:
            return api::ResultCode::unknownError;
    }
}


Client::ResultCode Client::toResultCode(const network::http::FusionRequestResult& fusionRequestResult)
{
    api::ResultCode resultCode = nx::utils::switch_(fusionRequestResult.resultCode,
        "ok", []() { return api::ResultCode::ok; },
        "badRequest", []() { return api::ResultCode::badRequest; },
        "unauthorized", []() { return api::ResultCode::unauthorized; },
		"forbidden", []() { return api::ResultCode::forbidden; },
        "notFound", []() { return api::ResultCode::notFound; },
        "internalError", []() { return api::ResultCode::internalError; },
        "timedOut", []() { return api::ResultCode::timedOut; },
        "networkError", []() { return api::ResultCode::networkError; },
        "awsApiError", []() { return api::ResultCode::awsApiError; },
        nx::utils::default_, [](){ return api::ResultCode::unknownError; });

    return ResultCode(resultCode, fusionRequestResult.errorText.toStdString());
}

} // namespace nx::cloud::storage::service::api