#include <gtest/gtest.h>

#include <recorder/device_file_catalog.h>
#include <recorder/storage_manager.h>
#include <nx/core/access/access_types.h>
#include <server/server_globals.h>
#include <common/common_module.h>

#include "media_server_module_fixture.h"
#include <media_server/media_server_module.h>
#include <test_support/mediaserver_launcher.h>

namespace nx {
namespace vms::server {
namespace test {

class TestStorageManager: public QnStorageManager
{
public:
    nx::vms::server::StorageResourceList storages;

    using QnStorageManager::QnStorageManager;

private:
    virtual std::chrono::milliseconds checkSystemFreeSpaceInterval() const override
    {
        return std::chrono::seconds(1);
    }

    virtual StorageResourceList getStorages() const override
    {
        return storages;
    }
};

class StorageStub: public StorageResource
{
public:
    bool isSystemFlag = false;
    qint64 freeSpace = -1;

    StorageStub(
        QnMediaServerModule* serverModule,
        bool isSystem,
        qint64 freeSpace)
        :
        StorageResource(serverModule),
        isSystemFlag(isSystem),
        freeSpace(freeSpace)
    {}

    virtual bool isSystem() const override { return isSystemFlag; }
    virtual bool isOnline() const override { return true; }
    virtual QIODevice* openInternal(const QString& fileName, QIODevice::OpenMode openMode) override { return nullptr; }
    virtual int getCapabilities() const override { return -1; }
    virtual qint64 getFreeSpace() override { return freeSpace; }
    virtual qint64 getTotalSpace() const override { return -1; }
    virtual Qn::StorageInitResult initOrUpdate() { return Qn::StorageInit_WrongPath; }
    virtual bool removeFile(const QString& /* url */) { return false; }
    virtual bool removeDir(const QString& /* url */) override { return false; }
    virtual bool renameFile(const QString& /* oldName */, const QString& /* newName */) { return false; }
    virtual FileInfoList getFileList(const QString& /* dirName */) override { return FileInfoList(); }
    virtual bool isFileExists(const QString& /* url */) override { return false; }
    virtual bool isDirExists(const QString& /* url */) override { return false; }
    virtual qint64 getFileSize(const QString& /* url */) const override { return false; }
};

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
        ASSERT_TRUE(m_failedStorage.dynamicCast<StorageStub>());
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
        m_storageManager.reset(new TestStorageManager(
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
    std::unique_ptr<TestStorageManager> m_storageManager;
    std::atomic<bool> m_failureSignalReceived = false;
    QnStorageResourcePtr m_failedStorage;

    nx::vms::server::StorageResourceList createStorages(qint64 systemStorageFreeSpace) const
    {
        return nx::vms::server::StorageResourceList{
            StorageResourcePtr(new StorageStub(m_server->serverModule(), /* isSystem */ false, /* freeSpace */ 5)),
            StorageResourcePtr(new StorageStub(m_server->serverModule(), /* isSystem */ true, /* freeSpace */ systemStorageFreeSpace))};
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
