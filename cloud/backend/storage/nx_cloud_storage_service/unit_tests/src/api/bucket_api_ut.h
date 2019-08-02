#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/storage/service/test_support/cloud_storage_launcher.h>
#include <nx/geo_ip/test_support/memory_resolver.h>
#include <nx/network/cloud/storage/service/api/client.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/storage/service/controller/geo_ip_resolver.h>
#include <nx/cloud/storage/service/view/http/cloud_db_authentication_manager.h>

#include "../s3_bucket.h"

namespace nx::cloud::storage::service::api::test {


class BucketApi:
    public testing::Test
{
public:
    ~BucketApi();

protected:
    virtual void SetUp() override;

    void givenExistingBucket();
    void givenAddedBucket();
    void whenAddBucket(std::string bucketName = {});
    void whenListBuckets();
    void whenRemoveBucket(std::string bucketName = {});
    void thenAddBucketSucceeds();
    void thenListBucketsSucceeds();
    void thenRemoveBucketSucceeds();
    void andAddedBucketMatchesExpectedBucket();
    void andListedBucketsContainsAddedBucket();
    void andBucketIsNotInService();

private:
    service::test::S3Bucket* createBucket();

protected:
    std::unique_ptr<Client> m_cloudStorageClient;
    service::test::S3Bucket* m_lastBucketCreated = nullptr;
    std::unique_ptr<service::test::CloudStorageLauncher> m_cloudStorage;
    Bucket m_lastBucketAdded;
    Buckets m_lastBucketsListed;

private:
    std::map<std::string, std::unique_ptr<service::test::S3Bucket>> m_buckets;
    nx::network::http::Credentials m_credentials;
    std::string m_lastBucketRemoved;
    nx::utils::SyncQueue<std::pair<Result, Bucket>> m_addBucketResponse;
    nx::utils::SyncQueue<std::pair<Result, Buckets>> m_listBucketsResponse;
    nx::utils::SyncQueue<Result> m_removeBucketResponse;
    controller::GeoIpResolverFactory::Function m_geoIpFactoryFuncBak;
};

} // namespace namespace nx::cloud::storage::service::api::test