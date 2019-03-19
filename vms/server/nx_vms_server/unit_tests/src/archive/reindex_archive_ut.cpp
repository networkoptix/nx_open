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

    virtual void TearDown() override
    {
        if (!m_baseDirPath.isEmpty())
            QDir(m_baseDirPath).removeRecursively();
    }

    void whenServerStarted()
    {
        m_server->addSetting("minStorageSpace", 0);
        ASSERT_TRUE(m_server->start());

        if (m_baseDirPath.isEmpty())
        {
            const auto storages =
                m_server->serverModule()->resourcePool()->getResources<QnStorageResource>();
            NX_ASSERT(!storages.isEmpty());

            const auto storage = storages[0];
            m_baseDirPath = storage->getUrl();
        }

        QObject::connect(
            m_server->serverModule()->normalStorageManager(), &QnStorageManager::rebuildFinished,
            this, &Reindex::onReindexFinished);
    }

    void whenServerStopped()
    {
        ASSERT_TRUE(m_server->stop());
    }

    void givenSomeArchiveOnHdd()
    {
        NX_ASSERT(!m_baseDirPath.isEmpty());
        QDir(m_baseDirPath).removeRecursively();

        m_generatedArchive["camera1"] = generateCameraArchive(
            "camera1", std::chrono::system_clock::now() - std::chrono::hours(24), 10);

        m_generatedArchive["camera2"] = generateCameraArchive(
            "camera2", std::chrono::system_clock::now() - std::chrono::hours(23), 5);
    }

    void thenAllArchiveShouldBeFastScannedCorreclty()
    {
        std::this_thread::sleep_for(std::chrono::seconds(100));
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
        Catalog result;
        result.lowQualityChunks = generateChunksForQuality(
            cameraName, "low_quality", startPoint, count);

        result.highQualityChunks = generateChunksForQuality(
            cameraName, "hi_quality", startPoint, count);

        return result;
    }

    ChunkDataList generateChunksForQuality(
        const QString& cameraName,
        const QString& quality,
        std::chrono::time_point<std::chrono::system_clock> startPoint,
        int count)
    {
        using namespace std::chrono;

        const int durationMs = 70000;
        qint64 startTimeMs = duration_cast<milliseconds>(startPoint.time_since_epoch()).count();
        ChunkDataList result;

        for (int i = 0; i < count; ++i)
        {
            QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(durationMs);
            QString pathString = QnStorageManager::dateTimeStr(
                startTimeMs, currentTimeZone() / 60,  "/");

            pathString = lit("%2/%1/%3").arg(cameraName).arg(quality).arg(pathString);
            auto fullDirPath = QDir(m_baseDirPath).absoluteFilePath(pathString);
            QString fullFileName =
                closeDirPath(m_baseDirPath) + closeDirPath(pathString) + fileName;

            NX_ASSERT(QDir().mkpath(fullDirPath));
            NX_ASSERT(QFile(fullFileName).open(QIODevice::WriteOnly));

            result.push_back(ChunkData{startTimeMs, durationMs});
            startTimeMs += durationMs + 5;
        }

        return result;
    }
};

TEST_F(Reindex, FastArchiveScan_AllDataRetrieved)
{
    whenServerStarted();
    whenServerStopped();
    givenSomeArchiveOnHdd();
    whenServerStarted();
    thenAllArchiveShouldBeFastScannedCorreclty();
}

} // nx::vms::server::test
