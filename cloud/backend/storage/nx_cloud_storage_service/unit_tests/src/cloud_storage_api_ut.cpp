#include <gtest/gtest.h>

#include <nx/cloud/storage/service/test_support/cloud_storage_launcher.h>
#include <nx/network/cloud/storage/service/api/client.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/storage/service/view/http/cloud_db_authentication_manager.h>

#include "s3_bucket.h"

namespace nx::cloud::storage::service::test {

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

class CloudStorageApi:
    public testing::Test
{
public:
    ~CloudStorageApi()
    {
        if (m_cloudDBAuthenticationFactoryFuncBak)
        {
            view::http::CloudDbAuthenticationFactory::instance().setCustomFunc(
                std::move(m_cloudDBAuthenticationFactoryFuncBak));
        }
    }

protected:
    virtual void SetUp() override
    {
        m_cloudDBAuthenticationFactoryFuncBak =
            view::http::CloudDbAuthenticationFactory::instance().setCustomFunc(
                [this](const conf::Settings& /*settings*/)
                {
                    return std::make_unique<CloudDBAuthenticationForwarderStub>(
                        &m_authenticationEvent);
                });

        m_credentials.username = nx::utils::generateRandomName(7);
        m_credentials.authToken.setPassword(nx::utils::generateRandomName(7));

        m_cloudStorage = std::make_unique<CloudStorageLauncher>();
        m_cloudStorage->addArg("-aws/userName", m_credentials.username.toUtf8().constData());
        m_cloudStorage->addArg("-aws/authToken", m_credentials.authToken.value.constData());
        ASSERT_TRUE(m_cloudStorage->startAndWaitUntilStarted());

        m_cloudStorageClient = std::make_unique<api::Client>(m_cloudStorage->httpUrl());
        m_cloudStorageClient->setRequestTimeout(std::chrono::milliseconds(0));
    }

    void givenExistingBucket()
    {
        createBucket();
    }

    void givenAddedBucket()
    {
        createBucket();
        whenAddBucket();
        thenRequestSucceeds();
    }

    void whenAddBucket(std::string bucketName = {})
    {
        if (bucketName.empty())
            bucketName = m_lastBucketCreated->name();

        m_cloudStorageClient->addBucket(
            api::AddBucketRequest{bucketName},
            [this, bucketName](api::Client::ResultCode resultCode, api::Bucket bucket)
            {
                ASSERT_EQ(bucketName, bucket.name);
                m_addedBucket = std::move(bucket);
                m_response.push(std::move(resultCode));
            });
    }

    void whenListBuckets()
    {
        m_cloudStorageClient->listBuckets(
            [this](api::Client::ResultCode result, api::Buckets buckets)
            {
                ASSERT_EQ(m_addedBucket, buckets.buckets.front());
                m_response.push(std::move(result));
            });
    }

    void whenRemoveBucket(std::string bucketName = {})
    {
        if (bucketName.empty())
            bucketName = m_lastBucketCreated->name();

        m_lastBucketRemoved = bucketName;

        m_cloudStorageClient->removeBucket(
            bucketName,
            [this](auto result)
            {
                m_response.push(result);
            });
    }

    void whenAddStorage(std::string region = {})
    {
        if (region.empty())
            region = m_lastBucketCreated->location();

        m_cloudStorageClient->addStorage({100, region},
            [this](api::Client::ResultCode resultCode, api::Storage response)
            {
                ASSERT_EQ(100, response.totalSpace);
                m_response.push(std::move(resultCode));
            });
    }

    void whenReadStorage()
    {
        m_cloudStorageClient->readStorage(
            "any_storage_id",
            [this](api::Client::ResultCode resultCode, api::Storage response)
            {
                ASSERT_EQ(std::string("any_storage_id"), response.id);
                m_response.push(std::move(resultCode));
            });
    }

    void whenRemoveStorage()
    {
        m_cloudStorageClient->removeStorage(
            "any_storage_id",
            [this](api::Client::ResultCode resultCode)
            {
                m_response.push(std::move(resultCode));
            });
    }

    void thenRequestIsForwardedToCloudDb()
    {
        ASSERT_TRUE(m_authenticationEvent.pop());
    }

    void thenRequestSucceeds()
    {
        ASSERT_EQ(api::ResultCode::ok, m_response.pop().resultCode);
    }

    void andBucketIsNotInService()
    {
        m_cloudStorageClient->listBuckets(
            [this](auto result, auto buckets)
            {
                auto it = std::find_if(buckets.buckets.begin(), buckets.buckets.end(),
                    [this](const auto& bucket)
                    {
                        return bucket.name == m_lastBucketRemoved;
                    });

                ASSERT_TRUE(it == buckets.buckets.end());

                m_response.push(result);
            });

        thenRequestSucceeds();
    }

private:
    S3Bucket* createBucket()
    {
        std::string bucketName = lm("bucket-%1").arg(m_buckets.size()).toStdString();
        m_buckets[bucketName] =
            std::make_unique<S3Bucket>(bucketName, aws::test::randomS3Location());

        m_buckets[bucketName]->enableAthentication(std::regex(".*"), m_credentials);

        m_lastBucketCreated = m_buckets[bucketName].get();

        return m_lastBucketCreated;
    }

private:
    std::map<std::string, std::unique_ptr<S3Bucket>> m_buckets;
    S3Bucket* m_lastBucketCreated = nullptr;
    nx::network::http::Credentials m_credentials;
    std::unique_ptr<CloudStorageLauncher> m_cloudStorage;
    std::unique_ptr<api::Client> m_cloudStorageClient;
    api::Bucket m_addedBucket;
    std::string m_lastBucketRemoved;
    nx::utils::SyncQueue<api::Client::ResultCode> m_response;
    view::http::CloudDbAuthenticationFactory::Function m_cloudDBAuthenticationFactoryFuncBak;
    nx::utils::SyncQueue<bool> m_authenticationEvent;
};

TEST_F(CloudStorageApi, add_bucket)
{
    givenExistingBucket();

    whenAddBucket();
    thenRequestSucceeds();
}


TEST_F(CloudStorageApi, list_buckets)
{
    givenAddedBucket();
    whenListBuckets();
    thenRequestSucceeds();
}


TEST_F(CloudStorageApi, remove_bucket)
{
    givenAddedBucket();
    whenRemoveBucket();
    thenRequestSucceeds();
}

TEST_F(CloudStorageApi, add_storage)
{
    whenAddStorage();
    thenRequestIsForwardedToCloudDb();
    thenRequestSucceeds();
}

TEST_F(CloudStorageApi, read_storage)
{
    whenReadStorage();
    thenRequestIsForwardedToCloudDb();
    thenRequestSucceeds();
}

TEST_F(CloudStorageApi, remove_storage)
{
    whenRemoveStorage();
    thenRequestIsForwardedToCloudDb();
    thenRequestSucceeds();
}

} // namespace nx::cloud::storage::service::test