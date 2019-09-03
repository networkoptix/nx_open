#include "test_fixture.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/geo_ip/test_support/memory_resolver.h>

namespace nx::cloud::storage::service::test {

TestFixture::TestFixture():
    m_cloudDb(testDataDir())
{
}

TestFixture::~TestFixture()
{
    controller::GeoIpResolverFactory::instance().setCustomFunc(
        std::move(m_geoIpResolverFactoryFuncBak));
}

void TestFixture::SetUp()
{
    ASSERT_TRUE(m_sts.bindAndListen());
    ASSERT_TRUE(m_discoveryServer.bindAndListen());
    ASSERT_TRUE(m_cloudDb.startAndWaitUntilStarted());
    m_cloudStorageCluster = std::make_unique<CloudStorageCluster>(m_discoveryServer.url());

    m_geoIpResolverFactoryFuncBak = controller::GeoIpResolverFactory::instance().setCustomFunc(
        [](auto& /*settings*/)
        {
            return std::make_unique<nx::geo_ip::test::MemoryResolver>();
        });
}

TestFixture::TestContext TestFixture::initializeTest()
{
    auto cloudDbAccount = m_cloudDb.addActivatedAccount2();
    auto& bucket = launchS3Bucket();
    auto& cloudStorage = launchCloudStorageService();
    auto cloudStorageUrl = cloudStorage.httpUrl();

    cloudStorageUrl.setUserName(cloudDbAccount.email.c_str());
    cloudStorageUrl.setPassword(cloudDbAccount.password.c_str());

    TestContext testContext;
    testContext.cloudDbAccount = std::move(cloudDbAccount);
    testContext.s3Bucket = &bucket;
    testContext.storageServiceClient = std::make_unique<api::Client>(cloudStorageUrl);

    addBucket(&testContext);
    addStorage(&testContext);
    addSystem(&testContext);

    return testContext;
}

nx::cloud::db::CdbLauncher& TestFixture::cloudDb()
{
    return m_cloudDb;
}

const nx::cloud::db::CdbLauncher& TestFixture::cloudDb() const
{
    return m_cloudDb;
}

CloudStorageLauncher& TestFixture::cloudStorage(int index)
{
    return m_cloudStorageCluster->server(index);
}

const CloudStorageLauncher& TestFixture::cloudStorage(int index) const
{
    return m_cloudStorageCluster->server(index);
}

void TestFixture::addBucket(TestContext* outTestContext)
{
    api::AddBucketRequest addBucketRequest;
    addBucketRequest.name = outTestContext->s3Bucket->name();

    std::promise<api::Result> addBucketDone;
    outTestContext->storageServiceClient->addBucket(
        addBucketRequest,
        [this, &addBucketDone](auto result, auto /*bucket*/)
        {
            addBucketDone.set_value(std::move(result));
        });

    ASSERT_TRUE(addBucketDone.get_future().get().ok());
}


void TestFixture::addStorage(TestContext* outTestContext)
{
    api::AddStorageRequest addStorageRequest;
    addStorageRequest.totalSpace = 10000;
    addStorageRequest.region = outTestContext->s3Bucket->location();

    std::promise<std::pair<api::Result, api::Storage>> addStorageDone;
    outTestContext->storageServiceClient->addStorage(
        addStorageRequest,
        [this, &addStorageDone](auto result, auto storage)
        {
            addStorageDone.set_value({std::move(result), std::move(storage)});
        });

    auto [result, storage] = addStorageDone.get_future().get();
    ASSERT_TRUE(result.ok());

    outTestContext->storage = std::move(storage);
}

void TestFixture::addSystem(TestContext* outTestContext)
{
    outTestContext->system = m_cloudDb.addRandomSystemToAccount(outTestContext->cloudDbAccount);

    api::AddSystemRequest addSystemRequest;
    addSystemRequest.id = outTestContext->system.id;

    std::promise<api::Result> addSystemDone;
    outTestContext->storageServiceClient->addSystem(
        outTestContext->storage.id,
        addSystemRequest,
        [this, &addSystemDone](auto result, auto /*system*/)
        {
            addSystemDone.set_value(std::move(result));
        });

    ASSERT_TRUE(addSystemDone.get_future().get().ok());

    outTestContext->storage.systems.emplace_back(outTestContext->system.id);
}

S3Bucket& TestFixture::launchS3Bucket()
{
    std::string bucketName = nx::utils::random::generateName(7).toStdString();
    while (m_s3Buckets.find(bucketName) != m_s3Buckets.end())
        bucketName = nx::utils::random::generateName(7).toStdString();

    return *m_s3Buckets.emplace(
        bucketName,
        std::make_unique<S3Bucket>(aws::s3::test::randomS3Location(), bucketName)).first->second;
}

CloudStorageLauncher& TestFixture::launchCloudStorageService()
{
    auto& cloudStorage = m_cloudStorageCluster->addServer();
    auto cloudDbUrl = network::url::Builder().setScheme(network::http::kUrlSchemeName)
        .setEndpoint(m_cloudDb.endpoint()).toUrl();
    cloudStorage.addArg("-cloud_db/url", cloudDbUrl.toStdString().c_str());

    cloudStorage.addArg("-aws/stsUrl", m_sts.url().toStdString().c_str());
    cloudStorage.addArg("-aws/assumeRoleArn", "arn:aws:iam::12345678:role/assumeRole-testArn");
    // Intentionally skipping aws credentials settings

    NX_ASSERT(cloudStorage.startAndWaitUntilStarted());

    return cloudStorage;
}

TEST_F(TestFixture, launch_services)
{
    initializeTest();
}

} // namespace nx::cloud::storage::service::test