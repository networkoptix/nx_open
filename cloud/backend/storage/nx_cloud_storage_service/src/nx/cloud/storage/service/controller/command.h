#pragma once

#include <string>

#include <nx/network/buffer.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/clusterdb/engine/command_descriptor.h>

namespace nx::cloud::storage::service::controller {

struct Bucket
{
    std::string name;
    std::string region;
};

#define Bucket_sync_Fields (name)(region)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Bucket),
    (json)(ubjson))

//-------------------------------------------------------------------------------------------------

namespace command {

enum Code
{
    saveBucket = nx::clusterdb::engine::command::kUserCommand + 1,
    removeBucket
};

struct SaveBucket
{
    using Data = Bucket;
    static constexpr int code = Code::saveBucket;
    static constexpr char name[] = "saveBucket";

    static nx::Buffer hash(const Data& data)
    {
        return data.name.c_str();
    }
};

struct RemoveBucket
{
    using Data = std::string;
    static constexpr int code = Code::removeBucket;
    static constexpr char name[] = "removeBucket";

    static nx::Buffer hash(const Data& data)
    {
        return data.c_str();
    }
};

} // namespace command

} // namespace nx::cloud::storage::service::controller