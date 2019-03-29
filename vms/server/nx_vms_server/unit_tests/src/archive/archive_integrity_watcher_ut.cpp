#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_with_storage_fixture.h>
#include <media_server/media_server_module.h>
#include <recorder/archive_integrity_watcher.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/archive_stream_reader.h>

namespace nx::vms::server::test {

class FtArchiveIntegrityWatcher:
    public test_support::MediaserverWithStorageFixture,
    public QnAbstractMediaDataReceptor
{
protected:
    virtual void SetUp() override
    {
        test_support::MediaserverWithStorageFixture::SetUp();
        m_server->addSetting("appserverPassword", "admin");
    }

    virtual void TearDown() override
    {
        m_archiveReader->stop();
        m_archiveReader->removeDataProcessor(this);
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
        // #TODO #akulikov Uncomment and fix
        //NX_MUTEX_LOCKER lock(&m_archivePlaybackMutex);
        //while (!m_archivePlayedTillTheEnd)
        //    m_archivePlaybackWaitCondition.wait(&m_archivePlaybackMutex);

        //m_archivePlayedTillTheEnd = false;
    }

    void whenPlayArchiveRequestIsIssued()
    {
        const auto allCameras = m_server->serverModule()->resourcePool()->getAllCameras();
        QnVirtualCameraResourcePtr camera1;
        for (const auto camera : allCameras)
        {
            if (camera->getUniqueId() == test_support::kCamera1Name)
            {
                camera1 = camera;
                break;
            }
        }

        ASSERT_TRUE((bool)camera1);
        m_archiveReader = test_support::createArchiveStreamReader(camera1);
        m_archiveReader->addDataProcessor(this);
        m_archiveReader->start();

        m_archiveStartTime =
            m_generatedArchive[test_support::kCamera1Name].highQualityChunks.front().startTimeMs;
        m_archiveEndTime =
            m_generatedArchive[test_support::kCamera1Name].highQualityChunks.back().startTimeMs;
    }

    void whenArchiveIntegritySignalsConnected()
    {
        const auto serverArchiveIntegrityWatcher = dynamic_cast<ServerArchiveIntegrityWatcher*>(
            m_server->serverModule()->archiveIntegrityWatcher());

        connect(
            serverArchiveIntegrityWatcher, &ServerArchiveIntegrityWatcher::fileIntegrityCheckFailed,
            this, &FtArchiveIntegrityWatcher::onArchiveIntegrityBreached, Qt::DirectConnection);
    }

private:
    QnMutex m_archiveIntegrityMutex;
    QnWaitCondition m_archiveIntegrityWaitCondition;
    bool m_archiveIntegritySignalReceived = false;
    QnMutex m_archivePlaybackMutex;
    QnWaitCondition m_archivePlaybackWaitCondition;
    bool m_archivePlayedTillTheEnd = false;
    std::unique_ptr<QnArchiveStreamReader> m_archiveReader;
    int64_t m_archiveStartTime;
    int64_t m_archiveEndTime;

    void onArchiveIntegrityBreached(const QnStorageResourcePtr& storage)
    {
        NX_MUTEX_LOCKER lock(&m_archiveIntegrityMutex);
        m_archiveIntegritySignalReceived = true;
        m_archiveIntegrityWaitCondition.wakeOne();
    }

    virtual bool canAcceptData() const override
    {
        return true;
    }

    virtual void putData(const QnAbstractDataPacketPtr& data) override
    {
        qDebug() << data->timestamp;
        ASSERT_LE(data->timestamp - m_archiveStartTime, 1000000);
        m_archiveStartTime = data->timestamp;

        if (m_archiveStartTime - m_archiveEndTime < 1000000)
        {
            NX_MUTEX_LOCKER lock(&m_archivePlaybackMutex);
            m_archivePlaybackWaitCondition.wakeOne();
        }
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

} // namespace nx::vms::server::test