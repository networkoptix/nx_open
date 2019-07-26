#pragma once

#include <nx/network/http/generic_api_client.h>

#include "add_storage.h"
#include "storage.h"
#include "result.h"
#include "add_bucket.h"
#include "bucket.h"

namespace nx::cloud::storage::service::api {

class NX_NETWORK_API Client:
    public network::http::GenericApiClient<Client>
{
    using base_type = network::http::GenericApiClient<Client>;

public:
    using ResultCode = api::Result;

    Client(const nx::utils::Url& cloudStorageServiceUrl);
    ~Client();

    void addStorage(
        const AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, Storage)> handler);

    void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(ResultCode, Storage)> handler);

    void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);

    void listCameras(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<std::string>)> handler);

    void addBucket(
        const AddBucketRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, Bucket)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(ResultCode, Buckets)> handler);

    template<typename... Output>
    static ResultCode getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const network::http::FusionRequestResult& fusionRequestResult,
        const Output&... output);

private:
    static ResultCode toResultCode(SystemError::ErrorCode errorCode);
    static ResultCode toResultCode(const network::http::Response* response);
    static ResultCode toResultCode(const network::http::FusionRequestResult& fusionRequestResult);
};

template<typename... Output>
Client::ResultCode Client::getResultCode(
    SystemError::ErrorCode systemErrorCode,
    const network::http::Response* response,
    const network::http::FusionRequestResult& fusionRequestResult,
    const Output&... /*output*/)
{
    if (systemErrorCode != SystemError::noError)
        return toResultCode(systemErrorCode);

    if (!fusionRequestResult.resultCode.isEmpty())
        return toResultCode(fusionRequestResult);

    if (!response)
        return api::ResultCode::networkError;

    return toResultCode(response);
}

} // namespace nx::cloud::storage::service::api