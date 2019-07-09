#include "storage_client.h"

#include "content_client.h"

namespace nx::cloud::storage::client::aws_s3 {

StorageClient::StorageClient() = default;

StorageClient::~StorageClient() = default;

void StorageClient::open(
    const std::string& storageClientId,
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    m_contentClient = std::make_unique<ContentClient>(storageClientId, url, credentials);

    // TODO: #ak Verify url and credentials.
    post([this, handler = std::move(handler)]() { handler(ResultCode::ok); });
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
