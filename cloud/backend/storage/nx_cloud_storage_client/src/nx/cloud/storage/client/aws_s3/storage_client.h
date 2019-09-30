#pragma once

#include <memory>

#include <nx/cloud/aws/s3/api_client.h>

#include "../abstract_storage_client.h"

namespace nx::cloud::storage::client::aws_s3 {

class ContentClient;

class NX_CLOUD_STORAGE_CLIENT_API StorageClient:
    public AbstractStorageClient
{
    using base_type = AbstractStorageClient;

public:
    StorageClient();
    virtual ~StorageClient();

    StorageClient(StorageClient&&) = delete;
    StorageClient& operator=(StorageClient&&) = delete;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void open(
        const std::string& storageClientId,
        const nx::utils::Url& awsS3BucketUrl,
        const nx::network::http::Credentials& credentials,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler) override;

    virtual AbstractContentClient* getContentClient() override;
    virtual AbstractMetadataClient* getMetadataClient() override;

    std::string awsBucketRegion() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<ContentClient> m_contentClient;

    std::string m_storageClientId;
    nx::utils::Url m_awsS3BucketUrl;
    nx::network::http::Credentials m_credentials;
    nx::utils::MoveOnlyFunc<void(ResultCode)> m_openHandler;
    std::unique_ptr<aws::s3::ApiClient> m_apiClient;
    std::string m_awsRegion;

    void resolveAwsBucketRegion();

    void handleGetLocationResult(aws::Result result, std::string awsRegion);

    void initialize();
};

} // namespace nx::cloud::storage::client::aws_s3
