#include "test_fixture.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/geo_ip/test_support/memory_resolver.h>

namespace nx::cloud::storage::service::test {

using namespace nx::cloud::storage::service::api;

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

TestFixture::TestContext TestFixture::initializeTest(int initializationFlags)
{
    auto cloudDbAccount = m_cloudDb.addActivatedAccount2();
    auto& bucket = launchS3Bucket();
    auto& cloudStorage = launchCloudStorageService();
    auto cloudStorageUrl = cloudStorage.httpUrl();

    TestContext testContext;
    testContext.cloudDbAccount = std::move(cloudDbAccount);
    testContext.s3Bucket = &bucket;
	testContext.storageServiceClient =
		makeStorageServiceClient(cloudStorageUrl, testContext.cloudDbAccount);

	if (initializationFlags & InitializeFlags::bucket)
		addBucket(&testContext);
	if (initializationFlags & InitializeFlags::storage)
		addStorage(&testContext);
	if (initializationFlags & InitializeFlags::system)
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

CloudStorageLauncher& TestFixture::storageService(int index)
{
    return m_cloudStorageCluster->server(index);
}

const CloudStorageLauncher& TestFixture::storageService(int index) const
{
    return m_cloudStorageCluster->server(index);
}

void TestFixture::readStorage(Client* storageServiceClient, const std::string& storageId)
{
	storageServiceClient->readStorage(
		storageId,
		[this](auto result, auto storage)
		{
			m_readStorageResponse.push({std::move(result), std::move(storage)});
		});
}

std::pair<Result, Storage> TestFixture::waitForReadStorageResponse()
{
	return m_readStorageResponse.pop();
}

void TestFixture::removeStorage(Client* storageServiceClient, const std::string& storageId)
{
	storageServiceClient->removeStorage(
		storageId,
		[this](auto result)
		{
			m_removeStorageResponse.push(std::move(result));
		});
}

Result TestFixture::waitForRemoveStorageResponse()
{
	return m_removeStorageResponse.pop();
}

void TestFixture::getCredentials(api::Client* cloudStorageClient, const std::string& storageId)
{
	cloudStorageClient->getCredentials(
		storageId,
		[this](auto result, auto storageCredentials)
		{
			m_getCredentialsResponse.push({std::move(result), std::move(storageCredentials)});
		});
}

std::pair<api::Result, api::StorageCredentials> TestFixture::waitForGetCredentialsResponse()
{
	return m_getCredentialsResponse.pop();
}

std::unique_ptr<Client> TestFixture::makeStorageServiceClient(
	const nx::utils::Url& storageSeviceUrl,
	const nx::cloud::db::AccountWithPassword& cloudDbAccount) const
{
	auto url = storageSeviceUrl;
	url.setUserName(cloudDbAccount.email.c_str());
	url.setPassword(cloudDbAccount.password.c_str());

	auto client = std::make_unique<Client>(url);
	client->setRequestTimeout(std::chrono::milliseconds::zero());

	return client;
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