#include <gtest/gtest.h>
#include <test_storage.h>
#include "test_common.h"

class TestStorageTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        storage = factory.createStorage("test://storage/some/path", nullptr);
        ASSERT_TRUE(storage);
    }

    virtual void TearDown() override
    {
        storage->releaseRef();
    }

    TestStorageFactoryHelper factory;
    nx_spl::Storage* storage;
};

TEST_F(TestStorageTest, permanentGetters)
{
    ASSERT_TRUE(storage->isAvailable());
    ASSERT_GT(storage->getFreeSpace(nullptr), 50 * 1024 * 1024 * 1024LL);
    ASSERT_GT(storage->getTotalSpace(nullptr), storage->getFreeSpace(nullptr));
    ASSERT_TRUE(storage->getCapabilities() & nx_spl::cap::ListFile);
    ASSERT_TRUE(storage->getCapabilities() & nx_spl::cap::RemoveFile);
    ASSERT_TRUE(storage->getCapabilities() & nx_spl::cap::ReadFile);
    ASSERT_TRUE(storage->getCapabilities() & nx_spl::cap::WriteFile);
    ASSERT_TRUE(storage->getCapabilities() & nx_spl::cap::DBReady);
}

TEST_F(TestStorageTest, existGetters)
{
    ASSERT_TRUE(storage->fileExists("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", nullptr));
    ASSERT_TRUE(storage->fileExists("test://storage/some/path/low_quality/someCameraId1/2016/01/23/15/1453550461077.mkv", nullptr));
    ASSERT_TRUE(storage->fileExists("test://storage/some/path/hi_quality/someCameraId2/2016/01/23/15/1453550461078.mkv", nullptr));
    ASSERT_TRUE(storage->fileExists("test://storage/some/path/low_quality/someCameraId2/2016/01/23/15/1453550461079.mkv", nullptr));
    ASSERT_FALSE(storage->fileExists("test://storage/some/path/low_quality/someCameraId2/2016/01/23/15/1453550461099.mkv", nullptr));

    ASSERT_TRUE(storage->dirExists("test://storage/some/path/hi_quality/someCameraId1/2016/01/23", nullptr));
    ASSERT_TRUE(storage->dirExists("test://storage/some/path/hi_quality/someCameraId1", nullptr));
    ASSERT_FALSE(storage->dirExists("test://storage/some/path/hi_quality/someCameraId3", nullptr));
}

TEST_F(TestStorageTest, removeFile)
{
    int ecode;
    storage->removeFile("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", &ecode);
    ASSERT_FALSE(storage->fileExists("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", nullptr));
    ASSERT_EQ(ecode, nx_spl::error::NoError);

    storage->removeFile("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", &ecode);
    ASSERT_NE(ecode, nx_spl::error::NoError);
}

TEST_F(TestStorageTest, removeDir)
{
    int ecode;
    storage->removeDir("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", &ecode);
    ASSERT_NE(ecode, nx_spl::error::NoError);

    storage->removeDir("test://storage/some/path/hi_quality/someCameraId1", &ecode);
    ASSERT_FALSE(storage->fileExists("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", nullptr));
    ASSERT_TRUE(storage->fileExists("test://storage/some/path/low_quality/someCameraId1/2016/01/23/15/1453550461077.mkv", nullptr));
    ASSERT_FALSE(storage->dirExists("test://storage/some/path/hi_quality/someCameraId1/2016/01", nullptr));

    storage->removeDir("test://storage/some/path/hi_quality/someCameraId1", &ecode);
    ASSERT_NE(ecode, nx_spl::error::NoError);
}

TEST_F(TestStorageTest, renameFile)
{
    int ecode;
    storage->renameFile(
        "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", 
        "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_11111.mkv",
        &ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_FALSE(storage->fileExists("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv", nullptr));
    ASSERT_TRUE(storage->fileExists("test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_11111.mkv", nullptr));

    storage->renameFile(
        "test://storage/some/path/hi_quality/someCameraId1",
        "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_11111.mkv",
        &ecode);
    ASSERT_NE(ecode, nx_spl::error::NoError);
}
