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

struct TimePeriodData
{
    QnTimePeriod timePeriod;
    QList<int64_t> startTimeMsList;
};

struct GeneratedFileData
{
    QByteArray fileData;
    QList<TimePeriodData> periods;
};

static QnTimePeriodList timePeriodListFromGeneratedData(const GeneratedFileData& generatedData)
{
    QnTimePeriodList result;
    for (const auto& timePeriodData: generatedData.periods)
        result.append(timePeriodData.timePeriod);
    return result;
}

static GeneratedFileData generateTestFileData(
    const UploadTestParameters& testParameters, const QString& basePath,
    const QList<int>& filePerPeriodList)
{
    using namespace std::chrono;
    const int64_t globalStartTimeMs =
        duration_cast<milliseconds>((system_clock::now() - hours(24) * 14).time_since_epoch()).count();
    auto fileStartTimeMs = globalStartTimeMs;

    GeneratedFileData result;
    result.fileData = test_support::createTestMkvFileData(
        kTestFileDurationMs / 1000, 640, 480, testParameters.container, testParameters.codec);

    for (int filesInPeriod : filePerPeriodList)
    {
        if (fileStartTimeMs != globalStartTimeMs)
            fileStartTimeMs += 120000;

        TimePeriodData timePeriodData;
        timePeriodData.timePeriod.startTimeMs = fileStartTimeMs;
        timePeriodData.timePeriod.setEndTimeMs(fileStartTimeMs);

        for (int i = 0; i < filesInPeriod; ++i)
        {
            timePeriodData.startTimeMsList.append(fileStartTimeMs);
            fileStartTimeMs += kTestFileDurationMs;
            timePeriodData.timePeriod.setDuration(
                timePeriodData.timePeriod.duration() + milliseconds(kTestFileDurationMs));
        }

        if (!timePeriodData.timePeriod.isEmpty())
            result.periods.append(timePeriodData);
    }

    return result;
}

class FileUploader
{
public:
    FileUploader(MediaServerLauncher* launcher): m_launcher(launcher) {}

    QnUuid lockCamera(const QnUuid& cameraId)
    {
        const auto urlPattern = QString("/api/wearableCamera/lock?cameraId=%1&ttl=60000&userId=%2");
        const auto url = urlPattern.arg(cameraId.toString()).arg(adminId());
        return doWearableCameraPostRequest(url).token;
    }

    void releaseCamera(const QnUuid& cameraId, const QnUuid& token)
    {
        doWearableCameraPostRequest(
            QString("/api/wearableCamera/release?cameraId=%1&token=%2")
                .arg(cameraId.toString()).arg(token.toString()));
    }

    void addFile(
        const QnUuid& cameraId, const QnUuid& token, const QString& fileName,
        const QByteArray& fileData, int64_t startTimeMs)
    {
        uploadFile(fileName, fileData);
        doWearableCameraPostRequest(
            QString("/api/wearableCamera/consume?cameraId=%1&token=%2&uploadId=%3&startTime=%4")
                .arg(cameraId.toString()).arg(token.toString()).arg(fileName)
                .arg(startTimeMs));

        while (true)
        {
            const auto reply = doWearableCameraPostRequest(
                QString("/api/wearableCamera/extend?cameraId=%1&token=%2&userId=%3&ttl=60000")
                    .arg(cameraId.toString()).arg(token.toString())
                    .arg(adminId()));

            ASSERT_TRUE(reply.success);
            if (reply.progress == 100)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

private:
    MediaServerLauncher* m_launcher;

    QnWearableStatusReply doWearableCameraPostRequest(const QString& url)
    {
        QByteArray rawResponse;
        [&]()
        {
            using namespace nx::test;
            NX_TEST_API_POST(
                m_launcher, url, QByteArray(), nullptr, network::http::StatusCode::ok, "admin",
                "admin", &rawResponse);
        }();

        bool success;
        QnJsonRestResult restResult = QJson::deserialized<QnJsonRestResult>(
            rawResponse, QnJsonRestResult(), &success);

        if (!success || restResult.error != network::rest::Result::NoError)
            throw std::runtime_error("doPostRequest: response failed");

        QnWearableStatusReply reply = restResult.deserialized<QnWearableStatusReply>(&success);
        if (!success)
            throw std::runtime_error("doPostRequest: Failed to deserialized QnWearableStatusReply");

        return reply;
    }

    QString adminId() const
    {
        return m_launcher->serverModule()->resourcePool()->getAdministrator()->getId().toString();
    }

    void uploadFile(const QString& fileName, const QByteArray& fileData)
    {
        using namespace nx::test;
        NX_TEST_API_POST(m_launcher, composeUploadUrl(fileName, fileData), QByteArray());
        NX_TEST_API_POST(
            m_launcher, QString("/api/downloads/%1/chunks/0").arg(fileName),
            fileData, nullptr, nx::network::http::StatusCode::ok, "admin", "admin", nullptr,
            "application/octet-stream");

        using namespace vms::common::p2p::downloader;
        QnJsonRestResult uploadResult;
        NX_TEST_API_GET(
            m_launcher, QString("/api/downloads/%1/status").arg(fileName), &uploadResult);

        ASSERT_EQ(QnRestResult::Error::NoError, uploadResult.error);
        const auto fileInformation = uploadResult.deserialized<FileInformation>();
        ASSERT_TRUE(fileInformation.status == FileInformation::Status::downloaded);
    }

    QString composeUploadUrl(const QString& fileName, const QByteArray& fileData)
    {
        const auto fileSize = fileData.size();
        const auto ttl = 1000 * 60 * 10;
        const auto chunkSize = fileSize;

        nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Md5);
        hash.addData(fileData);
        const auto md5 = hash.result();

        return QString("/api/downloads/%1?size=%2&chunkSize=%3&md5=%4&ttl=%5&upload=true")
            .arg(fileName).arg(fileSize).arg(chunkSize).arg(QString::fromLatin1(md5.toHex()))
            .arg(ttl);
    }
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
        m_uploader.reset(new FileUploader(m_server.get()));
    }

    void givenSomeFilesOnDisk()
    {
        m_generatedData = generateTestFileData(GetParam(), m_storagePath, {3, 2, 4});
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
        const auto token = m_uploader->lockCamera(m_wearableCameraId);
        for (const auto& timePeriodData: m_generatedData.periods)
        {
            for (int64_t startTimeMs: timePeriodData.startTimeMsList)
            {
                m_uploader->addFile(
                    m_wearableCameraId, token, QString::number(startTimeMs),
                    m_generatedData.fileData, startTimeMs);
            }
        }
        m_uploader->releaseCamera(m_wearableCameraId, token);
    }

    void whenPlayArchiveRequestIsIssued()
    {
        const auto camera = test_support::getCamera(
            m_server->serverModule()->resourcePool(), m_wearableCameraName);

        ASSERT_TRUE((bool)camera);
        const auto archiveReader = test_support::createArchiveStreamReader(camera);
        test_support::checkPlaybackCorrecteness(
            archiveReader.get(), timePeriodListFromGeneratedData(m_generatedData));
    }

    void thenArchiveShouldBePlayedWithoutGaps()
    {
        // Intentionally empty.
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

        const auto referenceTimePeriods = timePeriodListFromGeneratedData(m_generatedData);
        ASSERT_EQ(referenceTimePeriods.size(), serverTimePeriods[0].periods.size());

        const auto& testCameraTimePeriods = serverTimePeriods[0].periods;
        for (int i = 0; i < referenceTimePeriods.size(); ++i)
        {
            ASSERT_EQ(referenceTimePeriods[i].startTimeMs, testCameraTimePeriods[i].startTimeMs);
            ASSERT_LE(referenceTimePeriods[i].endTimeMs() - testCameraTimePeriods[i].endTimeMs(), 40);
        }
    }

private:
    const QString m_wearableCameraName = "testCamera";
    QnUuid m_wearableCameraId;
    GeneratedFileData m_generatedData;
    std::unique_ptr<FileUploader> m_uploader;
};

TEST_P(FtWearableCameraUpload, UploadSingleFile)
{
    givenSomeFilesOnDisk();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenWearableCameraIsCreated();
    whenFilesAreUploaded();

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
