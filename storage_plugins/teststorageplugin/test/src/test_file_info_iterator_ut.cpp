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

TEST_F(TestFileInfoIteratorTest, main)
{
    auto fileIterator = storage->getFileIterator("test://storage/some/path", nullptr);
    ASSERT_TRUE(fileIterator);

    int ecode;
    auto fInfo = fileIterator->next(&ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_EQ(strcmp(fInfo->url, "test://storage/some/path/hi_quality"), 0);
}