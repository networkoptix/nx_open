#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <common/common_module.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

namespace {

/**
 * Assert that the list contains the storage with the specified name, retrieve this storage and
 * check its id and parentId if specified.
 */
#define NX_FIND_STORAGE_BY_NAME(...) ASSERT_NO_FATAL_FAILURE(findStorageByName(__VA_ARGS__))
static void findStorageByName(
    const vms::api::StorageDataList& storages,
    vms::api::StorageData* outStorage,
    const QString name,
    QnUuid parentId = QnUuid(),
    QnUuid id = QnUuid())
{
    ASSERT_TRUE(storages.size() > 0);

    const auto foundStorage = std::find_if(storages.cbegin(), storages.cend(),
        [name](const vms::api::StorageData& s) { return s.name == name; });

    ASSERT_TRUE(foundStorage != storages.cend());

    *outStorage = *foundStorage; //< Copy struct by value.

    if (!id.isNull())
        ASSERT_EQ(id, outStorage->id);

    if (!parentId.isNull())
        ASSERT_EQ(parentId, outStorage->parentId);
}

} // namespace

TEST(GetStorages, saveAndMerge)
{
    MediaServerLauncher launcher;
    launcher.addSetting(QnServer::kNoInitStoragesOnStartup, "1");
    ASSERT_TRUE(launcher.start());

    vms::api::StorageDataList storages;
    vms::api::StorageData storage;

    NX_INFO(this, "Create a new storage with auto-generated id.");
    storage.name = "original name";
    storage.parentId = launcher.commonModule()->moduleGUID();
    storage.spaceLimit = 113326;
    storage.storageType = "local";
    NX_TEST_API_POST(&launcher,
        lit("/ec2/saveStorage"), storage, removeJsonFields({"id"}));

    NX_INFO(this, "Retrieve the created storage.");
    NX_TEST_API_GET(&launcher, lit("/ec2/getStorages"), &storages);
    NX_FIND_STORAGE_BY_NAME(storages, &storage, storage.name, storage.parentId);
    ASSERT_EQ(1U, storages.size());

    NX_INFO(this, "Rename the storage via Merge.");
    storage.name = "new name";
    NX_TEST_API_POST(&launcher,
        lit("/ec2/saveStorage"), storage, keepOnlyJsonFields({"id", "name"}));

    NX_INFO(this, "Check that the storage is renamed.");
    NX_TEST_API_GET(&launcher, lit("/ec2/getStorages"), &storages);
    NX_FIND_STORAGE_BY_NAME(storages, &storage, storage.name, storage.parentId, storage.id);
    ASSERT_EQ(1U, storages.size());

    NX_INFO(this, "Check the storage can be found by its parent server id.");
    NX_TEST_API_GET(&launcher,
        lit("/ec2/getStorages?id=%1").arg(storage.parentId.toString()), &storages);
    NX_FIND_STORAGE_BY_NAME(storages, &storage, storage.name, storage.parentId, storage.id);
    ASSERT_EQ(1U, storages.size());

    NX_INFO(this, "Check that no storages are found by another (non-existing) parent server id.");
    NX_TEST_API_GET(&launcher,
        lit("/ec2/getStorages?id=%1").arg(QnUuid::createUuid().toString()), &storages);
    ASSERT_TRUE(storages.empty());
}

} // namespace test
} // namespace nx
