#include "bucket_api_ut.h"

#include <nx/network/url/url_builder.h>

namespace nx::cloud::storage::service::api::test {

namespace{

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

    void givenMultipleAddedBuckets()
    {
        m_expectedBucket = givenAddedBucket("us-east-1"); // <North America
        givenAddedBucket("eu-west-1"); //< Europe
    }

    void givenAddedStorage()
    {
        whenAddStorageWithRegion();
        thenRequestIsForwardedToCloudDb();
        thenAddStorageResponseIs(ResultCode::ok);
        andBucketHasCloudStorageCountUpdated(1);
    }

    void whenAddStorageWithRegion()
    {
        addStorage({1000, m_lastBucketCreated->location()});
    }

    void whenAddMultipleStorages(int count = 2)
    {
        for (int i = 0; i < count; ++i)
            whenAddStorageWithRegion();
    }

    void whenAddStorageWithoutRegion()
    {
        using namespace nx::geo_ip;
        m_geoIpResolver->add("127.0.0.1", Location(Continent::northAmerica));

        addStorage({1000, {}});
    }

    void whenAddStorageWithInvalidSize()
    {
        addStorage({0, m_lastBucketCreated->location()});
    }

    void whenAddStorageWithUnknownRegion()
    {
        addStorage({1000, "unkown-aws-region"});
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

    void thenAddStorageResponseIs(ResultCode resultCode)
    {
        auto [result, storage] = m_addStorageResponse.pop();
        ASSERT_EQ(resultCode, result.resultCode);
        m_addedStorages.push_back(storage);
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

    void andStorageServiceHasMultipleStorages()
    {
        for (const auto& storage : m_addedStorages)
        {
            readStorage(storage.id);
            thenReadStorageSucceeds();
            ASSERT_EQ(storage.id, m_lastStorageRead.id);
        }
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

    void andBucketChosenForStorageIsClosestToClient()
    {
        ASSERT_EQ(
            m_expectedBucket->url(),
            network::url::Builder(m_lastStorageAdded.ioDevices.back().dataUrl).setPath({}));
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

    void addStorage(const AddStorageRequest& request)
    {
        m_cloudStorageClient->addStorage(
            request,
            [this](auto result, auto storage)
            {
                m_addStorageResponse.push({std::move(result), std::move(storage)});
            });
    }

private:
    nx::utils::SyncQueue<std::pair<Result, Storage>> m_addStorageResponse;
    nx::utils::SyncQueue<std::pair<Result, Storage>> m_readStorageResponse;
    nx::utils::SyncQueue<Result> m_removeStorageResponse;
    Storage m_lastStorageAdded;
    std::vector<Storage> m_addedStorages;
    Storage m_lastStorageRead;
    Storage m_lastStorageRemoved;
    service::test::S3Bucket* m_expectedBucket = nullptr;

    nx::utils::SyncQueue<bool> m_cloudDbAuthenticationEvent;
    view::http::CloudDbAuthenticationFactory::Function m_cloudDBAuthenticationFactoryFuncBak;
};

TEST_F(StorageApi, add_storage_by_region)
{
    givenAddedBucket();

    whenAddStorageWithRegion();

    thenRequestIsForwardedToCloudDb();
    thenAddStorageResponseIs(ResultCode::ok);

    andBucketHasCloudStorageCountUpdated(1);
}

TEST_F(StorageApi, add_storage_by_client_location)
{
    givenMultipleAddedBuckets();

    whenAddStorageWithoutRegion();

    thenAddStorageResponseIs(ResultCode::ok);

    andBucketChosenForStorageIsClosestToClient();
}

TEST_F(StorageApi, add_multiple_storages_to_the_same_bucket)
{
    givenAddedBucket();

    whenAddMultipleStorages(2);

    thenRequestIsForwardedToCloudDb();
    thenAddStorageResponseIs(ResultCode::ok);

    andBucketHasCloudStorageCountUpdated(2);
    andStorageServiceHasMultipleStorages();
}

TEST_F(StorageApi, fails_to_add_storage_with_unknown_region)
{
    whenAddStorageWithUnknownRegion();

    thenRequestIsForwardedToCloudDb();
    thenAddStorageResponseIs(ResultCode::notFound);
}

TEST_F(StorageApi, rejects_add_storage_with_invalid_storage_size)
{
    givenAddedBucket();

    whenAddStorageWithInvalidSize();

    thenAddStorageResponseIs(ResultCode::badRequest);
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

