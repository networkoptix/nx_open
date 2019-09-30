#pragma once

#include <string>

#include <nx/clusterdb/engine/command_descriptor.h>
#include <nx/network/buffer.h>
#include <nx/cloud/storage/service/api/bucket.h>
#include <nx/cloud/storage/service/api/storage.h>
#include <nx/cloud/storage/service/api/system.h>

namespace nx::cloud::storage::service::model {

namespace command {

enum class Code
{
    saveBucket = nx::clusterdb::engine::command::kUserCommand + 1,
    removeBucket,
    saveStorage,
    removeStorage,
    saveSystem,
    removeSystem,
};

struct SaveBucket
{
    using Data = api::Bucket;
    static constexpr int code = (int) Code::saveBucket;
    static constexpr char name[] = "saveBucket";

    static nx::Buffer hash(const Data& data)
    {
        return data.name.c_str();
    }
};

struct RemoveBucket
{
    using Data = std::string;
    static constexpr int code = (int) Code::removeBucket;
    static constexpr char name[] = "removeBucket";

    static nx::Buffer hash(const Data& data)
    {
        return data.c_str();
    }
};

struct SaveStorage
{
    using Data = api::Storage;
    static constexpr int code = (int) Code::saveStorage;
    static constexpr char name[] = "saveStorage";

    static nx::Buffer hash(const Data& data)
    {
        return data.id.c_str();
    }
};

struct RemoveStorage
{
    using Data = std::string;
    static constexpr int code = (int) Code::removeStorage;
    static constexpr char name[] = "removeStorage";

    static nx::Buffer hash(const Data& data)
    {
        return data.c_str();
    }
};

struct SaveSystem
{
    using Data = api::System;
    static constexpr int code = (int) Code::saveSystem;
    static constexpr char name[] = "saveSystem";

    static nx::Buffer hash(const Data& data)
    {
        return (data.storageId + "." + data.id).c_str();
    }
};

struct RemoveSystem
{
    using Data = api::System;
    static constexpr int code = (int) Code::removeSystem;
    static constexpr char name[] = "removeSystem";

    static nx::Buffer hash(const Data& data)
    {
        return (data.storageId + "." + data.id).c_str();
    }
};


} // namespace command

} // namespace nx::cloud::storage::service::model