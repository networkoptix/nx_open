#include <gtest/gtest.h>

#include <nx/vms/api/metrics.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"
#include <nx/mediaserver/camera_mock.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/metrics/helpers.h>
#include <utils/common/synctime.h>
#include <recorder/storage_manager.h>
#include <core/misc/schedule_task.h>
#include <nx/mediaserver/live_stream_provider_mock.h>
#include <camera/video_camera_mock.h>

namespace nx::test {

using namespace nx::vms::api::metrics;
using namespace nx::vms::server;
using namespace std::chrono;

static const QString kCameraName("Camera1");
static const QString kCameraHostAddress("192.168.0.2");
static const QString kCameraFirmware("1.2.3.4");
static const QString kCameraModel("model1");
static const QString kCameraVendor("vendor1");
static const int kMinDays = 5;

class DataProviderStub : public resource::test::LiveStreamProviderMock
{
public:
    using resource::test::LiveStreamProviderMock::LiveStreamProviderMock;
    ~DataProviderStub()
    {
        stop();
    }

    virtual void run() override
    {
        while (!m_needStop)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void onData(const QnAbstractMediaDataPtr& data)
    {
        m_stat[0].onData(data);
    }
};

class MetricsCameraApi: public ::testing::Test
{
public:
    static QnSharedResourcePointer<resource::test::CameraMock> m_camera;
    static std::unique_ptr<MediaServerLauncher> launcher;

    static void SetUpTestCase()
    {
        nx::vms::server::metrics::setTimerMultiplier(100);

        launcher = std::make_unique<MediaServerLauncher>();

        EXPECT_TRUE(launcher->start());
        auto resourcePool = launcher->serverModule()->resourcePool();
        // Some tests here add camera to VideoCameraPool manually.
        // Recording manager do the same but async. Block it.
        resourcePool->disconnect(launcher->serverModule()->recordingManager());

        m_camera.reset(new resource::test::CameraMock(launcher->serverModule()));
        m_camera->blockInitialization();
        m_camera->setPhysicalId("testCamera 1");
        m_camera->setName(kCameraName);
        m_camera->setModel(kCameraModel);
        m_camera->setVendor(kCameraVendor);
        m_camera->setParentId(launcher->commonModule()->moduleGUID());
        m_camera->setHostAddress(kCameraHostAddress);
        m_camera->setFirmware(kCameraFirmware);
        m_camera->setMAC(nx::utils::MacAddress(QLatin1String("12:12:12:12:12:12")));
        m_camera->setMinDays(kMinDays);
        m_camera->setMaxFps(30);

        QnScheduleTaskList schedule;
        for (int i = 0; i < 7; ++i)
        {
            QnScheduleTask t;
            t.startTime = 0;
            t.endTime = 24 * 3600;
            t.dayOfWeek = i;
            t.fps = 30;
            t.recordingType = Qn::RecordingType::always;
            schedule << t;
        }
        m_camera->setScheduleTasks(schedule);

        launcher->commonModule()->resourcePool()->addResource(m_camera);

        auto catalog = launcher->serverModule()->normalStorageManager()->getFileCatalog(
            m_camera->getUniqueId(), QnServer::HiQualityCatalog);

        const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();

        static const qint64 kMsInMinute = 1000 * 60;
        static const qint64 kMsInHour = kMsInMinute * 60;
        static const qint64 kMsInDay = kMsInHour * 24;
        nx::vms::server::Chunk chunk1;
        chunk1.startTimeMs = currentTimeMs - kMsInDay * 2;
        chunk1.durationMs = 45 * 1000;
        chunk1.setFileSize(1000 * 1000);
        catalog->addRecord(chunk1);

        // Fill last 24 hours with 50% archive density.
        auto timeMs = currentTimeMs - kMsInDay + kMsInMinute;
        while (timeMs < currentTimeMs)
        {
            nx::vms::server::Chunk chunk;
            chunk.startTimeMs = timeMs;
            chunk.durationMs = kMsInMinute;
            chunk.setFileSize(1000 * 1000 * 60); //< Bitrate 8 Mbit.
            catalog->addRecord(chunk);
            timeMs += kMsInMinute * 2;
        }
    }

    static bool hasAlarm(const Alarms& alarms, const QString& value)
    {
        return std::any_of(alarms.begin(), alarms.end(),
            [value](const auto& alarm) { return alarm.parameter == value; });
    }

    static void TearDownTestCase()
    {
        m_camera.reset();
        launcher.reset();
        nx::vms::server::metrics::setTimerMultiplier(1);
    }

    MetricsCameraApi()
    {
    }

    template<typename T>
    T get(const QString& api)
    {
        QnJsonRestResult result;
        [&]() { NX_TEST_API_GET(launcher.get(), api, &result); }();
        EXPECT_EQ(result.error, QnJsonRestResult::NoError);
        return result.deserialized<T>();
    }

    void checkStreamParams(bool isPrimary)
    {
        auto streamPrefix = isPrimary ? "primaryStream" : "secondaryStream";
        m_camera->setLicenseUsed(true);

        auto dataProvider = QSharedPointer<DataProviderStub>(new DataProviderStub(m_camera));
        auto videoCameraMock = new MediaServerVideoCameraMock();
        if (isPrimary)
            videoCameraMock->setPrimaryReader(dataProvider);
        else
            videoCameraMock->setSecondaryReader(dataProvider);
        ASSERT_TRUE(launcher->serverModule()->videoCameraPool()->addVideoCamera(
            m_camera, QnVideoCameraPtr(videoCameraMock)));

        dataProvider->start();
        QnLiveStreamParams liveParams;
        liveParams.fps = 30;
        liveParams.resolution = QSize(1920, 1080);
        dataProvider->setPrimaryStreamParams(liveParams);

        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
        auto streamData = cameraData[streamPrefix];
        ASSERT_EQ(30, streamData["targetFps"].toInt());
        ASSERT_EQ("1920x1080", streamData["resolution"].toString());

        auto alarms = get<Alarms>("/ec2/metrics/alarms");
        if (!isPrimary)
        {
            ASSERT_TRUE(hasAlarm(alarms, "secondaryStream.resolution"));

            liveParams.resolution = QSize(1280, 720);
            dataProvider->setPrimaryStreamParams(liveParams);
            alarms = get<Alarms>("/ec2/metrics/alarms");
            ASSERT_FALSE(hasAlarm(alarms, "secondaryStream.resolution"));
        }

        ASSERT_FALSE(hasAlarm(alarms, lm("%1.fpsDeltaAvarage").args(streamPrefix)));
        ASSERT_FALSE(hasAlarm(alarms, lm("%1.actualBitrateBps").args(streamPrefix)));

        // Add some 'live' data to get actual bitrate.
        QnWritableCompressedVideoDataPtr video(new QnWritableCompressedVideoData());
        video->width = liveParams.resolution.width();
        video->height = liveParams.resolution.height();
        const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
        video->timestamp = currentTimeMs;
        video->m_data.resize(1000 * 50);

        for (int i = 0; i < 5; ++i)
        {
            video->timestamp += 50000; //< 20 fps, 1Mb/sec
            dataProvider->onData(video);
        }

        nx::utils::ElapsedTimer timer;
        timer.restart();
        do {
            systemValues = get<SystemValues>("/ec2/metrics/values");
            cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
            streamData = cameraData[streamPrefix];
        } while (streamData["fpsDelta"].isNull() && !timer.hasExpired(10s));

        ASSERT_EQ(1000000, streamData["actualBitrateBps"].toInt());
        ASSERT_FLOAT_EQ(10, streamData["fpsDelta"].toDouble());

        dataProvider->stop();
        m_camera->setLicenseUsed(false);
    }
};

QnSharedResourcePointer<resource::test::CameraMock> MetricsCameraApi::m_camera;
std::unique_ptr<MediaServerLauncher> MetricsCameraApi::launcher;

TEST_F(MetricsCameraApi, infoGroup)
{
    auto systemValues = get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    ASSERT_EQ(5, cameraData.size());
    auto infoData = cameraData["info"];
    const auto commonModule = launcher->commonModule();

    ASSERT_EQ(kCameraName, infoData["name"].toString());
    ASSERT_EQ(commonModule->moduleGUID().toSimpleString(), infoData["server"].toString());
    ASSERT_EQ("Camera", infoData["type"].toString());
    ASSERT_EQ(kCameraHostAddress, infoData["ip"].toString());
    ASSERT_EQ(kCameraModel, infoData["model"].toString());
    ASSERT_EQ(kCameraVendor, infoData["vendor"].toString());
    ASSERT_EQ(kCameraFirmware, infoData["firmware"].toString());
    ASSERT_EQ("Off", infoData["recording"].toString());

    m_camera->setLicenseUsed(true);
    systemValues = get<SystemValues>("/ec2/metrics/values");
    cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    infoData = cameraData["info"];
    ASSERT_EQ("Scheduled", infoData["recording"].toString());
    m_camera->setLicenseUsed(false);
}

TEST_F(MetricsCameraApi, availabilityGroup)
{
    auto systemValues = get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    ASSERT_EQ(5, cameraData.size());

    int offlineEvents = cameraData["availability"]["offlineEvents"].toInt();
    ASSERT_EQ(1, offlineEvents);

    const int kOfflineIterations = 4;
    for (int i = 0; i < kOfflineIterations; ++i)
    {
        m_camera->setStatus(Qn::Online);
        m_camera->setStatus(Qn::Offline);
    }

    systemValues = get<SystemValues>("/ec2/metrics/values");
    cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    offlineEvents = cameraData["availability"]["offlineEvents"].toInt();
    ASSERT_EQ(1 + kOfflineIterations, offlineEvents);

    auto systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_TRUE(hasAlarm(systemAlarms, "availability.offlineEvents"));
    ASSERT_TRUE(hasAlarm(systemAlarms, "availability.status"));

    m_camera->setStatus(Qn::Online);
    systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_FALSE(hasAlarm(systemAlarms, "availability.status"));

    auto eventConnector = launcher->serverModule()->eventConnector();
    QStringList macAddrList;
    macAddrList << m_camera->getMAC().toString();
    eventConnector->at_cameraIPConflict(
        m_camera, QHostAddress(m_camera->getHostAddress()), macAddrList, /*time*/ 0);

    nx::utils::ElapsedTimer timer;
    timer.restart();
    do
    {
        systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    } while (!hasAlarm(systemAlarms, "availability.ipConflicts3min") && !timer.hasExpired(15s));
    ASSERT_TRUE(hasAlarm(systemAlarms, "availability.ipConflicts3min"));
}

TEST_F(MetricsCameraApi, analyticsGroup)
{
    m_camera->setLicenseUsed(true);

    auto systemValues = get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    auto analyticsData = cameraData["analytics"];
    ASSERT_EQ(1000000, analyticsData["recordingBitrateBps"].toDouble());
    ASSERT_EQ(24 * 3600 * 2, analyticsData["archiveLengthS"].toInt());
    ASSERT_EQ(24 * 3600 * kMinDays, analyticsData["minArchiveLengthS"].toInt());

    auto systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_TRUE(hasAlarm(systemAlarms, "analytics.minArchiveLengthS"));

    m_camera->setLicenseUsed(false);
}

TEST_F(MetricsCameraApi, primaryStreamGroup)
{
    checkStreamParams(/*isPrimary*/ true);
}

TEST_F(MetricsCameraApi, secondaryStreamGroup)
{
    checkStreamParams(/*isPrimary*/ false);
}

} // nx::test
