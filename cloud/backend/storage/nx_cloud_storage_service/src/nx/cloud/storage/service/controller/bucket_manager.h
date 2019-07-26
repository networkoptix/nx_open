#pragma once

#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/add_bucket.h>
#include <nx/network/cloud/storage/service/api/bucket.h>
#include <nx/utils/move_only_func.h>

namespace nx::cloud {
namespace storage::service {

namespace conf {

struct Aws;
class Settings;

} // namespace conf

namespace controller {

class BucketManager
{
public:
    BucketManager(const conf::Settings& settings, model::Model* model);

   void addBucket(
        const api::AddBucketRequest& request,
        const nx::utils::MoveOnlyFunc<void(api::Result, api::Bucket)> handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, api::Buckets)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);
};

} // namespace controller
} // namespace storage::service
} // namespace nx::cloud

