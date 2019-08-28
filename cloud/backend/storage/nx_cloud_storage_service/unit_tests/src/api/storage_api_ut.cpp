#include "bucket_api_ut.h"

#include <nx/cloud/aws/test_support/aws_sts_emulator.h>
#include <nx/cloud/db/api/cdb_client.h>
#include <nx/cloud/storage/service/settings.h>
#include <nx/cloud/storage/service/controller/cloud_db/authentication_manager.h>
#include <nx/cloud/storage/service/test_support/cloud_db_emulator.h>
#include <nx/network/url/url_builder.h>

#include "nx/cloud/aws/s3/api_client.h"

namespace nx::cloud::storage::service::api::test {

namespace{

static constexpr char kFileContents[] = "hello world";

class CloudDBAuthenticationForwarderStub:
    public service::controller::cloud_db::AuthenticationForwarder
{
    using base_type = controller::cloud_db::AuthenticationForwarder;
public:
    CloudDBAuthenticationForwarderStub(
        const conf::CloudDb& settings,
        nx::utils::SyncQueue<bool>* authenticationEvent)
        :
        base_type(settings),
        m_authenticationEvent(authenticationEvent)
    {
    }

    virtual void authenticate(
        const network::http::HttpServerConnection& connection,
        const network::http::Request& request,
        network::http::server::AuthenticationCompletionHandler completionHandler) override
    {
        m_authenticationEvent->push(true);
        base_type::authenticate(connection, request, std::move(completionHandler));
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
            controller::cloud_db::AuthenticationManagerFactory::instance().setCustomFunc(
                std::move(m_cloudDBAuthenticationFactoryFuncBak));
        }

    }

protected:
    virtual void SetUp()override
    {
        m_cloudDBAuthenticationFactoryFuncBak =
            controller::cloud_db::AuthenticationManagerFactory::instance().setCustomFunc(
                [this](const auto& settings)
                {
                    return std::make_unique<CloudDBAuthenticationForwarderStub>(
                        settings.cloudDb(),
                        &m_cloudDbAuthenticationEvent);
                });

        ASSERT_TRUE(m_sts.bindAndListen());
        ASSERT_TRUE(m_cloudDb.bindAndListen());

        addArg("-aws/assumeRoleArn", "arn:aws:iam::ACCOUNTID:role/sts-assumeRole-testArn");
        addArg("-aws/stsUrl", m_sts.url().toStdString());

        addArg("-cloud_db/url", m_cloudDb.url().toStdString());

        base_type::SetUp();
    }

    void givenCloudDbAccount()
    {
        using namespace nx::network::http;

        m_expectedCloudDbOwner = "owner1";
        Credentials credentials1(
            m_expectedCloudDbOwner.c_str(),
            PasswordAuthToken("owner1Password"));
        m_cloudDb.addAccount(m_expectedCloudDbOwner.c_str(), credentials1).addSystem("system1");

        auto cloudStorageUrl = m_cloudStorage->httpUrl();
        cloudStorageUrl.setUserName(m_expectedCloudDbOwner.c_str());
        cloudStorageUrl.setPassword("owner1Password");

        m_cloudStorageClient = std::make_unique<Client>(cloudStorageUrl);
        m_cloudStorageClient->setRequestTimeout(std::chrono::milliseconds(0));
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
        andAddedStorageHasExpectedOwner();

        const auto& prefix = m_lastStorageAdded.id;
        m_lastBucketCreated->saveOrReplaceFile(prefix + "/f.txt", kFileContents);
    }

    void givenAddedSystem(const std::string& systemId = std::string())
    {
        whenAddSystem(systemId);
        thenAddSystemResponseIs(ResultCode::ok);
        andAddedSystemHasExpectedData();
    }

    void givenMultipleAddedSystems()
    {
        for (int i = 0; i < 2; ++i)
        {
            m_expectedSystems.emplace_back(std::string("system-") + std::to_string(i));
            givenAddedSystem(m_expectedSystems.back());
        }
    }

    void whenAddStorageWithRegion(std::string region = {})
    {
        if (region.empty())
            region = m_lastBucketCreated->location();

        addStorage({1000, region});
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

    void whenGetCredentials()
    {
        m_cloudStorageClient->getCredentials(
            m_lastStorageAdded.id,
            [this](auto result, auto credentials)
            {
                m_getCredentialsResponse.push({std::move(result), std::move(credentials)});
            });
    }

    void whenAddSystem(const std::string& systemId = std::string())
    {
        m_expectedSystem = systemId;
        if (m_expectedSystem.empty())
            m_expectedSystem = "some-system";
        addSystem(m_lastStorageAdded.id, {m_expectedSystem});
    }

    void whenRemoveSystem()
    {
        m_cloudStorageClient->removeSystem(
            m_lastSystemAdded.storageId,
            m_lastSystemAdded.id,
            [this](auto result)
            {
                m_removeSystemResponse.push(std::move(result));
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

    void thenAllStoragesAreAdded(int count)
    {
        for(int i = 0; i < count; ++i)
            thenAddStorageResponseIs(ResultCode::ok);
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

    void thenGetCredentialsSucceeds()
    {
        auto [result, credentials] = m_getCredentialsResponse.pop();
        ASSERT_EQ(api::ResultCode::ok, result.resultCode);
        m_lastCredentialsGotten = std::move(credentials);
    }

    void andRoleIsAssumed()
    {
        auto assumeRoleResult = m_sts.getAssumedRole(m_lastCredentialsGotten.login);
        ASSERT_NE(std::nullopt, assumeRoleResult);
        ASSERT_EQ(assumeRoleResult->credentials.accessKeyId, m_lastCredentialsGotten.login);
        ASSERT_EQ(assumeRoleResult->credentials.secretAccessKey, m_lastCredentialsGotten.password);
        ASSERT_FALSE(m_lastCredentialsGotten.urls.empty());

        auto url = m_lastCredentialsGotten.urls.front().toStdString();
        ASSERT_NE(url.find(m_lastStorageAdded.id), std::string::npos);
    }

    void andS3IsAcessibleOnlyThroughStorageCredentials()
    {
        aws::Credentials credentials(
            m_lastCredentialsGotten.login.c_str(),
            network::http::PasswordAuthToken(m_lastCredentialsGotten.password.c_str()),
            m_lastCredentialsGotten.sessionToken.c_str());

        const auto doTest =
            [&credentials](const nx::utils::Url& url, nx::cloud::aws::ResultCode expectedResultCode)
        {
            auto client = std::make_unique<nx::cloud::aws::s3::ApiClient>(
                "us-west-1",
                url,
                credentials);

            nx::utils::SyncQueue<aws::Result> done;
            client->uploadFile(
                "test.txt",
                "testdata",
                [&done, &client](auto awsResult)
                {
                    done.push(awsResult);
                    client->deleteFile(
                        "test.txt",
                        [&done](auto awsResult) { done.push(awsResult); });
                });

            auto result = done.pop();
            auto result2 = done.pop();
            ASSERT_EQ(expectedResultCode, result.code());
            ASSERT_EQ(expectedResultCode, result.code());
        };

        doTest(m_lastCredentialsGotten.urls.front(), nx::cloud::aws::ResultCode::ok);

        auto url = m_lastCredentialsGotten.urls.front();
        url.setPath("");
        doTest(url, nx::cloud::aws::ResultCode::unauthorized);
    }

    void thenAddSystemResponseIs(ResultCode resultCode)
    {
        auto [result, system] = m_addSystemResponse.pop();
        ASSERT_EQ(resultCode, result.resultCode);
        m_lastSystemAdded = std::move(system);
    }

    void thenRemoveStorageResponseIs(ResultCode resultCode)
    {
        auto result = m_removeSystemResponse.pop();
        ASSERT_EQ(resultCode, result.resultCode);
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

    void andAddedStorageHasExpectedOwner()
    {
        ASSERT_EQ(m_lastStorageAdded.owner, m_expectedCloudDbOwner);
    }

    void andStorageServiceHasMultipleStorages()
    {
        ASSERT_TRUE((int) m_addedStorages.size() > 1);
        for (const auto& storage : m_addedStorages)
        {
            readStorage(storage.id);
            thenReadStorageSucceeds();
            ASSERT_EQ(storage.id, m_lastStorageRead.id);
            ASSERT_EQ(storage.owner, m_lastStorageRead.owner);
        }
    }

    void andReadStorageContainsAddedStorage()
    {
        auto readStorage = m_lastStorageRead;
        readStorage.freeSpace = m_lastStorageAdded.freeSpace;
        ASSERT_EQ(m_lastStorageAdded, readStorage);
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

    void andAddedSystemHasExpectedData()
    {
        ASSERT_EQ(m_lastSystemAdded.id, m_expectedSystem);
        ASSERT_EQ(m_lastSystemAdded.storageId, m_lastStorageAdded.id);
    }

    void andSystemIsAddedToStorage()
    {
        readStorage(m_lastSystemAdded.storageId);
        thenReadStorageSucceeds();

        const auto& systems = m_lastStorageRead.systems;
        auto it = std::find(systems.begin(), systems.end(), m_lastSystemAdded.id);
        ASSERT_NE(it, systems.end());
    }

    void andSystemIsRemovedFromStorage()
    {
        readStorage(m_lastSystemAdded.storageId);
        thenReadStorageSucceeds();

        const auto& systems = m_lastStorageRead.systems;
        auto it = std::find(systems.begin(), systems.end(), m_lastSystemAdded.id);
        ASSERT_EQ(it, systems.end());
    }

    void andReadStorageContainsExpectedSystems()
    {
        const auto& systems = m_lastStorageRead.systems;
        for (const auto& system : m_expectedSystems)
        {
            auto it = std::find(systems.begin(), systems.end(), system);
            ASSERT_NE(it, systems.end());
        }
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

    void addSystem(const std::string& storageId, const AddSystemRequest& request)
    {
        m_cloudStorageClient->addSystem(
            storageId,
            request,
            [this](auto result, auto system)
            {
                m_addSystemResponse.push({std::move(result), std::move(system)});
            });
    }

private:
    nx::cloud::aws::sts::test::AwsStsEmulator m_sts;
    service::test::CloudDbEmulator m_cloudDb;
    std::string m_expectedCloudDbOwner;
    nx::utils::SyncQueue<std::pair<Result, Storage>> m_addStorageResponse;
    nx::utils::SyncQueue<std::pair<Result, Storage>> m_readStorageResponse;
    nx::utils::SyncQueue<Result> m_removeStorageResponse;
    nx::utils::SyncQueue<std::pair<Result, StorageCredentials>>m_getCredentialsResponse;
    nx::utils::SyncQueue<std::pair<Result, System>>m_addSystemResponse;
    nx::utils::SyncQueue<Result> m_removeSystemResponse;
    Storage m_lastStorageAdded;
    std::vector<Storage> m_addedStorages;
    Storage m_lastStorageRead;
    Storage m_lastStorageRemoved;
    StorageCredentials m_lastCredentialsGotten;
    std::string m_expectedSystem;
    std::vector<std::string> m_expectedSystems;
    System m_lastSystemAdded;
    service::test::S3Bucket* m_expectedBucket = nullptr;

    nx::utils::SyncQueue<bool> m_cloudDbAuthenticationEvent;
    controller::cloud_db::AuthenticationManagerFactory::Function
        m_cloudDBAuthenticationFactoryFuncBak;
};

TEST_F(StorageApi, add_storage_by_region)
{
    givenCloudDbAccount();
    givenAddedBucket();

    whenAddStorageWithRegion();

    thenRequestIsForwardedToCloudDb();
    thenAddStorageResponseIs(ResultCode::ok);

    andBucketHasCloudStorageCountUpdated(1);
    andAddedStorageHasExpectedOwner();
}

TEST_F(StorageApi, add_storage_by_client_location)
{
    givenCloudDbAccount();
    givenMultipleAddedBuckets();

    whenAddStorageWithoutRegion();

    thenAddStorageResponseIs(ResultCode::ok);

    andBucketChosenForStorageIsClosestToClient();
    andAddedStorageHasExpectedOwner();
}

TEST_F(StorageApi, add_multiple_storages_to_the_same_bucket)
{
    givenCloudDbAccount();
    givenAddedBucket();

    whenAddMultipleStorages(2);

    thenRequestIsForwardedToCloudDb();
    thenAllStoragesAreAdded(2);

    andBucketHasCloudStorageCountUpdated(2);
    andStorageServiceHasMultipleStorages();
}

TEST_F(StorageApi, fails_to_add_storage_with_unknown_region)
{
    givenCloudDbAccount();
    //Intentionally missing added bucket
    whenAddStorageWithUnknownRegion();

    thenRequestIsForwardedToCloudDb();
    thenAddStorageResponseIs(ResultCode::notFound);
}

TEST_F(StorageApi, rejects_add_storage_with_invalid_storage_size)
{
    givenCloudDbAccount();
    givenAddedBucket();

    whenAddStorageWithInvalidSize();

    thenAddStorageResponseIs(ResultCode::badRequest);
}

TEST_F(StorageApi, read_storage)
{
    givenCloudDbAccount();
    givenAddedBucket();
    givenAddedStorage();

    whenReadStorage();

    thenRequestIsForwardedToCloudDb();
    thenReadStorageSucceeds();

    andReadStorageContainsAddedStorage();
}

TEST_F(StorageApi, remove_storage)
{
    givenCloudDbAccount();
    givenAddedBucket();
    givenAddedStorage();

    whenRemoveStorage();

    thenRequestIsForwardedToCloudDb();
    thenRemoveStorageSucceeds();

    andStorageIsNotInService();
}

TEST_F(StorageApi, get_credentials)
{
    givenCloudDbAccount();
    givenAddedBucket();
    givenAddedStorage();

    whenGetCredentials();

    thenGetCredentialsSucceeds();
    andRoleIsAssumed();
}

TEST_F(StorageApi, DISABLED_get_credentials_using_actual_aws_services)
{
    // To use this test suite:
    // 1. SetUp() must NOT provide "-aws/stsUrl" setting
    // 2. SetUp() must provide a valid aws arn via "-aws/assumeRoleArn" setting
    // 3. SetUp() must provide valid aws credentials via setCredentials() helper function
    // 4. givenAddedBucket() below must provide a real bucket
    //    like givenAddedBucket("us-east-1", "actual-bucket-name", false);
    //    "actual-bucket-name" must be a valid bucket in the region given by first param

    givenCloudDbAccount();
    givenAddedBucket("some-aws-region", "actual-bucket-name", false);
    givenAddedStorage();

    whenGetCredentials();

    thenGetCredentialsSucceeds();
    andS3IsAcessibleOnlyThroughStorageCredentials();
}

TEST_F(StorageApi, add_system_storage_relation)
{
    givenCloudDbAccount();
    givenAddedBucket();
    givenAddedStorage();

    whenAddSystem();
    thenRequestIsForwardedToCloudDb();

    thenAddSystemResponseIs(ResultCode::ok);

    andAddedSystemHasExpectedData();
    andSystemIsAddedToStorage();
}

TEST_F(StorageApi, remove_system_storage_relation)
{
    givenCloudDbAccount();
    givenAddedBucket();
    givenAddedStorage();
    givenAddedSystem();

    whenRemoveSystem();
    thenRequestIsForwardedToCloudDb();

    thenRemoveStorageResponseIs(ResultCode::ok);

    andSystemIsRemovedFromStorage();
}

TEST_F(StorageApi, read_storage_lists_multiple_systems)
{
    givenCloudDbAccount();
    givenAddedBucket();
    givenAddedStorage();
    givenMultipleAddedSystems();

    whenReadStorage();
    thenReadStorageSucceeds();

    andReadStorageContainsExpectedSystems();
}

} // namespace nx::cloud::storage::service::api::test

