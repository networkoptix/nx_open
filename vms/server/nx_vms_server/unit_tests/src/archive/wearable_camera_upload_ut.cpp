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

struct UploadTestParameters
{
    QString container;
    QString codec;

    UploadTestParameters(const QString& container, const QString& codec):
        container(container),
        codec(codec)
    {}
};

struct FileData
{
    QString path;
    int64_t startTimeMs;

    FileData(const QString& path, int64_t startTimeMs):
        path(path),
        startTimeMs(startTimeMs)
    {}
};

struct TimePeriodWithFiles
{
    QnTimePeriod period;
    QList<FileData> files;
};

class FtWearableCameraUpload:
    public test_support::MediaserverWithStorageFixture,
    public ::testing::WithParamInterface<UploadTestParameters>
{
protected:
    virtual void SetUp() override
    {
        test_support::MediaserverWithStorageFixture::SetUp();
        m_server->addSetting("appserverPassword", "admin");
    }

    void givenSomeFilesOnDisk()
    {
        createTestFiles(4);
        createTimePeriods(2, 2);
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

    void whenFilesAreUploaded()
    {
        for (int i = 0; i < m_testFilePaths.size(); ++i)
            uploadSingleFile(i);
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
    QList<QString> m_testFilePaths;
    const QString m_wearableCameraName = "testCamera";
    QnUuid m_wearableCameraId;
    int64_t m_testFileStartTimeMs;
    QnTimePeriodList m_timePeriodsToCheck;

    void createTimePeriods(int periodCount, int filesPerPeriod)
    {
        for (int p = 0; p < periodCount; ++p)
        {
            m_timePeriodsToCheck.append(
                QnTimePeriod(m_testFileStartTimeMs, kTestFileDurationMs * filesPerPeriod));
        }
    }

    QList<TimePeriodWithFiles> generateTestFileData(const QList<int>& filePerPeriodList)
    {
        using namespace std::chrono;
        for (int i = 0; i < fileCount; ++i)
        {
            auto content = test_support::createTestMkvFileData(
                kTestFileDurationMs / 1000, 640, 480, GetParam().container, GetParam().codec);

            m_testFilePaths.append(
                QDir(m_storagePath).absoluteFilePath(QString("test_media_file_%1.mkv").arg(i)));

            QFile testFile(m_testFilePaths.back());
            ASSERT_TRUE(testFile.open(QIODevice::WriteOnly));
            ASSERT_TRUE(testFile.write(content));
        }
    }

    void uploadSingleFile(int index)
    {
        const QFileInfo fileInfo(m_testFilePaths[index]);
        ASSERT_TRUE(fileInfo.exists());
        const auto fileSize = fileInfo.size();
        const auto ttl = 1000 * 60 * 10;
        const auto chunkSize = fileSize;

        QFile testFile(m_testFilePaths[index]);
        ASSERT_TRUE(testFile.open(QIODevice::ReadOnly));
        const auto fileData = testFile.readAll();

        nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Md5);
        hash.addData(fileData);
        const auto md5 = hash.result();

        using namespace nx::test;
        const auto createUploadUrl =
            QString("/api/downloads/%1?size=%2&chunkSize=%3&md5=%4&ttl=%5&upload=true")
            .arg(fileInfo.fileName()).arg(fileSize).arg(chunkSize)
            .arg(QString::fromLatin1(md5.toHex())).arg(ttl);

        NX_TEST_API_POST(m_server.get(), createUploadUrl, QByteArray());
        NX_TEST_API_POST(
            m_server.get(), QString("/api/downloads/%1/chunks/0").arg(fileInfo.fileName()),
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
};

TEST_P(FtWearableCameraUpload, UploadSingleFile)
{
    givenSomeFilesOnDisk();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenWearableCameraIsCreated();
    whenFilesAreUploaded();
    whenUploadedFileAddedToTheCamera();

    whenPlayArchiveRequestIsIssued();
    thenArchiveShouldBePlayedWithoutGaps();
    thenGetTimePeriodsShouldReturnCorrectResult();
}

INSTANTIATE_TEST_CASE_P(
    UploadTestDifferentContainersAndCodecs, FtWearableCameraUpload,
    ::testing::Values(
        UploadTestParameters("matroska","h263p"), UploadTestParameters("mp4","h263p"),
        UploadTestParameters("mov","h263p"), UploadTestParameters("avi","h263p")));

} // namespace nx::vms::server::test
