#pragma once

#include <nx/utils/basic_factory.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/add_bucket.h>
#include <nx/network/cloud/storage/service/api/bucket.h>

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
    BucketManager(const conf::Aws& settings);

   void addBucket(
        const api::AddBucketRequest& request,
        const nx::utils::MoveOnlyFunc<void(api::Result, api::Error)> handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::Bucket>)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);
};

} // namespace controller
} // namespace storage::service
} // namespace nx::cloud

