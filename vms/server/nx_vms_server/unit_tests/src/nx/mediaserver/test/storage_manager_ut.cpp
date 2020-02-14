#include <gtest/gtest.h>

#include <QtCore/QThread>

#include <recorder/device_file_catalog.h>
#include <nx/core/access/access_types.h>
#include <nx/utils/test_support/utils.h>
#include <server/server_globals.h>
#include <common/common_module.h>
#include <core/resource/storage_plugin_factory.h>

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

class FtSystemStorageSpace: public ::testing::Test
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

TEST_F(FtSystemStorageSpace, SettingSetCorrectly)
{
    minSystemFreeSpace = 10;
    startServer();
    assertSettingSetCorrectly();
}

TEST_F(FtSystemStorageSpace, EmitIfNoSpace)
{
    minSystemFreeSpace = 10;
    startServer();
    createStorageManager(/* systemStorageFreeSpace */ 3);
    assertSignalReceived();
}

TEST_F(FtSystemStorageSpace, NoEmitIfEnoughSpace)
{
    minSystemFreeSpace = 10;
    startServer();
    createStorageManager(/* systemStorageFreeSpace */ 15);
    assertNoSignalReceived();
}

using namespace std::chrono;

class FtClearSpace: public ::testing::Test
{
protected:
    std::chrono::milliseconds initDelay = 1s;
    int storageCount = 0;

    virtual void SetUp() override
    {
        m_thread.start();
        m_thread.moveToThread(&m_thread);
        std::promise<void> p;
        auto f = p.get_future();
        QTimer::singleShot(
            0ms, &m_thread,
            [this, p = std::move(p)]() mutable
            {
                const auto disabledFeatures = {
                    MediaServerLauncher::DisabledFeature::noPlugins,
                    MediaServerLauncher::DisabledFeature::noMonitorStatistics,
                    MediaServerLauncher::DisabledFeature::noResourceDiscovery };
                m_server.reset(new MediaServerLauncher(/*tmpDir*/ "", /*port*/ 0, disabledFeatures));
                p.set_value();
            });
        f.wait();
    }

    virtual void TearDown() override
    {
        m_thread.quit();
        m_thread.wait();
    }

    void mockPluginCreateStorage()
    {
        QnStoragePluginFactory::setDefault(
            [this](QnCommonModule*, const QString& url)
            {
                auto result = new test_support::StorageStub(
                    m_server->serverModule(),
                    /*url*/ "",
                    /*totalSpace*/ 100'000'000'000LL,
                    /*freeSpace*/ 100'000'000'000LL,
                    /*spaceLimit*/ 0,
                    /*isSystem*/ false,
                    /*isOnline*/ true,
                    /*isUsedForWriting*/ false);
                result->initDelay = initDelay;
                return result;
            });
    }

    void mockMediaFilterConfig()
    {
        using namespace nx::vms::server::fs::media_paths;
        FilterConfig config;
        for (int i = 0; i < storageCount; ++i)
        {
            auto p = PlatformMonitor::PartitionSpace();
            p.path = QString("/some/path_%1").arg(i);
            p.type = PlatformMonitor::LocalDiskPartition;
            config.partitions.push_back(p);
        }
        FilterConfig::setDefault(config);
    }

    void assertStoragesInitalizationAndClearSpaceCallTime()
    {
        while (true)
        {
            std::this_thread::sleep_for(50ms);
            if (m_clearSpaceCalled)
                return;
        }
    }

    void startServer()
    {
        std::promise<void> p;
        auto f = p.get_future();
        QTimer::singleShot(
            0ms, &m_thread,
            [this, p = std::move(p)]() mutable
            {
                ASSERT_TRUE(m_server->start());
                auto manager = m_server->serverModule()->normalStorageManager();
                QObject::connect(
                    manager,
                    &QnStorageManager::clearSpaceCalled,
                    [this, manager]()
                    {
                        saveStorages(manager->getStorages());
                        assertStoragesInitialized();
                        m_clearSpaceCalled = true;
                        m_afterClearSpaceFunc();
                    });
                p.set_value();
            });
        f.wait();
    }

    void stopServer()
    {
        m_server->stop();
    }

    void mockEmptyFilterConfig()
    {
        using namespace nx::vms::server::fs::media_paths;
        FilterConfig::setDefault(FilterConfig());
    }

    void prepareDataToClear()
    {
        ASSERT_GE(storageCount, 2);
        ASSERT_GE(m_storages.size(), 2);
        /**
            * We will make freeSpace < spaceLimit for the first storage.
            * Also we will place 1 chunk on the 1st storage and 100 on the second. The 1st
            * chunk will be newer than the other 100 so to clear the first one - the 100 chunks
            * from the other storage should be deleted first.
            */

        const auto chunkSize = 10LL;
        const auto secondStorageChunks = createChunks(
            /*count*/ m_secondStorageChunks,
            /*timestamp*/ 1,
            /*size*/ chunkSize,
            m_storages[1]);

        const auto firstStorageChunks = createChunks(
            /*count*/ m_firstStorageChunks,
            /*timestamp*/ secondStorageChunks.back().startTimeMs + 1,
            /*size*/ 10,
            m_storages[0]);

        addToCatalog(m_cameraName, firstStorageChunks);
        addToCatalog(m_cameraName, secondStorageChunks);
        m_storages[0]->freeSpace = m_storages[0]->getSpaceLimit() - chunkSize + 1;
    }

    void disconnectOnClearSignal()
    {
        QObject::disconnect(
            m_server->serverModule()->normalStorageManager(),
            &QnStorageManager::clearSpaceCalled,
            nullptr,
            nullptr);
    }

    void setAfterClearCallback(std::function<void()> f)
    {
        m_afterClearSpaceFunc = f;
    }

    void assertDataCleared()
    {
        while (true)
        {
            std::this_thread::sleep_for(50ms);
            const bool storagesCleared =
                m_storages[0]->removeCallCount + m_storages[1]->removeCallCount
                >= m_firstStorageChunks + m_secondStorageChunks;

            const auto catalog = m_server->serverModule()->normalStorageManager()->getFileCatalog(
                m_cameraName, QnServer::HiQualityCatalog);
            const bool catalogCleared = catalog->getChunks().empty();

            if (catalogCleared && storagesCleared)
                break;
        }
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
    QThread m_thread;
    std::atomic<bool> m_clearSpaceCalled = false;
    QList<QSharedPointer<test_support::StorageStub>> m_storages;
    std::function<void()> m_afterClearSpaceFunc = [](){};
    const int m_firstStorageChunks = 1;
    const int m_secondStorageChunks = 100;
    const QString m_cameraName = "cam1";

    std::deque<Chunk> createChunks(
        int count,
        int64_t startTimeMs,
        int64_t chunkSize,
        const QSharedPointer<test_support::StorageStub>& storage)
    {
        std::deque<Chunk> result;
        const int storageIndex =
            m_server->serverModule()->storageDbPool()->getStorageIndex(storage);

        for (int i = 0; i < count; ++i)
        {
            Chunk chunk;
            chunk.startTimeMs = startTimeMs + i;
            chunk.setFileSize(chunkSize);
            chunk.storageIndex = storageIndex;
            result.push_back(chunk);
        }

        return result;
    }

    void addToCatalog(const QString& cameraId, const std::deque<Chunk>& chunks)
    {
        auto catalog = m_server->serverModule()->normalStorageManager()->getFileCatalog(
            cameraId, QnServer::HiQualityCatalog);
        catalog->addChunks(chunks);
    }

    void assertStoragesInitialized()
    {
        ASSERT_EQ(storageCount, m_storages.size())
            << "No storages have been created by the time when "
            << "QnStorageManager::clearSpace has been called";

        const bool allInitialized = std::all_of(
            m_storages.cbegin(), m_storages.cend(),
            [](const auto& s)
            {
                NX_GTEST_ASSERT_TRUE((bool) s);
                NX_GTEST_ASSERT_GT(s->getSpaceLimit(), 0);
                return s->initFinished.load();
            });

        ASSERT_TRUE(allInitialized)
            << "Not all storages have been initialized when "
            << "clearSpace has been called";
    }

    void saveStorages(const StorageResourceList& storages)
    {
        std::transform(
            storages.cbegin(), storages.cend(), std::back_inserter(m_storages),
            [](const auto& s) { return s.dynamicCast<test_support::StorageStub>(); });
    }
};

TEST_F(FtClearSpace, ClearSpaceWontCalledUntilAllStoragesAddedToStorageManager)
{
    initDelay = 10s;
    storageCount = 1;
    mockPluginCreateStorage();
    mockMediaFilterConfig();
    startServer();
    assertStoragesInitalizationAndClearSpaceCallTime();
}

TEST_F(FtClearSpace, NewStoragesCorrectlyDiscovered)
{
    storageCount = 5;
    mockPluginCreateStorage();
    mockMediaFilterConfig();
    startServer();
    assertStoragesInitalizationAndClearSpaceCallTime();
}

TEST_F(FtClearSpace, StoragesLoadedFromDb)
{
    storageCount = 5;
    mockPluginCreateStorage();
    mockMediaFilterConfig();
    startServer();
    assertStoragesInitalizationAndClearSpaceCallTime();

    stopServer();
    mockEmptyFilterConfig(); //< No new storages should be discovered after this.
    startServer();
    assertStoragesInitalizationAndClearSpaceCallTime();
}

TEST_F(FtClearSpace, DISABLED_WontStart_NoRemoveCapability)
{
    // #TODO #akulikov.
}

TEST_F(FtClearSpace, CorrectDataCleared)
{
    storageCount = 2;
    setAfterClearCallback(
        [this]()
        {
            prepareDataToClear();
            disconnectOnClearSignal();
        });

    mockPluginCreateStorage();
    mockMediaFilterConfig();
    startServer();
    assertStoragesInitalizationAndClearSpaceCallTime();
    assertDataCleared();
    // #TODO #akulikov. Check relevant log messages have been written.
}

TEST_F(FtClearSpace, DISABLED_MotionCleaned)
{
    // #TODO #server.
}

TEST_F(FtClearSpace, DISABLED_AnalyticsCleaned_DISABLED)
{
    // #TODO #server.
}

} // namespace test
} // namespace vms::server
} // namespace nx
