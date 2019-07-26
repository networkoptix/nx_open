#include "bucket_manager.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/cloud/aws/api_client.h>

#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::controller {

using namespace std::placeholders;

namespace {

static nx::utils::Url makeBucketUrl(const std::string& bucketName)
{
    static constexpr char kBucketUrlTemplate[] = "https://%1.s3.amazonaws.com";
    return lm(kBucketUrlTemplate).arg(bucketName).toQString();
}

} // namespace

BucketManager::BucketManager(const conf::Aws& /*settings*/)
{
}

void BucketManager::addBucket(
    const api::AddBucketRequest& /*request*/,
    const nx::utils::MoveOnlyFunc<void(api::Result, api::Bucket)> /*handler*/)
{
}

void BucketManager::listBuckets(
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::Bucket>)> /*handler*/)
{
}

void BucketManager::removeBucket(
    const std::string& /*bucketName*/,
    nx::utils::MoveOnlyFunc<void(api::Result)> /*handler*/)
{
}

} // namespace nx::cloud::storage::service::controller
