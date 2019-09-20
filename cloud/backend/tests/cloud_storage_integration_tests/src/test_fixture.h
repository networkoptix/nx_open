#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/aws/test_support/aws_sts_emulator.h>
#include <nx/cloud/storage/service/test_support/s3_bucket.h>
#include <nx/cloud/storage/service/test_support/cloud_storage_cluster.h>
#include <nx/cloud/storage/service/controller/geo_ip_resolver.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>
#include <nx/cloud/db/test_support/cdb_launcher.h>
#include <nx/cloud/storage/service/api/client.h>

namespace nx::cloud::storage::service::test {

class TestFixture:
    public nx::utils::test::TestWithTemporaryDirectory,
    public testing::Test
{
public:
    TestFixture();
    ~TestFixture();

protected:
	class TestContext
    {
	public:
        S3Bucket* s3Bucket = nullptr;
        nx::cloud::db::AccountWithPassword cloudDbAccount;
        nx::cloud::db::api::SystemData system;
        api::Storage storage;
		nx::utils::Url storageServiceUrl;
        std::unique_ptr<api::Client> storageServiceClient;
    };

    virtual void SetUp() override;

    /**
     * Initializes a TestContext with an S3Bucket, Cloud account and
	 * System and a CloudStorageService client with Cloud account credentials.
     */
    TestContext initializeTest();

	/**
	 * Identical to initializeTest except that CloudStorageService client is initialised with
	 * System credentials instead of Cloud account credentials.
	 */
	TestContext initializeTestWithSystemCredentials();

    nx::cloud::db::CdbLauncher& cloudDb();
    const nx::cloud::db::CdbLauncher& cloudDb() const;
    CloudStorageLauncher& storageService(int index = 0);
    const CloudStorageLauncher& storageService(int index = 0) const;

	void addStorage(
		const std::unique_ptr<api::Client>& storageServiceClient,
		const api::AddStorageRequest& request);
	std::pair<api::Result, api::Storage> waitForAddStorageResponse();

	void readStorage(
		const std::unique_ptr<api::Client>& storageServiceClient,
		const std::string& storageId);
	std::pair<api::Result, api::Storage> waitForReadStorageResponse();

	void removeStorage(
		const std::unique_ptr<api::Client>& storageServiceClient,
		const std::string& storageId);
	api::Result waitForRemoveStorageResponse();

	void addSystem(
		const std::unique_ptr<api::Client>& storageServiceClient,
		const std::string& storageId,
		const api::AddSystemRequest& request);
	std::pair<api::Result, api::System> waitForAddSystemResponse();

	void removeSystem(
		const std::unique_ptr<api::Client>& storageServiceClient,
		const std::string& storageId,
		const std::string& systemId);
	api::Result waitForRemoveSystemResponse();

	void requestMediaContentCredentials(
		const std::unique_ptr<api::Client>& storageServiceClient,
		const std::string& storageId);
	std::pair<api::Result, api::StorageCredentials> waitForGetCredentialsResponse();

	std::unique_ptr<api::Client> makeStorageServiceClient(
		const nx::utils::Url& storageServiceUrl,
		const std::string& userName,
		const std::string& password) const;

	void addBucket(TestContext* outTestContext);
	void addStorage(TestContext* outTestContext);
	void addSystem(TestContext* outTestContext);

private:
    S3Bucket& launchS3Bucket();
    CloudStorageLauncher& launchCloudStorageService();

private:
    controller::GeoIpResolverFactory::Function m_geoIpResolverFactoryFuncBak;
    aws::sts::test::AwsStsEmulator m_sts;
    std::map<std::string /*bucketName*/, std::unique_ptr<S3Bucket>> m_s3Buckets;
    discovery::test::DiscoveryServer m_discoveryServer;
    std::unique_ptr<CloudStorageCluster> m_cloudStorageCluster;
    nx::cloud::db::CdbLauncher m_cloudDb;
	nx::utils::SyncQueue<std::pair<api::Result, api::Storage>> m_addStorageResponse;
	nx::utils::SyncQueue<std::pair<api::Result, api::Storage>> m_readStorageResponse;
	nx::utils::SyncQueue<api::Result> m_removeStorageResponse;
	nx::utils::SyncQueue<std::pair<api::Result, api::System>> m_addSystemResponse;
	nx::utils::SyncQueue<api::Result> m_removeSystemResponse;
	nx::utils::SyncQueue<std::pair<api::Result, api::StorageCredentials>> m_getCredentialsResponse;
};

} // namespace nx::cloud::storage::service::test