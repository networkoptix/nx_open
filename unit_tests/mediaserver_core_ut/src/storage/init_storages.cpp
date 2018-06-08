#include <fstream>

#include "media_server/settings.h"
#include "storage/storage_test_helper.h"
#include "media_server_process.h"
#include "media_server_process_aux.h"
#include <nx/core/access/access_types.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource/storage_resource.h"
#include "media_server_process.h"
#include "common/common_module.h"
#include <test_support/mediaserver_launcher.h>
#include <recorder/storage_manager.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include "../media_server_module_fixture.h"

TEST(InitStoragesTest, main)
{
    MediaServerLauncher launcher;
    launcher.addSetting(
        "minStorageSpace",
        (qint64)std::numeric_limits<int64_t>::max());
    ASSERT_TRUE(launcher.start());
    auto storages = launcher.commonModule()->resourcePool()->getResources<QnStorageResource>();
    ASSERT_TRUE(storages.isEmpty());
}

TEST(SaveRestoreStoragesInfoFromConfig, main)
{
    QString path1 = lit("c:\\some path\\data1");
    QString path2 = lit("f:\\some\\other\\path\\data1");

    qint64 spaceLimit1 = 50 * 1024 * 1024l;
    qint64 spaceLimit2 = 255 * 1024 * 1024l;

    QnCommonModule commonModule(/*clientMode*/ false, nx::core::access::Mode::direct);
    nx::ut::utils::FileStorageTestHelper storageTestHelper;
    auto storage1 = storageTestHelper.createStorage(&commonModule, path1, spaceLimit1);
    auto storage2 = storageTestHelper.createStorage(&commonModule, path2, spaceLimit2);

    QnStorageResourceList storageList;
    BeforeRestoreDbData restoreData;
    storageList << storage1 << storage2;

    nx::mserver_aux::saveStoragesInfoToBeforeRestoreData(&restoreData, storageList);

    ASSERT_TRUE(restoreData.hasInfoForStorage(path1));
    ASSERT_TRUE(restoreData.hasInfoForStorage(path2));

    ASSERT_EQ(restoreData.getSpaceLimitForStorage(path1), spaceLimit1);
    ASSERT_EQ(restoreData.getSpaceLimitForStorage(path2), spaceLimit2);
}

class UnmountedLocalStoragesFilterTest:
    public MediaServerModuleFixture
{
protected:

    virtual void TearDown() override
    {
        qnNormalStorageMan->stopAsyncTasks();
        qnBackupStorageMan->stopAsyncTasks();
    }
    enum class PathFound
    {
        yes,
        no
    };

    enum class MediaFolderSuffix
    {
        yes,
        no
    };

    enum class StorageUnmounted
    {
        yes,
        no
    };

    UnmountedLocalStoragesFilterTest():
        mediaFolderName(lit("HD Witness Media")),
        unmountedFilter(mediaFolderName),
        spaceLimit(10 * 1024 * 1024 * 1024ll)
    {
    }

    void when(PathFound isPathFound, MediaFolderSuffix isSuffix)
    {
        QString url = basePath;
        if (isSuffix == MediaFolderSuffix::yes)
            url += "/" + mediaFolderName;

        unmountedStorages = unmountedFilter.getUnmountedStorages(
              QnStorageResourceList() << storageTestHelper.createStorage(serverModule().commonModule(), url, spaceLimit),
              isPathFound == PathFound::yes ? QStringList() << url : QStringList());
    }

    void then(StorageUnmounted isUnmounted)
    {
        if (isUnmounted == StorageUnmounted::yes)
            ASSERT_FALSE(unmountedStorages.isEmpty());
        else
            ASSERT_TRUE(unmountedStorages.isEmpty());
    }

    nx::ut::utils::FileStorageTestHelper storageTestHelper;
    QString basePath = "/some/path";
    QString mediaFolderName;
    nx::mserver_aux::UnmountedLocalStoragesFilter unmountedFilter;
    qint64 spaceLimit;
    QnStorageResourceList unmountedStorages;
};

TEST_F(UnmountedLocalStoragesFilterTest, main)
{
    when(PathFound::yes, MediaFolderSuffix::yes);
    then(StorageUnmounted::no);

    when(PathFound::yes, MediaFolderSuffix::no);
    then(StorageUnmounted::no);

    when(PathFound::no, MediaFolderSuffix::yes);
    then(StorageUnmounted::yes);

    when(PathFound::no, MediaFolderSuffix::no);
    then(StorageUnmounted::yes);
}
