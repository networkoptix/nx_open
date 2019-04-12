#include <gtest/gtest.h>

#include <test_support/mediaserver_with_storage_fixture.h>
#include <test_support/storage_utils.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <api/model/wearable_camera_reply.h>
#include <api/model/wearable_status_reply.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

namespace nx::vms::server::test {

static const int kTestFileDurationMs = 60000;

class FtWearableCameraUpload: public test_support::MediaserverWithStorageFixture
{
protected:
    virtual void SetUp() override
    {
        test_support::MediaserverWithStorageFixture::SetUp();
        m_server->addSetting("appserverPassword", "admin");
    }

    void givenSingleFileOnDisk()
    {
        using namespace std::chrono;
        auto content = test_support::createTestMkvFileData(kTestFileDurationMs / 1000, 640, 480);
        m_testFileStartTimeMs =
            duration_cast<milliseconds>((system_clock::now() - hours(24)).time_since_epoch()).count();

        m_testFileName = QString("%1_%2.mkv").arg(m_testFileStartTimeMs).arg(kTestFileDurationMs);
        m_testFilePath = QDir(m_storagePath).absoluteFilePath(m_testFileName);
        test_support::updateMkvMetaData(content, m_testFileStartTimeMs);

        QFile testFile(m_testFilePath);
        ASSERT_TRUE(testFile.open(QIODevice::WriteOnly));
        ASSERT_TRUE(testFile.write(content));

        m_timePeriodsToCheck.append(QnTimePeriod(m_testFileStartTimeMs, kTestFileDurationMs));
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
        NX_TEST_API_GET(
            m_server.get(), QString("/api/downloads/%1/status").arg(m_testFileName), &uploadResult);
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
        test_support::checkPlaybackCorrecteness(archiveReader.get(), m_timePeriodsToCheck);
    }

    void thenArchiveShouldBePlayedWithoutGaps()
    {
        // Intentionally empty.
    }

    void whenUploadedFileAddedToTheCamera()
    {
        using namespace nx::test;
        QByteArray rawResponse;
        const auto adminId = m_server->serverModule()->resourcePool()->getAdministrator()->getId();

        NX_TEST_API_POST(
            m_server.get(),
            QString("/api/wearableCamera/lock?cameraId=%1&ttl=60000&userId=%2")
                .arg(m_wearableCameraId.toString()).arg(adminId.toString()),
            QByteArray(), nullptr, network::http::StatusCode::ok, "admin", "admin", &rawResponse);

        QnJsonRestResult restResult = QJson::deserialized<QnJsonRestResult>(rawResponse);
        auto wearableStatusReply = restResult.deserialized<QnWearableStatusReply>();

        NX_TEST_API_POST(
            m_server.get(),
            QString("/api/wearableCamera/consume?cameraId=%1&token=%2&uploadId=%3&startTime=%4")
                .arg(m_wearableCameraId.toString()).arg(wearableStatusReply.token.toString())
                .arg(m_testFileName).arg(m_testFileStartTimeMs),
            QByteArray());

        while (true)
        {
            NX_TEST_API_POST(
                m_server.get(),
                QString("/api/wearableCamera/extend?cameraId=%1&token=%2&userId=%3&ttl=60000")
                    .arg(m_wearableCameraId.toString()).arg(wearableStatusReply.token.toString())
                    .arg(adminId.toString()),
                QByteArray(), nullptr, network::http::StatusCode::ok, "admin", "admin", &rawResponse);

            QnJsonRestResult restResult = QJson::deserialized<QnJsonRestResult>(rawResponse);
            const auto wearableStatusReply = restResult.deserialized<QnWearableStatusReply>();

            ASSERT_TRUE(wearableStatusReply.success);
            if (wearableStatusReply.progress == 100)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        NX_TEST_API_POST(
            m_server.get(),
            QString("/api/wearableCamera/release?cameraId=%1&token=%2")
                .arg(m_wearableCameraId.toString()).arg(wearableStatusReply.token.toString()),
            QByteArray());
    }

    void thenGetTimePeriodsShouldReturnCorrectResult()
    {
        using namespace nx::test;
        QnJsonRestResult timePeriodsResult;
        NX_TEST_API_GET(
            m_server.get(),
            QString("/ec2/recordedTimePeriods?cameraId=%1").arg(m_wearableCameraId.toString()),
            &timePeriodsResult);

        ASSERT_EQ(timePeriodsResult.error, network::rest::Result::NoError);
        MultiServerPeriodDataList serverTimePeriods =
            timePeriodsResult.deserialized<MultiServerPeriodDataList>();
        ASSERT_EQ(m_timePeriodsToCheck.size(), serverTimePeriods[0].periods.size());

        const auto& testCameraTimePeriods = serverTimePeriods[0].periods;
        for (int i = 0; i < m_timePeriodsToCheck.size(); ++i)
        {
            ASSERT_EQ(m_timePeriodsToCheck[i].startTimeMs, testCameraTimePeriods[i].startTimeMs);
            ASSERT_LE(m_timePeriodsToCheck[i].endTimeMs() - testCameraTimePeriods[i].endTimeMs(), 40);
        }
    }

private:
    QString m_testFilePath;
    QString m_testFileName;
    const QString m_wearableCameraName = "testCamera";
    QnUuid m_wearableCameraId;
    int64_t m_testFileStartTimeMs;
    QnTimePeriodList m_timePeriodsToCheck;
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
    thenGetTimePeriodsShouldReturnCorrectResult();
}

} // namespace nx::vms::server::test
