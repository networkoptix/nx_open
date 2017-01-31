#include <gtest/gtest.h>

#include <common/common_module.h>

// Config for debugging the tests.
static const struct
{
    const bool enableHangOnFinish = true;
    const bool forceLog = true;
} conf{};
#include <nx/utils/test_support/test_utils.h>

#include "test_api_request.h"

namespace nx {
namespace test {

namespace {

/**
 * Assert that the list contains the storage with the specified name, retrieve this storage and
 * check its id and parentId if specified.
 */
static void findStorageByName(
    const ec2::ApiStorageDataList& storages,
    ec2::ApiStorageData* outStorage,
    const QString name,
    QnUuid parentId = QnUuid(),
    QnUuid id = QnUuid())
{
    ASSERT_TRUE(storages.size() > 0);

    const auto foundStorage = std::find_if(storages.cbegin(), storages.cend(),
        [name](const ec2::ApiStorageData& s) { return s.name == name; });

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
    ASSERT_TRUE(launcher.start());

    ec2::ApiStorageDataList storages;
    ec2::ApiStorageData storage;
    ec2::ApiStorageDataList::const_iterator foundStorage;

    LOG("Create a new storage with auto-generated id:");
    storage.name = "original name";
    storage.parentId = qnCommon->moduleGUID();
    storage.spaceLimit = 113326;
    storage.storageType = "local";
    testApiPost(launcher, lit("/ec2/saveStorage"), storage, removeJsonFields({"id"}));

    LOG("Retrieve the created storage:");
    testApiGet(launcher, lit("/ec2/getStorages"), &storages);
    findStorageByName(storages, &storage, storage.name, storage.parentId);

    LOG("Rename the storage via Merge:");
    storage.name = "new name";
    testApiPost(launcher, lit("/ec2/saveStorage"), storage, keepOnlyJsonFields({"id", "name"}));

    LOG("Check that the storage is renamed:");
    testApiGet(launcher, lit("/ec2/getStorages"), &storages);
    findStorageByName(storages, &storage, storage.name, storage.parentId, storage.id);

    LOG("Check the storage can be found by its parent server id:");
    testApiGet(launcher, lit("/ec2/getStorages?id=%1").arg(storage.parentId.toString()),
        &storages);
    findStorageByName(storages, &storage, storage.name, storage.parentId, storage.id);

    LOG("Check that no storages are found by another (non-existing) parent server id:");
    testApiGet(launcher, lit("/ec2/getStorages?id=%1").arg(QnUuid::createUuid().toString()),
        &storages);
    ASSERT_TRUE(storages.empty());

    finishTest(HasFailure());
}

} // namespace test
} // namespace nx
