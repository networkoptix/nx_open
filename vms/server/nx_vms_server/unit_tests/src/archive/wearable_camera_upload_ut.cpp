#include <gtest/gtest.h>

#include <test_support/mediaserver_with_storage_fixture.h>
#include <test_support/storage_utils.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <api/model/wearable_camera_reply.h>

namespace nx::vms::server::test {

class FtWearableCameraUpload: public test_support::MediaserverWithStorageFixture
{
protected:
    void givenSingleFileOnDisk()
    {
        using namespace std::chrono;
        auto content = test_support::createTestMkvFileData(60, 640, 480);
        m_testFileStartTimeMs =
            duration_cast<milliseconds>((system_clock::now() - hours(24)).time_since_epoch()).count();

        m_testFileName = QString("%1_%2.mkv").arg(m_testFileStartTimeMs).arg(60000);
        m_testFilePath = QDir(m_storagePath).absoluteFilePath(m_testFileName);
        test_support::updateMkvMetaData(content, m_testFileStartTimeMs);

        QFile testFile(m_testFilePath);
        ASSERT_TRUE(testFile.open(QIODevice::WriteOnly));
        ASSERT_TRUE(testFile.write(content));
    }

    void whenWearableCameraIsCreated()
    {
        using namespace nx::test;
        QnJsonRestResult addCameraRestResult;
        QByteArray response;
        const auto url = QString("/api/wearableCamera/add?name=%1").arg(m_wearableCameraName);

        NX_TEST_API_POST(
            m_server.get(), url, QByteArray(),nullptr, network::http::StatusCode::ok, "admin",
            "admin", &response);

        addCameraRestResult = QJson::deserialized<QnJsonRestResult>(response);
        QnWearableCameraReply reply = addCameraRestResult.deserialized<QnWearableCameraReply>();
        m_wearableCameraId = reply.id;
    }

    void whenFileIsUploaded()
    {
        QFileInfo fileInfo(m_testFilePath);
        ASSERT_TRUE(fileInfo.exists());
        const auto fileSize = fileInfo.size();
        const auto ttl = 1000 * 60 * 10;
        const auto chunkSize = fileSize;

        QFile testFile(m_testFilePath);
        ASSERT_TRUE(testFile.open(QIODevice::ReadOnly));
        const auto fileData = testFile.readAll();

        nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Md5);
        hash.addData(fileData);
        const auto md5 = hash.result();

        using namespace nx::test;
        const auto createUploadUrl =
            QString("/api/downloads/%1?size=%2&chunkSize=%3&md5=%4&ttl=%5&upload=true")
                .arg(m_testFileName).arg(fileSize).arg(chunkSize)
                .arg(QString::fromLatin1(md5.toHex())).arg(ttl);

        NX_TEST_API_POST(m_server.get(), createUploadUrl, QByteArray());
        NX_TEST_API_POST(
            m_server.get(), QString("/api/downloads/%1/chunks/0").arg(m_testFileName),
            fileData, nullptr, nx::network::http::StatusCode::ok, "admin", "admin", nullptr,
            "application/octet-stream");

        using namespace vms::common::p2p::downloader;
        QnJsonRestResult uploadResult;
        NX_TEST_API_GET(m_server.get(), QString("/api/downloads/%1/status").arg(m_testFileName), &uploadResult);
        ASSERT_EQ(QnRestResult::Error::NoError, uploadResult.error);
        const auto fileInformation = uploadResult.deserialized<FileInformation>();
        ASSERT_TRUE(fileInformation.status == FileInformation::Status::downloaded);
    }

    void whenPlayArchiveRequestIsIssued()
    {
        const auto camera = test_support::getCamera(
            m_server->serverModule()->resourcePool(), m_wearableCameraName);

        ASSERT_TRUE((bool)camera);
        const auto archiveReader = test_support::createArchiveStreamReader(camera);

        QnTimePeriod periodToCheck(m_testFileStartTimeMs, 60000);
        test_support::checkPlaybackCorrecteness(archiveReader.get(), { periodToCheck });
    }

    void thenArchiveShouldBePlayedWithoutGaps()
    {
        // Intentionally empty.
    }

    void whenUploadedFileAddedToTheCamera()
    {
        using namespace nx::test;
        NX_TEST_API_POST(
            m_server.get(),
            QString("/api/wearableCamera/lock/?cameraId=%1&ttl=60000&userId=%2")
                .arg(m_wearableCameraId.toString()).arg(m_lockId),
            QByteArray());
    }

private:
    QString m_testFilePath;
    QString m_testFileName;
    const QString m_wearableCameraName = "testCamera";
    const QString m_lockId = "testUser";
    QnUuid m_wearableCameraId;
    int64_t m_testFileStartTimeMs;
};

TEST_F(FtWearableCameraUpload, UploadSingleFile)
{
    givenSingleFileOnDisk();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenWearableCameraIsCreated();
    whenFileIsUploaded();
    whenUploadedFileAddedToTheCamera();

    whenPlayArchiveRequestIsIssued();
    thenArchiveShouldBePlayedWithoutGaps();
}

} // namespace nx::vms::server::test
