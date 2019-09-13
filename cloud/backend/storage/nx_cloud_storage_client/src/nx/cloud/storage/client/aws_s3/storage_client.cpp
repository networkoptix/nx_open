#include "storage_client.h"

#include <nx/utils/log/log.h>

#include "content_client.h"
#include "utils.h"

namespace nx::cloud::storage::client::aws_s3 {

StorageClient::StorageClient() = default;

StorageClient::~StorageClient()
{
    pleaseStopSync();
}

void StorageClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_apiClient)
        m_apiClient->bindToAioThread(aioThread);

    if (m_contentClient)
        m_contentClient->bindToAioThread(aioThread);
}

void StorageClient::open(
    const std::string& storageClientId,
    const nx::utils::Url& awsS3BucketUrl,
    const nx::network::http::Credentials& credentials,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    NX_INFO(this, "Initializing aws S3 storage client using URL %1 and client id %2",
        awsS3BucketUrl, storageClientId);

    m_storageClientId = storageClientId;
    m_awsS3BucketUrl = awsS3BucketUrl;
    m_credentials = credentials;
    m_openHandler = std::move(handler);

    resolveAwsBucketRegion();
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

void StorageClient::stopWhileInAioThread()
{
    m_apiClient.reset();
    m_contentClient.reset();
}

std::string StorageClient::awsBucketRegion() const
{
    return m_awsRegion;
}

void StorageClient::resolveAwsBucketRegion()
{
    NX_VERBOSE(this, "Resolving AWS bucket %1 region", m_awsS3BucketUrl);

    m_apiClient = std::make_unique<aws::s3::ApiClient>(
        "us-east-1", //< Can use any valid region to resolve the region of a bucket.
        m_awsS3BucketUrl,
        m_credentials);
    m_apiClient->bindToAioThread(getAioThread());

    m_apiClient->getLocation(
        [this](auto&&... args)
        {
            handleGetLocationResult(std::forward<decltype(args)>(args)...);
        });
}

void StorageClient::handleGetLocationResult(aws::Result result, std::string awsRegion)
{
    m_apiClient.reset();

    if (!result.ok())
    {
        NX_DEBUG(this, "Failed to resolve AWS bucket %1 region: %2",
            m_awsS3BucketUrl, result.text());
        return nx::utils::swapAndCall(m_openHandler, toResultCode(result));
    }

    NX_DEBUG(this, "Resolved AWS bucket %1 region to %2", m_awsS3BucketUrl, awsRegion);

    m_awsRegion = awsRegion;
    initialize();

    nx::utils::swapAndCall(m_openHandler, ResultCode::ok);
}

void StorageClient::initialize()
{
    m_contentClient = std::make_unique<ContentClient>(
        m_storageClientId,
        m_awsRegion,
        m_awsS3BucketUrl,
        m_credentials);
    m_contentClient->bindToAioThread(getAioThread());

    // TODO: #ak Instantiate AbstractMetadataClient.
}

} // namespace nx::cloud::storage::client::aws_s3
