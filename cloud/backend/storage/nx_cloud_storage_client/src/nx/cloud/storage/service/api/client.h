#pragma once

#include <nx/network/http/generic_api_client.h>

#include "add_system.h"
#include "add_bucket.h"
#include "add_storage.h"
#include "bucket.h"
#include "result.h"
#include "storage.h"
#include "storage_credentials.h"
#include "system.h"

namespace nx::cloud::storage::service::api {

class NX_CLOUD_STORAGE_CLIENT_API Client:
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

    void addSystem(
        const std::string& storageId,
        const AddSystemRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, System)> handler);

    void removeSystem(
        const std::string& storageId,
        const std::string& systemId,
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

    void getCredentials(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(ResultCode, StorageCredentials)> handler);

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

    if (!response)
        return api::ResultCode::networkError;

    auto result = toResultCode(response);
    if (!result.ok())
    {
        result.error = fusionRequestResult.errorText.toStdString();
        return result;
    }

    return toResultCode(fusionRequestResult);
}

} // namespace nx::cloud::storage::service::api