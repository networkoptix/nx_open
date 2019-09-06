#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/storage/service/test_support/cloud_storage_launcher.h>
#include <nx/cloud/storage/service/test_support/s3_bucket.h>
#include <nx/geo_ip/test_support/memory_resolver.h>
#include <nx/cloud/storage/service/api/client.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/storage/service/controller/geo_ip_resolver.h>

namespace nx::cloud::storage::service::test {

class BucketApi:
    public testing::Test
{
public:
    ~BucketApi();

protected:
    struct Arg
    {
        std::string name;
        std::string value;
    };

    virtual void SetUp() override;

    void setCredentials(
        const std::string& accessKeyId,
        const std::string& secretAccessKey);

    void addArg(const std::string& name, const std::string& value);

    void givenExistingBucket();
    service::test::S3Bucket* givenAddedBucket(
        std::string region = {},
        std::string name = {},
        bool local = true);
    void whenAddBucket(std::string bucketName = {});
    void whenAddUnknownBucket();
    void whenListBuckets();
    void whenRemoveBucket(std::string bucketName = {});
    void thenAddBucketResponseIs(api::ResultCode resultCode);
	void thenAddBucketFails();
    void thenListBucketsSucceeds();
    void thenRemoveBucketSucceeds();
    void andAddedBucketMatchesExpectedBucket();
    void andListedBucketsContainsAddedBucket();
    void andBucketIsNotInService();

private:
    service::test::S3Bucket* createBucket(
        std::string region = {},
        std::string name = {},
        bool local = true);
    void addBucket(const api::AddBucketRequest& request);

protected:
    nx::geo_ip::test::MemoryResolver* m_geoIpResolver = nullptr;
    std::unique_ptr<api::Client> m_cloudStorageClient;
    service::test::S3Bucket* m_lastBucketCreated = nullptr;
    std::unique_ptr<service::test::CloudStorageLauncher> m_cloudStorage;
    api::Bucket m_lastBucketAdded;
    api::Buckets m_lastBucketsListed;

private:
    std::map<std::string, std::string>m_args;
    std::map<std::string, std::unique_ptr<service::test::S3Bucket>> m_buckets;
    nx::network::http::Credentials m_credentials;
    nx::utils::SyncQueue<std::pair<api::Result, api::Bucket>> m_addBucketResponse;
    nx::utils::SyncQueue<std::pair<api::Result, api::Buckets>> m_listBucketsResponse;
    nx::utils::SyncQueue<api::Result> m_removeBucketResponse;
    controller::GeoIpResolverFactory::Function m_geoIpFactoryFuncBak;
};

} // namespace namespace nx::cloud::storage::service::api::test