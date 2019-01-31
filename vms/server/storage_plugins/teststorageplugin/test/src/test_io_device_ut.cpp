#include <string.h>
#include <algorithm>
#include "test_common.h"


TEST_F(TestStorageTest, IODevice_open_Read)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_60000.mkv",
                nx_spl::io::ReadOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_NE(ioDevice1.get(), nullptr);

    /* write should fail*/
    ASSERT_EQ(ioDevice1->write(nullptr, 10, &ecode), 0);
    ASSERT_NE(ecode, nx_spl::error::NoError);
}

TEST_F(TestStorageTest, IODevice_open_Read_NotExisting)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/not_existing.mkv",
                nx_spl::io::ReadOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_NE(ecode, nx_spl::error::NoError);
    ASSERT_EQ(ioDevice1.get(), nullptr);
}

TEST_F(TestStorageTest, IODevice_open_Write_Existing)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_60000.mkv",
                nx_spl::io::WriteOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_NE(ioDevice1.get(), nullptr);

    /* write should NOT fail*/
    ASSERT_EQ(ioDevice1->write(nullptr, 10, &ecode), 10);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
}

TEST_F(TestStorageTest, IODevice_open_Write_NotExisting)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/not_existing.mkv",
                nx_spl::io::WriteOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_NE(ioDevice1.get(), nullptr);
    ASSERT_TRUE(
        storage->fileExists(
            "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/not_existing.mkv",
            nullptr));
}

TEST_F(TestStorageTest, IODevice_MediaFile_Read_Seek)
{
    char buf[512];
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_60000.mkv",
                nx_spl::io::ReadOnly,
                &ecode)), 
        ioDeviceDeleter);

    ASSERT_TRUE(ioDevice1);

    ASSERT_EQ(ioDevice1->read(buf, 5, &ecode), 5);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    buf[5] = '\0';
    ASSERT_EQ(strcmp(buf, "12345"), 0);

    ASSERT_EQ(ioDevice1->read(buf + 5, 10, &ecode), 5);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    buf[10] = '\0';
    ASSERT_EQ(strcmp(buf, "1234567890"), 0);

    ASSERT_TRUE(ioDevice1->seek(0, &ecode));
    ASSERT_EQ(ioDevice1->read(buf, 10, &ecode), 10);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    buf[10] = '\0';
    ASSERT_EQ(strcmp(buf, "1234567890"), 0);
}

TEST_F(TestStorageTest, IODevice_DbFile_Write)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/db.nxdb",
                nx_spl::io::WriteOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_TRUE((bool)ioDevice1);
    ASSERT_EQ(ecode, nx_spl::error::NoError);

    ASSERT_EQ(ioDevice1->write(nullptr, 10, &ecode), 10);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
}

TEST_F(TestStorageTest, IODevice_DbFile_Read)
{
    int ecode;

    /* open for read should fail */
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/db.nxdb",
                nx_spl::io::ReadOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_FALSE((bool)ioDevice1);
    ASSERT_NE(ecode, nx_spl::error::NoError);
}

TEST_F(TestStorageTest, IODevice_InfoTxt_Read)
{
    int ecode;
    char buf[512];
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/info.txt",
                nx_spl::io::ReadOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_TRUE((bool)ioDevice1);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_GT(ioDevice1->read(buf, 512, &ecode), 0);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_NE(strstr(buf, "TestStorageCamera"), nullptr);
}

TEST_F(TestStorageTest, IODevice_InfoTxt_Write)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/info.txt",
                nx_spl::io::WriteOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_TRUE((bool)ioDevice1);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_EQ(ioDevice1->write(nullptr, 10, &ecode), 10);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
}
