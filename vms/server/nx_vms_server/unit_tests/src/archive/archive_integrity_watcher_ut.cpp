#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_with_storage_fixture.h>
#include <test_support/utils.h>
#include <test_support/log_utils.h>
#include <media_server/media_server_module.h>
#include <recorder/archive_integrity_watcher.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/log/log_writers.h>

namespace nx::vms::server::test {

class FtArchiveIntegrityWatcher: public test_support::MediaserverWithStorageFixture
{
protected:
    virtual void SetUp() override
    {
        test_support::MediaserverWithStorageFixture::SetUp();
        m_server->addSetting("appserverPassword", "admin");
        test_support::createTestLogger({"ServerArchiveIntegrityWatcher"}, &m_logBuffer);
    }

    virtual void TearDown() override
    {
        ASSERT_TRUE(m_archiveReader);
        m_archiveReader->stop();
        utils::log::removeLoggers({ utils::log::Filter(QString("ServerArchiveIntegrityWatcher")) });
        nx::utils::log::lockConfiguration();
    }

    void whenSomeFilesAreDeleted()
    {
        auto& camera1Archive = m_generatedArchive[test_support::kCamera1Name];
        for (const auto& chunkData: camera1Archive.highQualityChunks)
            QFile(chunkData.path).remove();

        camera1Archive.highQualityChunks.clear();
    }

    void thenIntegritySignalShouldBeReceived()
    {
        NX_MUTEX_LOCKER lock(&m_archiveIntegrityMutex);
        while (!m_archiveIntegritySignalReceived)
            m_archiveIntegrityWaitCondition.wait(&m_archiveIntegrityMutex);

        m_archiveIntegritySignalReceived = false;
    }

    void thenArchiveShouldBePlayedWithoutGaps()
    {
        // Intentionally empty.
    }

    void whenPlayArchiveRequestIsIssued()
    {
        const auto camera = test_support::getCamera(
            m_server->serverModule()->resourcePool(), test_support::kCamera1Name);

        ASSERT_TRUE((bool)camera);
        m_archiveReader = test_support::createArchiveStreamReader(camera);

        const auto& chunks = m_generatedArchive[test_support::kCamera1Name].lowQualityChunks;
        QnTimePeriod periodToCheck(
            chunks.front().startTimeMs,
            chunks.back().startTimeMs + chunks.back().durationsMs - chunks.front().startTimeMs);
        test_support::checkPlaybackCorrecteness(m_archiveReader.get(), { periodToCheck });
    }

    void whenArchiveIntegritySignalsConnected()
    {
        const auto serverArchiveIntegrityWatcher = dynamic_cast<ServerArchiveIntegrityWatcher*>(
            m_server->serverModule()->archiveIntegrityWatcher());

        connect(
            serverArchiveIntegrityWatcher, &ServerArchiveIntegrityWatcher::fileIntegrityCheckFailed,
            this, &FtArchiveIntegrityWatcher::onArchiveIntegrityBreached, Qt::DirectConnection);
    }

    void whenHoursFoldersSwapped(const QString& cameraName, QnServer::ChunksCatalog quality)
    {
        const auto serverCatalogIt = m_serverCatalog[quality].find(cameraName);
        ASSERT_NE(m_serverCatalog[quality].cend(), serverCatalogIt);

        QString folder1Path;
        QString folder2Path;

        for (const auto& chunk: (*serverCatalogIt)->getChunksUnsafe())
        {
            const auto fullFileName = (*serverCatalogIt)->fullFileName(chunk);
            const auto folderPath = QFileInfo(fullFileName).absolutePath();
            if (folder1Path.isEmpty())
            {
                folder1Path = folderPath;
            }
            else if (folder1Path != folderPath)
            {
                folder2Path = folderPath;
                break;
            }
        }

        ASSERT_FALSE(folder1Path.isEmpty());
        ASSERT_FALSE(folder2Path.isEmpty());

        ASSERT_TRUE(QDir().rename(folder1Path, folder1Path + "_tmp"));
        ASSERT_TRUE(QDir().rename(folder2Path, folder1Path));
        ASSERT_TRUE(QDir().rename(folder1Path + "_tmp", folder2Path));
    }

    void thenLogShouldContain(const QString& message)
    {
        bool logContainsMessage = false;
        for (const auto& m: m_logBuffer->takeMessages())
        {
            if (m.contains(message))
            {
                logContainsMessage = true;
                break;
            }
        }

        ASSERT_TRUE(logContainsMessage);
    }

    void whenTwoFilesSwapped(const QString& cameraName, QnServer::ChunksCatalog quality)
    {
        const auto serverCatalogIt = m_serverCatalog[quality].find(cameraName);
        ASSERT_NE(m_serverCatalog[quality].cend(), serverCatalogIt);

        QString file1Path;
        QString file2Path;

        for (const auto& chunk: (*serverCatalogIt)->getChunksUnsafe())
        {
            const auto fullFileName = (*serverCatalogIt)->fullFileName(chunk);
            if (file1Path.isEmpty())
            {
                file1Path = fullFileName;
            }
            else if (file1Path != fullFileName)
            {
                file2Path = fullFileName;
                break;
            }
        }

        ASSERT_FALSE(file1Path.isEmpty());
        ASSERT_FALSE(file2Path.isEmpty());

        ASSERT_TRUE(QFile().rename(file1Path, file1Path + "_tmp"));
        ASSERT_TRUE(QFile().rename(file2Path, file1Path));
        ASSERT_TRUE(QDir().rename(file1Path + "_tmp", file2Path));
    }

private:
    QnMutex m_archiveIntegrityMutex;
    QnWaitCondition m_archiveIntegrityWaitCondition;
    bool m_archiveIntegritySignalReceived = false;
    std::unique_ptr<QnArchiveStreamReader> m_archiveReader;
    utils::log::Buffer* m_logBuffer = nullptr;

    void onArchiveIntegrityBreached(const QnStorageResourcePtr& storage)
    {
        NX_MUTEX_LOCKER lock(&m_archiveIntegrityMutex);
        m_archiveIntegritySignalReceived = true;
        m_archiveIntegrityWaitCondition.wakeOne();
    }
};

TEST_F(FtArchiveIntegrityWatcher, RemovingFiles)
{
    givenSomeArchiveOnHdd();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();

    whenArchiveIntegritySignalsConnected();

    whenSomeFilesAreDeleted();
    whenPlayArchiveRequestIsIssued();
    thenArchiveShouldBePlayedWithoutGaps();
    thenIntegritySignalShouldBeReceived();
}

TEST_F(FtArchiveIntegrityWatcher, SwappingHoursFolders)
{
    givenSomeArchiveOnHdd({ test_support::CameraChunksInfo(test_support::kCamera1Name, 120) });
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();

    whenArchiveIntegritySignalsConnected();

    whenHoursFoldersSwapped(test_support::kCamera1Name, QnServer::HiQualityCatalog);
    whenPlayArchiveRequestIsIssued();
    thenArchiveShouldBePlayedWithoutGaps();
    thenIntegritySignalShouldBeReceived();
    thenLogShouldContain("File is present in the DB but missing in the archive");
}

TEST_F(FtArchiveIntegrityWatcher, SwappingTwoFiles)
{
    givenSomeArchiveOnHdd({ test_support::CameraChunksInfo(test_support::kCamera1Name, 10) });
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();

    whenArchiveIntegritySignalsConnected();

    whenTwoFilesSwapped(test_support::kCamera1Name, QnServer::HiQualityCatalog);
    whenPlayArchiveRequestIsIssued();
    thenArchiveShouldBePlayedWithoutGaps();
    thenIntegritySignalShouldBeReceived();
    thenLogShouldContain("integrity problem");
}

} // namespace nx::vms::server::test
