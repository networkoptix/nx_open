#include "bucket_api_ut.h"

#include <nx/cloud/storage/service/settings.h>

namespace nx::cloud::storage::service::test {

using namespace nx::cloud::storage::service::api;

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
            [this](auto&& ... /*args*/)
            {
                auto resolver = std::make_unique<nx::geo_ip::test::MemoryResolver>();
                m_geoIpResolver = resolver.get();
                return resolver;
            });

    m_cloudStorage = std::make_unique<service::test::CloudStorageLauncher>();

    m_credentials.username = nx::utils::generateRandomName(7);
    m_credentials.authToken.setPassword(nx::utils::generateRandomName(7));

    m_cloudStorage->addArg("-aws/accessKeyId", m_credentials.username.toUtf8().constData());
    m_cloudStorage->addArg("-aws/secretAccessKey", m_credentials.authToken.value.constData());

    for (const auto& arg : m_args)
    {
        auto name = arg.first;
        while (name[0] == '-')
            name.erase(0, 1);
        m_cloudStorage->removeArgByName(name.c_str());
        m_cloudStorage->addArg(arg.first.c_str(), arg.second.c_str());
    }

    ASSERT_TRUE(m_cloudStorage->startAndWaitUntilStarted());

    m_cloudStorageClient = std::make_unique<Client>(m_cloudStorage->httpUrl());
    m_cloudStorageClient->setRequestTimeout(std::chrono::milliseconds(0));
}

void BucketApi::setCredentials(
    const std::string& accessKeyId,
    const std::string& secretAccessKey)
{
    addArg("-aws/accessKeyId", accessKeyId);
    addArg("-aws/secretAccessKey", secretAccessKey);
}

void BucketApi::addArg(const std::string& name, const std::string& value)
{
    m_args[name] = value;
}

void BucketApi::givenExistingBucket()
{
    createBucket();
}

service::test::S3Bucket* BucketApi::givenAddedBucket(std::string region, std::string name, bool local)
{
    auto bucket = createBucket(region, name, local);
    whenAddBucket(bucket->name());

    thenAddBucketResponseIs(ResultCode::ok);
    andAddedBucketMatchesExpectedBucket();
    return bucket;
}

void BucketApi::whenAddBucket(std::string bucketName)
{
    if (bucketName.empty())
        bucketName = m_lastBucketCreated->name();

    addBucket({bucketName});
}

void BucketApi::whenAddUnknownBucket()
{
    addBucket({"bucket-unknown"});
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

void BucketApi::thenAddBucketResponseIs(ResultCode resultCode)
{
    auto [result, bucket] = m_addBucketResponse.pop();
    ASSERT_EQ(resultCode, result.resultCode);
    m_lastBucketAdded = std::move(bucket);
}

void BucketApi::thenAddBucketFails()
{
	auto [result, bucket] = m_addBucketResponse.pop();
	ASSERT_FALSE(result.ok());
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


service::test::S3Bucket* BucketApi::createBucket(
    std::string region,
    std::string name,
    bool local)
{
    if (region.empty())
        region = aws::s3::test::randomS3Location();

    if (name.empty())
        name = lm("bucket-%1").arg(m_buckets.size()).toStdString();
    m_buckets[name] =
        std::make_unique<service::test::S3Bucket>(region, name, local);

    if (local)
        m_buckets[name]->enableAthentication(std::regex(".*"), m_credentials);

    m_lastBucketCreated = m_buckets[name].get();

    return m_lastBucketCreated;
}

void BucketApi::addBucket(const AddBucketRequest& request)
{
    m_cloudStorageClient->addBucket(
        request,
        [this](auto result, auto bucket)
        {
            m_addBucketResponse.push({std::move(result), std::move(bucket)});
        });
}

TEST_F(BucketApi, add_bucket)
{
    givenExistingBucket();

    whenAddBucket();

    thenAddBucketResponseIs(ResultCode::ok);

    andAddedBucketMatchesExpectedBucket();
}

TEST_F(BucketApi, fails_to_add_unknown_bucket)
{
    whenAddUnknownBucket();

	thenAddBucketFails();
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

} // namespace nx::cloud::storage::service::test