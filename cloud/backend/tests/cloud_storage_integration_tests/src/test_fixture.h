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
    struct TestContext
    {
        S3Bucket* s3Bucket = nullptr;
        nx::cloud::db::AccountWithPassword cloudDbAccount;
        nx::cloud::db::api::SystemData system;
        api::Storage storage;
        std::unique_ptr<api::Client> storageServiceClient;
    };

    virtual void SetUp() override;

    /**
     * Does the following:
     * - Launches an S3 bucket
     * - Adds an activated account to CloudDb
     * - Launches a Cloud Storage Service instance
     * - Prepares a Cloud Storage Service api client with CloudDb credentials
     * - Adds the S3 bucket to the Cloud Storage Service
     * - Creates a Cloud Storage.
     * - Associates the CloudDb system with the Cloud Storage
     */
    TestContext initializeTest();

    nx::cloud::db::CdbLauncher& cloudDb();
    const nx::cloud::db::CdbLauncher& cloudDb() const;
    CloudStorageLauncher& cloudStorage(int index = 0);
    const CloudStorageLauncher& cloudStorage(int index = 0) const;

private:
    S3Bucket& launchS3Bucket();
    CloudStorageLauncher& launchCloudStorageService();

    void addBucket(TestContext* outTestContext);
    void addStorage(TestContext* outTestContext);
    void addSystem(TestContext* outTestContext);

private:
    controller::GeoIpResolverFactory::Function m_geoIpResolverFactoryFuncBak;
    aws::sts::test::AwsStsEmulator m_sts;
    std::map<std::string /*bucketName*/, std::unique_ptr<S3Bucket>> m_s3Buckets;
    discovery::test::DiscoveryServer m_discoveryServer;
    std::unique_ptr<CloudStorageCluster> m_cloudStorageCluster;
    nx::cloud::db::CdbLauncher m_cloudDb;
};

} // namespace nx::cloud::storage::service::test