#pragma once

#include <test_support/mediaserver_launcher.h>
#include "storage_utils.h"
#include <recorder/storage_manager.h>
#include <api/test_api_requests.h>
#include <rest/server/json_rest_result.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test::test_support {

static const QString kCamera1Name = "camera1";
static const QString kCamera2Name = "camera2";

struct CameraChunksInfo
{
    QString cameraName;
    int chunksCount;

    CameraChunksInfo(const QString& cameraName, int chunksCount):
        cameraName(cameraName),
        chunksCount(chunksCount)
    {}
};

class MediaserverWithStorageFixture: public QObject, public ::testing::Test
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
            this, &MediaserverWithStorageFixture::onReindexFinished, Qt::DirectConnection);

        test_support::addTestStorage(m_server.get(), m_storagePath);
    }

    void whenServerStopped()
    {
        ASSERT_TRUE(m_server->stop());
    }

    void givenSomeArchiveOnHdd(
        const QList<CameraChunksInfo>& cameraChunks = { CameraChunksInfo(kCamera1Name, 10), CameraChunksInfo(kCamera2Name, 5) })
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

        for (const auto& cameraChunksInfo: cameraChunks)
            addArchiveDataForCamera(cameraChunksInfo.cameraName, cameraChunksInfo.chunksCount, startTimeMs);
    }

    void addArchiveDataForCamera(const QString& cameraName, int count, qint64 startTimeMs)
    {
        const auto newCatalog = test_support::generateCameraArchive(
            m_storagePath, cameraName, startTimeMs, count);

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

protected:
    std::unique_ptr<MediaServerLauncher> m_server;
    test_support::Archive m_generatedArchive;
    QString m_storagePath;
    QMap<QString, DeviceFileCatalogPtr> m_serverCatalog[QnServer::ChunksCatalogCount];
    QnMutex m_reindexMutex;
    QnWaitCondition m_reindexWaitCondition;
    bool m_reindexFinished = false;

    void onReindexFinished(QnSystemHealth::MessageType message)
    {
        NX_MUTEX_LOCKER lock(&m_reindexMutex);
        acquireServerCatalog(m_server->serverModule()->normalStorageManager(), kCamera1Name);
        acquireServerCatalog(m_server->serverModule()->normalStorageManager(), kCamera2Name);

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
        const test_support::ChunkDataList& generatedChunks)
    {
        const auto serverCatalogIt = m_serverCatalog[QnServer::LowQualityCatalog].find(cameraName);
        ASSERT_NE(m_serverCatalog[quality].cend(), serverCatalogIt);

        const auto& serverChunks = serverCatalogIt.value()->getChunksUnsafe();
        ASSERT_EQ(generatedChunks.size(), serverChunks.size());
        for (int i = 0; i < serverChunks.size(); ++i)
            ASSERT_EQ(generatedChunks[i].startTimeMs, serverChunks[i].chunk().startTimeMs);
    }
};
} // namespace nx::vms::server::test::test_support
