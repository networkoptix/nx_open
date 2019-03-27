#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <test_support/utils.h>
#include <health/system_health.h>
#include <recorder/storage_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <api/test_api_requests.h>
#include <rest/server/json_rest_result.h>

namespace nx::vms::server::test {

namespace {

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

static ChunkDataList generateChunksForQuality(
    const QString& baseDir,
    const QString& cameraName,
    const QString& quality,
    qint64 startTimeMs,
    int count)
{
    using namespace std::chrono;

    const int durationMs = 70000;
    ChunkDataList result;

    for (int i = 0; i < count; ++i)
    {
        QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(durationMs);
        QString pathString = QnStorageManager::dateTimeStr(
            startTimeMs, currentTimeZone() / 60,  "/");

        pathString = lit("%2/%1/%3").arg(cameraName).arg(quality).arg(pathString);
        auto fullDirPath = QDir(baseDir).absoluteFilePath(pathString);
        QString fullFileName =
            closeDirPath(baseDir) + closeDirPath(pathString) + fileName;

        NX_ASSERT(QDir().mkpath(fullDirPath));
        NX_ASSERT(QFile(fullFileName).open(QIODevice::WriteOnly));

        result.push_back(ChunkData{startTimeMs, durationMs});
        startTimeMs += durationMs + 5;
    }

    return result;
}

static Catalog generateCameraArchive(
    const QString& baseDir,
    const QString& cameraName,
    qint64 startTimeMs,
    int count)
{
    Catalog result;
    result.lowQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "low_quality", startTimeMs, count);

    result.highQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "hi_quality", startTimeMs, count);

    return result;
}

} // namespace

class FtReindex: public QObject, public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_server.reset(new MediaServerLauncher());

        m_storagePath = QDir(m_server->dataDir()).absoluteFilePath("test_storage");
        QDir(m_storagePath).removeRecursively();
        QDir().mkpath(m_storagePath);
    }

    virtual void TearDown() override
    {
        if (!m_storagePath.isEmpty())
            QDir(m_storagePath).removeRecursively();
    }

    void whenServerStarted()
    {
        ASSERT_TRUE(m_server->start());
        QObject::connect(
            m_server->serverModule()->normalStorageManager(), &QnStorageManager::rebuildFinished,
            this, &FtReindex::onReindexFinished, Qt::DirectConnection);

        ut::utils::addTestStorage(m_server.get(), m_storagePath);
    }

    void whenServerStopped()
    {
        ASSERT_TRUE(m_server->stop());
    }

    void givenSomeArchiveOnHdd()
    {
        qint64 startTimeMs = 0;
        for (const auto& catalog : m_generatedArchive)
        {
            if (!catalog.lowQualityChunks.empty() && startTimeMs < catalog.lowQualityChunks.back().startTimeMs)
                startTimeMs = catalog.lowQualityChunks.back().startTimeMs;

            if (!catalog.highQualityChunks.empty() && startTimeMs < catalog.highQualityChunks.back().startTimeMs)
                startTimeMs = catalog.highQualityChunks.back().startTimeMs;
        }

        using namespace std::chrono;
        if (startTimeMs == 0)
        {
            startTimeMs = duration_cast<milliseconds>(
                (system_clock::now() - hours(24 * 10)).time_since_epoch()).count();
        }
        else
        {
            startTimeMs += duration_cast<milliseconds>(hours(24)).count();
        }

        addArchiveDataForCamera("camera1", 10, startTimeMs);
        addArchiveDataForCamera("camera2", 5, startTimeMs);
    }

    void addArchiveDataForCamera(const QString& cameraName, int count, qint64 startTimeMs)
    {
        const auto newCatalog = generateCameraArchive(m_storagePath, cameraName, startTimeMs, count);

        std::copy(
            newCatalog.lowQualityChunks.cbegin(), newCatalog.lowQualityChunks.cend(),
            std::back_inserter(m_generatedArchive[cameraName].lowQualityChunks));

        std::copy(
            newCatalog.highQualityChunks.cbegin(), newCatalog.highQualityChunks.cend(),
            std::back_inserter(m_generatedArchive[cameraName].highQualityChunks));
    }

    void thenArchiveShouldBeScannedCorreclty()
    {
        NX_MUTEX_LOCKER lock(&m_reindexMutex);
        while (!m_reindexFinished)
            m_reindexWaitCondition.wait(&m_reindexMutex);

        for (auto it = m_generatedArchive.cbegin(); it != m_generatedArchive.cend(); ++it)
        {
            assertDataEquality(QnServer::LowQualityCatalog, it.key(), it.value().lowQualityChunks);
            assertDataEquality(QnServer::HiQualityCatalog, it.key(), it.value().highQualityChunks);
        }

        m_reindexFinished = false;
    }

    void whenSomeArchiveDataAdded()
    {
        givenSomeArchiveOnHdd();
    }

    void whenReindexRequestIssued()
    {
        using namespace nx::test;
        QnJsonRestResult result;
        NX_TEST_API_GET(m_server.get(), "/api/rebuildArchive?action=start", &result);
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
    Archive m_generatedArchive;
    QString m_storagePath;
    QMap<QString, DeviceFileCatalogPtr> m_serverCatalog[QnServer::ChunksCatalogCount];
    QnMutex m_reindexMutex;
    QnWaitCondition m_reindexWaitCondition;
    bool m_reindexFinished = false;

    void onReindexFinished(QnSystemHealth::MessageType message)
    {
        NX_MUTEX_LOCKER lock(&m_reindexMutex);
        acquireServerCatalog(m_server->serverModule()->normalStorageManager(), "camera1");
        acquireServerCatalog(m_server->serverModule()->normalStorageManager(), "camera2");

        m_reindexFinished = true;
        m_reindexWaitCondition.wakeOne();
    }

    void acquireServerCatalog(QnStorageManager* storageManager, const QString& cameraName)
    {
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i)
        {
            m_serverCatalog[i][cameraName] = storageManager->getFileCatalog(
                cameraName, (QnServer::ChunksCatalog) i);
        }
    }

    void assertDataEquality(
        QnServer::ChunksCatalog quality,
        const QString& cameraName,
        const ChunkDataList& generatedChunks)
    {
        const auto serverCatalogIt = m_serverCatalog[QnServer::LowQualityCatalog].find(cameraName);
        ASSERT_NE(m_serverCatalog[quality].cend(), serverCatalogIt);

        const auto& serverChunks = serverCatalogIt.value()->getChunksUnsafe();
        ASSERT_EQ(generatedChunks.size(), serverChunks.size());
        for (int i = 0; i < serverChunks.size(); ++i)
            ASSERT_EQ(generatedChunks[i].startTimeMs, serverChunks[i].startTimeMs);
    }
};

TEST_F(FtReindex, FastArchiveScan_AllDataRetrieved)
{
    givenSomeArchiveOnHdd();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();
}

TEST_F(FtReindex, FastArchiveScan_PartialDataRetrieved)
{
    givenSomeArchiveOnHdd();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenServerStopped();
    whenSomeArchiveDataAdded();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenServerStopped();
    whenSomeArchiveDataAdded();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();
}

TEST_F(FtReindex, FullArchiveScan_AllDataRetrieved)
{
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenSomeArchiveDataAdded();
    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();

    whenServerStopped();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenSomeArchiveDataAdded();
    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();
}

} // nx::vms::server::test
