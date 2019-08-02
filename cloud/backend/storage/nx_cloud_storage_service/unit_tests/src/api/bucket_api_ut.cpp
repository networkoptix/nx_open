#include "bucket_api_ut.h"

namespace nx::cloud::storage::service::api::test {

BucketApi::~BucketApi()
{
    if (m_geoIpFactoryFuncBak)
    {
        controller::GeoIpResolverFactory::instance().setCustomFunc(
            std::move(m_geoIpFactoryFuncBak));
    }
}

void BucketApi::SetUp()
{
    m_geoIpFactoryFuncBak =
        controller::GeoIpResolverFactory::instance().setCustomFunc(
            [](auto&& ... /*args*/)
            {
                return std::make_unique<nx::geo_ip::test::MemoryResolver>();
            });

    m_credentials.username = nx::utils::generateRandomName(7);
    m_credentials.authToken.setPassword(nx::utils::generateRandomName(7));

    m_cloudStorage = std::make_unique<service::test::CloudStorageLauncher>();
    m_cloudStorage->addArg("-aws/userName", m_credentials.username.toUtf8().constData());
    m_cloudStorage->addArg("-aws/authToken", m_credentials.authToken.value.constData());
    ASSERT_TRUE(m_cloudStorage->startAndWaitUntilStarted());

    m_cloudStorageClient = std::make_unique<Client>(m_cloudStorage->httpUrl());
    m_cloudStorageClient->setRequestTimeout(std::chrono::milliseconds(0));
}

void BucketApi::givenExistingBucket()
{
    createBucket();
}

void BucketApi::givenAddedBucket()
{
    givenExistingBucket();
    whenAddBucket();
    thenAddBucketSucceeds();
    andAddedBucketMatchesExpectedBucket();
}

void BucketApi::whenAddBucket(std::string bucketName)
{
    if (bucketName.empty())
        bucketName = m_lastBucketCreated->name();

    m_cloudStorageClient->addBucket(
        AddBucketRequest{bucketName},
        [this](auto result, auto bucket)
        {
            m_addBucketResponse.push({std::move(result), std::move(bucket)});
        });
}

void BucketApi::whenListBuckets()
{
    m_cloudStorageClient->listBuckets(
        [this](auto result, auto buckets)
        {
            m_listBucketsResponse.push({std::move(result), std::move(buckets)});
        });
}

void BucketApi::whenRemoveBucket(std::string bucketName)
{
    if (bucketName.empty())
        bucketName = m_lastBucketCreated->name();

    m_cloudStorageClient->removeBucket(
        bucketName,
        [this](auto result)
        {
            m_removeBucketResponse.push(std::move(result));
        });
}

void BucketApi::thenAddBucketSucceeds()
{
    auto [result, bucket] = m_addBucketResponse.pop();
    ASSERT_EQ(ResultCode::ok, result.resultCode);
    m_lastBucketAdded = std::move(bucket);
}

void BucketApi::thenListBucketsSucceeds()
{
    auto [result, buckets] = m_listBucketsResponse.pop();
    ASSERT_EQ(ResultCode::ok, result.resultCode);
    m_lastBucketsListed = std::move(buckets);
}

void BucketApi::thenRemoveBucketSucceeds()
{
    auto result = m_removeBucketResponse.pop();
    ASSERT_EQ(ResultCode::ok, result.resultCode);
}

void BucketApi::andAddedBucketMatchesExpectedBucket()
{
    ASSERT_EQ(m_lastBucketAdded.name, m_lastBucketCreated->name());
    ASSERT_EQ(m_lastBucketAdded.region, m_lastBucketCreated->location());
}

void BucketApi::andListedBucketsContainsAddedBucket()
{
    auto it = std::find(
        m_lastBucketsListed.buckets.begin(),
        m_lastBucketsListed.buckets.end(),
        m_lastBucketAdded);
    ASSERT_NE(it, m_lastBucketsListed.buckets.end());
}

void BucketApi::andBucketIsNotInService()
{
    whenListBuckets();
    thenListBucketsSucceeds();

    auto it = std::find(
        m_lastBucketsListed.buckets.begin(),
        m_lastBucketsListed.buckets.end(),
        m_lastBucketAdded);
    ASSERT_EQ(it, m_lastBucketsListed.buckets.end());
}


service::test::S3Bucket* BucketApi::createBucket()
{
    std::string bucketName = lm("bucket-%1").arg(m_buckets.size()).toStdString();
    m_buckets[bucketName] =
        std::make_unique<service::test::S3Bucket>(bucketName, aws::test::randomS3Location());

    m_buckets[bucketName]->enableAthentication(std::regex(".*"), m_credentials);

    m_lastBucketCreated = m_buckets[bucketName].get();

    return m_lastBucketCreated;
}

TEST_F(BucketApi, add_bucket)
{
    givenExistingBucket();

    whenAddBucket();

    thenAddBucketSucceeds();

    andAddedBucketMatchesExpectedBucket();
}


TEST_F(BucketApi, list_buckets)
{
    givenAddedBucket();

    whenListBuckets();

    thenListBucketsSucceeds();

    andListedBucketsContainsAddedBucket();
}


TEST_F(BucketApi, remove_bucket)
{
    givenAddedBucket();

    whenRemoveBucket();

    thenRemoveBucketSucceeds();

    andBucketIsNotInService();
}

} // namespace nx::cloud::storage::service::test::api