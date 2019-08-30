#pragma once

#include <memory>

#include "../abstract_storage_client.h"

namespace nx::cloud::storage::client::aws_s3 {

class ContentClient;

class NX_CLOUD_STORAGE_CLIENT_API StorageClient:
    public AbstractStorageClient
{
public:
    StorageClient();
    virtual ~StorageClient();

    StorageClient(StorageClient&&) = delete;
    StorageClient& operator=(StorageClient&&) = delete;

    /**
     * @param url Url of a AWS S3 bucket.
     */
    virtual void open(
        const std::string& storageClientId,
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler) override;

    virtual AbstractContentClient* getContentClient() override;
    virtual AbstractMetadataClient* getMetadataClient() override;

private:
    std::unique_ptr<ContentClient> m_contentClient;
};

} // namespace nx::cloud::storage::client::aws_s3
