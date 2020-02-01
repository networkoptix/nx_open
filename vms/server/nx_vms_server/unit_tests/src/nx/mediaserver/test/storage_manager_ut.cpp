#include <gtest/gtest.h>

#include <recorder/device_file_catalog.h>
#include <nx/core/access/access_types.h>
#include <server/server_globals.h>
#include <common/common_module.h>

#include "media_server_module_fixture.h"
#include <media_server/media_server_module.h>
#include <test_support/mediaserver_launcher.h>
#include <test_support/storage_utils.h>

namespace nx {
namespace vms::server {
namespace test {

void addChunk(
    nx::vms::server::ChunksDeque& chunks,
    qint64 startTimeMs,
    qint64 durationMs)
{
    nx::vms::server::Chunk chunk;
    chunk.startTimeMs = startTimeMs;
    chunk.durationMs = durationMs;
    chunks.push_back(chunk);
};

class StorageManager: public MediaServerModuleFixture
{
};

TEST_F(StorageManager, deleteRecordsToTime)
{
    nx::vms::server::ChunksDeque chunks;
    addChunk(chunks, 5, 10);
    addChunk(chunks, 20, 5);
    addChunk(chunks, 100, 500);

    DeviceFileCatalogPtr catalog(new DeviceFileCatalog(
        &serverModule(),
        lit("camera1"),
        QnServer::ChunksCatalog::HiQualityCatalog,
        QnServer::StoragePool::Normal));
    catalog->addChunks(chunks);

    serverModule().normalStorageManager()->deleteRecordsToTime(catalog, 4);
    ASSERT_EQ(5, catalog->minTime());

    serverModule().normalStorageManager()->deleteRecordsToTime(catalog, 50);
    ASSERT_EQ(100, catalog->minTime());

    serverModule().normalStorageManager()->deleteRecordsToTime(catalog, 100);
    ASSERT_EQ(100, catalog->minTime());

    serverModule().normalStorageManager()->deleteRecordsToTime(catalog, 599);
    ASSERT_EQ(100, catalog->minTime());

    serverModule().normalStorageManager()->deleteRecordsToTime(catalog, 600);
    ASSERT_EQ(AV_NOPTS_VALUE, catalog->minTime());
}

class FtStorageManager: public ::testing::Test
{
protected:
    qint64 minSystemFreeSpace = -1;

    virtual void SetUp() override
    {
        m_server.reset(new MediaServerLauncher);
    }

    virtual void TearDown() override
    {
        if (m_storageManager)
            m_storageManager->stopAsyncTasks();
    }

    void assertSignalReceived()
    {
        while (!m_failureSignalReceived)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        ASSERT_TRUE(m_failedStorage->isSystem());
        ASSERT_TRUE(m_failedStorage.dynamicCast<test_support::StorageStub>());
    }

    void assertNoSignalReceived()
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        ASSERT_FALSE(m_failureSignalReceived);
    }

    void assertSettingSetCorrectly()
    {
        ASSERT_EQ(minSystemFreeSpace, m_server->serverModule()->settings().minSystemStorageFreeSpace());
    }

    void startServer()
    {
        m_server->addSetting("minSystemStorageFreeSpace", minSystemFreeSpace);
        ASSERT_TRUE(m_server->start());
    }

    void createStorageManager(int64_t systemStorageFreeSpace)
    {
        m_storageManager.reset(new test_support::StorageManagerStub(
            m_server->serverModule(), nullptr, QnServer::StoragePool::Normal));

        m_storageManager->storages = createStorages(systemStorageFreeSpace);
        QObject::connect(
            m_storageManager.get(), &QnStorageManager::storageFailure,
            [this](const QnResourcePtr& resource, nx::vms::api::EventReason reason)
            {
                if (reason == nx::vms::api::EventReason::systemStorageFull)
                {
                    m_failureSignalReceived = true;
                    m_failedStorage = resource.dynamicCast<QnStorageResource>();
                }
            });
        m_storageManager->startAuxTimerTasks();
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
    std::unique_ptr<test_support::StorageManagerStub> m_storageManager;
    std::atomic<bool> m_failureSignalReceived = false;
    QnStorageResourcePtr m_failedStorage;

    nx::vms::server::StorageResourceList createStorages(qint64 systemStorageFreeSpace) const
    {
        return nx::vms::server::StorageResourceList{
            createStorage(/* isSystem */ false, /* freeSpace */ 5),
            createStorage(/* isSystem */ true, /* freeSpace */ systemStorageFreeSpace)};
    }

    StorageResourcePtr createStorage(bool isSystem, int64_t freeSpace) const
    {
        return StorageResourcePtr(new test_support::StorageStub(
            m_server->serverModule(),
            /*url*/ "",
            /*totalSpace*/ 0,
            freeSpace,
            /*spaceLimit*/ 0,
            isSystem,
            /*isOnline*/ true,
            /*isUsedForWriting*/ false));
    }
};

TEST_F(FtStorageManager, checkSystemStorageSpace_SettingSetCorrectly)
{
    minSystemFreeSpace = 10;
    startServer();
    assertSettingSetCorrectly();
}

TEST_F(FtStorageManager, checkSystemStorageSpace_EmitIfNoSpace)
{
    minSystemFreeSpace = 10;
    startServer();
    createStorageManager(/* systemStorageFreeSpace */ 3);
    assertSignalReceived();
}

TEST_F(FtStorageManager, checkSystemStorageSpace_NoEmitIfEnoughSpace)
{
    minSystemFreeSpace = 10;
    startServer();
    createStorageManager(/* systemStorageFreeSpace */ 15);
    assertNoSignalReceived();
}

} // namespace test
} // namespace vms::server
} // namespace nx
