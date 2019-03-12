#include <gtest/gtest.h>

#include <test_storage_factory.h>

#include "test_common.h"

TEST(TestStorageFactoryTest, main)
{
    TestStorageFactoryHelper factory;
    int ecode;

    /* auto generated config name */
    auto storage = factory.createStorage("test://storage1/some/path", &ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_TRUE(storage);
    ASSERT_TRUE(factory.getConfigPath().find("storage1.cfg") != std::string::npos);

    storage->releaseRef();

    /* custom config name */
    storage = factory.createStorage("test://storage2/some/path?config=myConfig.cfg", &ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_TRUE(storage);
    ASSERT_TRUE(factory.getConfigPath().find("myConfig.cfg") != std::string::npos);

    storage->releaseRef();
}

TEST(TestStorageFactoryTest, wrongUrl)
{
    TestStorageFactoryHelper factory;

    /* wrong scheme */
    ASSERT_FALSE(factory.createStorage("tes://storage1/some/path", nullptr));
    /* wrong url */
    ASSERT_FALSE(factory.createStorage("test//storage1/some/path", nullptr));
}

TEST(TestStorageFactoryTest, alreadyExists)
{
    TestStorageFactoryHelper factory;

    auto storage = factory.createStorage("test://storage1/some/path", nullptr);
    ASSERT_TRUE(storage);
    ASSERT_FALSE(factory.createStorage("test://storage1/some/path", nullptr));

    storage->releaseRef();
}

TEST(TestStorageFactoryTest, configNotRead)
{
    TestStorageFactoryHelper factory(true);

    ASSERT_FALSE(factory.createStorage("test://storage1/some/path", nullptr));
    ASSERT_FALSE(factory.createStorage("test://storage1/some/other/path2", nullptr));
}

TEST(TestStorageFactoryTest, StorageRemovedOnDestroy)
{
    TestStorageFactoryHelper factory;

    auto storage = factory.createStorage("test://storage1/some/path", nullptr);
    ASSERT_TRUE(storage);
    ASSERT_NE(factory.storageHosts().find("storage1"), factory.storageHosts().cend());

    storage->releaseRef();
    ASSERT_EQ(factory.storageHosts().find("storage1"), factory.storageHosts().cend());
}
