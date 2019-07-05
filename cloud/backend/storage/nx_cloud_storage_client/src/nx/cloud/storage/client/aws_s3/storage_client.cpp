#include "storage_client.h"

#include "content_client.h"

namespace nx::cloud::storage::client::aws_s3 {

StorageClient::StorageClient() = default;

StorageClient::~StorageClient() = default;

void StorageClient::open(
    const std::string& storageClientId,
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials)
{
    m_contentClient = std::make_unique<ContentClient>(storageClientId, url, credentials);
    // TODO
}

AbstractContentClient* StorageClient::getContentClient()
{
    return m_contentClient.get();
}

AbstractMetadataClient* StorageClient::getMetadataClient()
{
    // TODO
    return nullptr;
}

} // namespace nx::cloud::storage::client::aws_s3
