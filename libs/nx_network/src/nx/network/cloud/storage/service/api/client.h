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
    using ResultCode = api::ResultCode;

    Client(const nx::utils::Url& cloudStorageServiceUrl);

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

    /**
     * NOTE: Error is only defined if ResultCode is not ResultCode::ok.
     * If an error occurs, it contains the information from the underlying service
     * that generated it.
     */
    void addBucket(
        const AddBucketRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, Error)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<Bucket>)> handler);

    template<typename... Output>
    static auto getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const Output&... output);

private:
    static ResultCode toResultCode(SystemError::ErrorCode errorCode);
    static ResultCode toResultCode(const network::http::Response* response);
};

template<typename... Output>
auto Client::getResultCode(
    SystemError::ErrorCode systemErrorCode,
    const network::http::Response* response,
    const Output&... /*output*/)
{
    if (systemErrorCode != SystemError::noError)
        return toResultCode(systemErrorCode);

    if (!response)
        return api::ResultCode::networkError;

    return toResultCode(response);
}

} // namespace nx::cloud::storage::service::api