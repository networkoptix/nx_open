#include "media_server/settings.h"
#include "mediaserver_helper/media_server_helper.h"
#include "storage/storage_test_helper.h"
#include "media_server_process.h"
#include "media_server_process_aux.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/storage_resource.h"
#include "media_server_process.h"
#include "common/common_module.h"

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

TEST(InitStoragesTest, main)
{
    nx::ut::utils::MediaServerTestFuncTypeList testList;
    testList.push_back(
        []() 
        {
            auto storages = qnResPool->getResources<QnStorageResource>();
            ASSERT_TRUE(storages.isEmpty());
        });
    nx::ut::utils::MediaServerHelper helper(testList);
    MSSettings::roSettings()->setValue(
        nx_ms_conf::MIN_STORAGE_SPACE,
        (qint64)std::numeric_limits<int64_t>::max());
    helper.start();
}

TEST(SaveRestoreStoragesInfoFromConfig, main)
{
    QString path1 = lit("c:\\some path\\data1");
    QString path2 = lit("f:\\some\\other\\path\\data1");

    qint64 spaceLimit1 = 50 * 1024 * 1024l;
    qint64 spaceLimit2 = 255 * 1024 * 1024l;

    nx::ut::utils::FileStorageTestHelper storageTestHelper;
    auto storage1 = storageTestHelper.createStorage(path1, spaceLimit1);
    auto storage2 = storageTestHelper.createStorage(path2, spaceLimit2);

    QnStorageResourceList storageList;
    BeforeRestoreDbData restoreData;
    storageList << storage1 << storage2;

    nx::mserver_aux::saveStoragesInfoToBeforeRestoreData(&restoreData, storageList);

    ASSERT_TRUE(restoreData.hasInfoForStorage(path1));
    ASSERT_TRUE(restoreData.hasInfoForStorage(path2));

    ASSERT_EQ(restoreData.getSpaceLimitForStorage(path1), spaceLimit1);
    ASSERT_EQ(restoreData.getSpaceLimitForStorage(path2), spaceLimit2);
}

class UnmountedStoragesFilterTest : public ::testing::Test
{
protected:
    UnmountedStoragesFilterTest() :
        mediaFolderName(lit("HD Witness Media")),
        unmountedFilter(mediaFolderName),
        spaceLimit(10 * 1024 * 1024 * 1024ll)
    {}

    nx::ut::utils::FileStorageTestHelper storageTestHelper;
    QString mediaFolderName;
    nx::mserver_aux::UnmountedStoragesFilter unmountedFilter;
    qint64 spaceLimit;
};

TEST_F(UnmountedStoragesFilterTest , main)
{
    QString url1 = lit("/media/stor1/") + mediaFolderName;
    QString url2 = lit("/opt/mediaserver/data");
    QString url3 = lit("/some/other/data/") + mediaFolderName;
    QString url4 = lit("/media/stor2/");

    QnStorageResourceList storages;
    storages.append(storageTestHelper.createStorage(url1, spaceLimit));
    storages.append(storageTestHelper.createStorage(url2, spaceLimit));
    storages.append(storageTestHelper.createStorage(url3, spaceLimit));
    storages.append(storageTestHelper.createStorage(url4, spaceLimit));

    QStringList paths;
    paths << url2 << url3;

    auto unmountedStorages = unmountedFilter.getUnmountedStorages(storages, paths);
    ASSERT_EQ(unmountedStorages.size(), 2);
    ASSERT_TRUE(unmountedStorages[0]->getUrl() == url1);
    ASSERT_TRUE(unmountedStorages[1]->getUrl() == url4);

    paths << lit("/media/stor1") << url4;

    unmountedStorages = unmountedFilter.getUnmountedStorages(storages, paths);
    ASSERT_EQ(unmountedStorages.size(), 0);
}
