#include <test_support/mediaserver_with_storage_fixture.h>
#include <api/model/backup_status_reply.h>
#include <nx/mediaserver/camera_mock.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test {

class FtArchiveBackup: public test_support::MediaserverWithStorageFixture
{
protected:
    virtual void SetUp() override
    {
        test_support::MediaserverWithStorageFixture::SetUp();
        m_backupStoragePath = initStoragePath("backup_storage");
    }

    virtual void TearDown() override
    {
        test_support::MediaserverWithStorageFixture::TearDown();
        cleanupStoragePath(m_backupStoragePath);
    }

    void whenBackupStorageAdded()
    {
        NX_DEBUG(this, "Adding backup storage");
        test_support::addStorage(
            m_server.get(), m_backupStoragePath, QnServer::StoragePool::Backup);
        NX_DEBUG(this, "Backup storage successfully added");
    }

    void whenBackupForcefullyStarted()
    {
        using namespace nx::test;
        QnJsonRestResult reply;
        NX_DEBUG(this, "Starting backup");
        NX_TEST_API_GET(m_server.get(), "/api/backupControl?action=start", &reply);
        ASSERT_EQ(Qn::BackupState_InProgress, reply.deserialized<QnBackupStatusData>().state);
        NX_INFO(this, "Backup started");
    }

    void whenFakeCamerasForArchiveAdded()
    {
        m_server->serverModule()->globalSettings()->setBackupQualities(
            Qn::CameraBackupQuality::CameraBackup_Both);
        for (auto it = m_generatedArchive.cbegin(); it != m_generatedArchive.cend(); ++it)
        {
            auto camera = QnVirtualCameraResourcePtr(
                new nx::vms::server::resource::test::CameraMock(m_server->serverModule()));
            camera->setIdUnsafe(QnUuid::createUuid());
            camera->setPhysicalId(it.key());
            camera->setBackupQualities(Qn::CameraBackupQuality::CameraBackup_Both);
            m_server->serverModule()->resourcePool()->addResource(camera);
        }
    }

    void thenArchiveShouldBeBackedUp()
    {
        using namespace nx::test;
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            QnJsonRestResult reply;
            NX_TEST_API_GET(m_server.get(), "/api/backupControl", &reply);
            if (reply.deserialized<QnBackupStatusData>().state == Qn::BackupState_None)
                break;
        }
        const auto manager = m_server->serverModule()->backupStorageManager();
        for (auto it = m_generatedArchive.cbegin(); it != m_generatedArchive.cend(); ++it)
        {
            for (auto q: {QnServer::LowQualityCatalog, QnServer::HiQualityCatalog})
            {
                const auto backupChunks = manager->getFileCatalog(it.key(), q)->getChunksUnsafe();
                const test_support::ChunkDataList* generatedChunks = nullptr;
                if (q == QnServer::LowQualityCatalog)
                    generatedChunks = &it.value().lowQualityChunks;
                else
                    generatedChunks = &it.value().highQualityChunks;
                ASSERT_NE(0, generatedChunks->size());
                ASSERT_EQ(generatedChunks->size(), backupChunks.size());
                for (size_t i = 0; i < generatedChunks->size(); ++i)
                    ASSERT_EQ(generatedChunks->at(i).startTimeMs, backupChunks[i].startTimeMs);
            }
        }
    }

private:
    QString m_backupStoragePath;
};

TEST_F(FtArchiveBackup, OnScheduleForcefulStart)
{
    givenSomeArchiveOnHdd();
    whenServerStarted();
    whenFakeCamerasForArchiveAdded();
    whenBackupStorageAdded();
    thenArchiveShouldBeScannedCorreclty();
    whenBackupForcefullyStarted();
    thenArchiveShouldBeBackedUp();
}

} // namespace
