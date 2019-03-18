#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <health/system_health.h>
#include <recorder/storage_manager.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::server::test {

class Reindex: public QObject, public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        MediaServerLauncher::DisabledFeatures startupFlags;
        startupFlags.setFlag(MediaServerLauncher::noResourceDiscovery);
        startupFlags.setFlag(MediaServerLauncher::noMonitorStatistics);
        startupFlags.setFlag(MediaServerLauncher::noPlugins);

        m_server.reset(new MediaServerLauncher(QString(), 0, startupFlags));
    }

    void whenServerStarted()
    {
        m_server->addSetting("minStorageSpace", 0);
        ASSERT_TRUE(m_server->start());

        QObject::connect(
            m_server->serverModule()->normalStorageManager(), &QnStorageManager::rebuildFinished,
            this, &Reindex::onReindexFinished);
    }

    void givenSomeArchiveOnHdd()
    {
        m_generatedArchive["camera1"] = generateCameraArchive(
            "camera1", std::chrono::system_clock::now() - std::chrono::hours(24), 10);

        m_generatedArchive["camera2"] = generateCameraArchive(
            "camera2", std::chrono::system_clock::now() - std::chrono::hours(23), 5);
    }

    void thenAllArchiveShouldBeFastScannedCorreclty()
    {
    }

private:
    struct ChunkData
    {
        qint64 startTimeMs;
        qint64 durationsMs;
    };

    using ChunkDataList = std::vector<ChunkData>;

    struct Catalog
    {
        ChunkDataList lowQualityChunks;
        ChunkDataList highQualityChunks;
    };

    using Archive = QMap<QString, Catalog>;

    std::unique_ptr<MediaServerLauncher> m_server;
    Archive m_generatedArchive;
    QString m_baseDirPath;

    void onReindexFinished(QnSystemHealth::MessageType message)
    {
        qDebug() << "REINDEX FINISHED" << message;
    }

    Catalog generateCameraArchive(
        const QString& cameraName,
        std::chrono::time_point<std::chrono::system_clock> startPoint,
        int count)
    {
        const auto storages = m_server->serverModule()->resourcePool()->getResources<QnStorageResource>();
        NX_ASSERT(!storages.isEmpty());

        const auto storage = storages[0];
        m_baseDirPath = storage->getUrl();

        return Catalog();
    }
};

TEST_F(Reindex, FastArchiveScan_AllDataRetrieved)
{
    whenServerStarted();
    givenSomeArchiveOnHdd();
    thenAllArchiveShouldBeFastScannedCorreclty();
}

} // nx::vms::server::test
