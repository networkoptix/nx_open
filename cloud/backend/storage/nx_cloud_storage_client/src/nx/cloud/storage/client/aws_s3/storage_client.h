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

    virtual void open(
        const std::string& storageClientId,
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials) override;

    virtual AbstractContentClient* getContentClient() override;
    virtual AbstractMetadataClient* getMetadataClient() override;

private:
    std::unique_ptr<ContentClient> m_contentClient;
};

} // namespace nx::cloud::storage::client::aws_s3
