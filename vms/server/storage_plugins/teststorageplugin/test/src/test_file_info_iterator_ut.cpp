#include <string.h>
#include <gtest/gtest.h>
#include <test_file_info_iterator.h>
#include <test_common.h>

class TestFileInfoIteratorTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        storage = factory.createStorage("test://storage/some/path", nullptr);
        ASSERT_TRUE(storage);
    }

    virtual void TearDown() override
    {
        if (storage)
            storage->releaseRef();
    }

    TestStorageFactoryHelper factory;
    nx_spl::Storage* storage;
};

TEST_F(TestFileInfoIteratorTest, invalidPath)
{
    auto fileIterator = storage->getFileIterator("test://storage/some/invalid/path", nullptr);
    ASSERT_FALSE(fileIterator);
}

TEST_F(TestFileInfoIteratorTest, Folders)
{
    auto fileIterator = storage->getFileIterator("test://storage/some/path", nullptr);
    ASSERT_TRUE(fileIterator);

    int ecode;
    auto fInfo = fileIterator->next(&ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_EQ(strcmp(fInfo->url, "test://storage/some/path/hi_quality"), 0);
    ASSERT_EQ(fInfo->size, 0);
    ASSERT_EQ(fInfo->type, nx_spl::isDir);

    fInfo = fileIterator->next(&ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_EQ(strcmp(fInfo->url, "test://storage/some/path/low_quality"), 0);

    fInfo = fileIterator->next(&ecode);
    ASSERT_EQ(fInfo, nullptr);

    fileIterator->releaseRef();
}

TEST_F(TestFileInfoIteratorTest, Files)
{
    auto fileIterator = storage->getFileIterator(
        "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/", 
        nullptr);
    ASSERT_TRUE(fileIterator);

    int ecode;
    auto fInfo = fileIterator->next(&ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_EQ(strcmp(
        fInfo->url, 
        "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461075_60000.mkv"), 0);
    ASSERT_EQ(fInfo->type, nx_spl::isFile);

    fInfo = fileIterator->next(&ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_EQ(strcmp(
        fInfo->url, 
        "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076_60000.mkv"), 0);

    fileIterator->releaseRef();
}
