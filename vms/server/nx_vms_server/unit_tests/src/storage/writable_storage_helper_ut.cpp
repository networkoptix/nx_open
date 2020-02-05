#include "recorder/writable_storages_helper.h"

#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <test_support/storage_utils.h>

namespace nx::vms::server::test {

class FtWritableStorageHelper: public ::testing::Test
{
protected:
    std::unique_ptr<test_support::StorageManagerStub> storageManager;

    virtual void SetUp() override
    {
        m_server.reset(new MediaServerLauncher);
        ASSERT_TRUE(m_server->start());
        storageManager.reset(new test_support::StorageManagerStub(
            m_server->serverModule(), nullptr, QnServer::StoragePool::Normal));
    }

    virtual void TearDown() override
    {
        if (storageManager)
            storageManager->stopAsyncTasks();
    }

    StorageResourcePtr createStorage(const QString& url) const
    {
        return StorageResourcePtr(new test_support::StorageStub(
            m_server->serverModule(),
            url,
            /*totalSpace*/ 100'000'000'000LL,
            /*freeSpace*/ 100'000'000'000LL,
            /*spaceLimit*/ 10'000'000'000LL,
            /*isSystem*/ false,
            /*isOnline*/ true,
            /*isUsedForWriting*/ true));
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
};

TEST_F(FtWritableStorageHelper, DuplicateStorageNotReturned)
{
    storageManager->storages = {createStorage("url1"), createStorage("url2")};
    const auto writableStorages = storageManager->getAllWritableStorages(
        /*additional*/ {createStorage("url1")});

    ASSERT_EQ(2, writableStorages.size());
}

TEST_F(FtWritableStorageHelper, AdditionalStoragesPresent)
{
    storageManager->storages = { createStorage("url1"), createStorage("url2") };
    const auto writableStorages = storageManager->getAllWritableStorages(
        /*additional*/{ createStorage("url3") });

    ASSERT_EQ(3, writableStorages.size());
}

} // namespace nx::vms::server::test
