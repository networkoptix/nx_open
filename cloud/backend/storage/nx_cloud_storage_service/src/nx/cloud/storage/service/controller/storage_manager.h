#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/storage/service/api/add_storage.h>
#include <nx/network/cloud/storage/service/api/storage.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

namespace conf {

struct Aws;
class Settings;

} // namespace conf

namespace controller {

class StorageManager
{
public:
    StorageManager(const conf::Aws& settings);

    virtual void addStorage(
        const api::AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    virtual void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    virtual void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    virtual void listCameras(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler);

private:
};

} // namespace controller
} // namespace nx::cloud::storage::service
