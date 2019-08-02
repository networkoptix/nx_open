#include "bucket_api_ut.h"

namespace nx::cloud::storage::service::api::test {

namespace {

class CloudDBAuthenticationForwarderStub:
    public network::http::server::AbstractAuthenticationManager
{
public:
    CloudDBAuthenticationForwarderStub(nx::utils::SyncQueue<bool>* authenticationEvent):
        m_authenticationEvent(authenticationEvent)
    {
    }

    virtual void authenticate(
        const network::http::HttpServerConnection& /*connection*/,
        const network::http::Request& /*request*/,
        network::http::server::AuthenticationCompletionHandler completionHandler) override
    {
        m_authenticationEvent->push(true);
        completionHandler(network::http::server::SuccessfulAuthenticationResult());
    }

private:
    nx::utils::SyncQueue<bool>* m_authenticationEvent = nullptr;
};

} // namespace

class StorageApi:
    public BucketApi
{
    using base_type = BucketApi;

public:
    ~StorageApi()
    {
        if (m_cloudDBAuthenticationFactoryFuncBak)
        {
            view::http::CloudDbAuthenticationFactory::instance().setCustomFunc(
                std::move(m_cloudDBAuthenticationFactoryFuncBak));
        }
    }

protected:
    virtual void SetUp()override
    {
        m_cloudDBAuthenticationFactoryFuncBak =
            view::http::CloudDbAuthenticationFactory::instance().setCustomFunc(
                [this](const auto& /*settings*/)
                {
                    return std::make_unique<CloudDBAuthenticationForwarderStub>(
                        &m_cloudDbAuthenticationEvent);
                });

        base_type::SetUp();
    }

    void givenAddedStorage()
    {
        whenAddStorageWithRegion();
        thenRequestIsForwardedToCloudDb();
        thenAddStorageSucceeds();
        andBucketHasCloudStorageCountUpdated(1);
    }

    void whenAddStorageWithRegion()
    {
        m_cloudStorageClient->addStorage(
            AddStorageRequest{1000, m_lastBucketCreated->location()},
            [this](auto result, auto storage)
            {
                m_addStorageResponse.push({std::move(result), std::move(storage)});
            });
    }

    void whenReadStorage()
    {
        readStorage(m_lastStorageAdded.id);
    }

    void whenRemoveStorage()
    {
        m_cloudStorageClient->removeStorage(
            m_lastStorageAdded.id,
            [this, removedStorage = m_lastStorageAdded](auto result)
            {
                if (result.resultCode == ResultCode::ok)
                    m_lastStorageRemoved = std::move(removedStorage);
                m_removeStorageResponse.push(std::move(result));
            });
    }

    void thenRequestIsForwardedToCloudDb()
    {
        ASSERT_TRUE(m_cloudDbAuthenticationEvent.pop());
    }

    void thenAddStorageSucceeds()
    {
        auto [result, storage] = m_addStorageResponse.pop();
        ASSERT_EQ(ResultCode::ok, result.resultCode);
        m_lastStorageAdded = std::move(storage);
    }

    void thenReadStorageSucceeds()
    {
        auto [result, storage] = m_readStorageResponse.pop();
        ASSERT_EQ(ResultCode::ok, result.resultCode);
        m_lastStorageRead = std::move(storage);
    }

    void thenRemoveStorageSucceeds()
    {
        auto result = m_removeStorageResponse.pop();
        ASSERT_EQ(ResultCode::ok, result.resultCode);
    }

    void andBucketHasCloudStorageCountUpdated(int cloudStorageCount)
    {
        whenListBuckets();
        thenListBucketsSucceeds();

        auto expectedBucket = m_lastBucketAdded;
        expectedBucket.cloudStorageCount = cloudStorageCount;

        const auto& buckets = m_lastBucketsListed.buckets;
        auto it = std::find(buckets.begin(), buckets.end(), expectedBucket);
        ASSERT_NE(it, buckets.end());
    }

    void andReadStorageContainsAddedStorage()
    {
        ASSERT_EQ(m_lastStorageAdded, m_lastStorageRead);
    }

    void andStorageIsNotInService()
    {
        readStorage(m_lastStorageRemoved.id);
        auto [result, storage] = m_readStorageResponse.pop();
        ASSERT_EQ(ResultCode::notFound, result.resultCode);
    }

private:
    void readStorage(const std::string& storageId)
    {
        m_cloudStorageClient->readStorage(
            storageId,
            [this](auto result, auto storage)
            {
                m_readStorageResponse.push({std::move(result), std::move(storage)});
            });
    }

private:
    nx::utils::SyncQueue<std::pair<Result, Storage>> m_addStorageResponse;
    nx::utils::SyncQueue<std::pair<Result, Storage>> m_readStorageResponse;
    nx::utils::SyncQueue<Result> m_removeStorageResponse;
    Storage m_lastStorageAdded;
    Storage m_lastStorageRead;
    Storage m_lastStorageRemoved;
    int m_lastCloudStorageCount = 0;

    nx::utils::SyncQueue<bool> m_cloudDbAuthenticationEvent;
    view::http::CloudDbAuthenticationFactory::Function m_cloudDBAuthenticationFactoryFuncBak;
};


TEST_F(StorageApi, add_storage)
{
    givenAddedBucket();

    whenAddStorageWithRegion();

    thenRequestIsForwardedToCloudDb();
    thenAddStorageSucceeds();

    andBucketHasCloudStorageCountUpdated(1);
}

TEST_F(StorageApi, read_storage)
{
    givenAddedBucket();
    givenAddedStorage();

    whenReadStorage();

    thenRequestIsForwardedToCloudDb();
    thenReadStorageSucceeds();

    andReadStorageContainsAddedStorage();
}

TEST_F(StorageApi, remove_storage)
{
    givenAddedBucket();
    givenAddedStorage();

    whenRemoveStorage();

    thenRequestIsForwardedToCloudDb();
    thenRemoveStorageSucceeds();

    andStorageIsNotInService();
}

} // namespace nx::cloud::storage::service::api::test

