#include <camera/video_camera_mock.h>
#include <core/misc/schedule_task.h>
#include <core/resource_management/resource_pool.h>
#include <nx/mediaserver/camera_mock.h>
#include <nx/mediaserver/live_stream_provider_mock.h>
#include <nx/vms/api/metrics.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/metrics/helpers.h>
#include <recorder/storage_manager.h>
#include <server_for_tests.h>
#include <utils/common/synctime.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::server::test {

using namespace nx::vms::api::metrics;
using namespace std::chrono;

static const int kMinDays = 5;
const int kChannelCount = 4;

namespace {

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
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    void onData(const QnAbstractMediaDataPtr& data)
    {
        m_stat[data->channelNumber].onData(data);
    }
};

} // namespace

class MetricsCamerasApi: public ::testing::Test
{
public:
    static QnSharedResourcePointer<resource::test::CameraMock> m_camera;
    static std::unique_ptr<ServerForTests> launcher;

    static void SetUpTestCase()
    {
        nx::vms::server::metrics::setTimerMultiplier(100);

        launcher = std::make_unique<ServerForTests>();

        auto storage = launcher->addStorage(lm("Storage 1"));
        auto storageIndex = launcher->serverModule()->storageDbPool()->getStorageIndex(storage);

        auto resourcePool = launcher->serverModule()->resourcePool();
        // Some tests here add camera to VideoCameraPool manually.
        // Recording manager do the same but async. Block it.
        resourcePool->disconnect(launcher->serverModule()->recordingManager());

        m_camera = launcher->addCamera(2);
        m_camera->blockInitialization();
        m_camera->setMinDays(kMinDays);
        m_camera->setMaxFps(30);
        
        auto customVideoLayout = QnCustomResourceVideoLayoutPtr(
            new QnCustomResourceVideoLayout(QSize(kChannelCount, 1)));
        for (int i = 0; i < kChannelCount; ++i)
            customVideoLayout->setChannel(i, 0, i); //< Arrange multi video layout from left to right.
        m_camera->setProperty(ResourcePropertyKey::kVideoLayout, customVideoLayout->toString());


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

        auto catalog = launcher->serverModule()->normalStorageManager()->getFileCatalog(
            m_camera->getUniqueId(), QnServer::HiQualityCatalog);

        const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();

        static const qint64 kMsInMinute = 1000 * 60;
        static const qint64 kMsInHour = kMsInMinute * 60;
        static const qint64 kMsInDay = kMsInHour * 24;
        nx::vms::server::Chunk chunk;
        chunk.startTimeMs = currentTimeMs - kMsInDay * 2;
        chunk.durationMs = 45 * 1000;
        chunk.setFileSize(1000 * 1000);
        chunk.storageIndex = storageIndex;
        catalog->addRecord(chunk);

        // Fill last 24 hours with 50% archive density.
        auto timeMs = currentTimeMs - kMsInDay + kMsInMinute;
        while (timeMs < currentTimeMs)
        {
            chunk.startTimeMs = timeMs;
            chunk.durationMs = kMsInMinute;
            chunk.storageIndex = storageIndex;
            chunk.setFileSize(1000 * 1000 * 60); //< Bitrate 8 Mbit.
            catalog->addRecord(chunk);
            timeMs += kMsInMinute * 2;
        }
    }

    static void TearDownTestCase()
    {
        m_camera.reset();
        launcher.reset();
        nx::vms::server::metrics::setTimerMultiplier(1);
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
        liveParams.resolution = QSize(1280, 720);
        dataProvider->setPrimaryStreamParams(liveParams);

        const auto cameraId = m_camera->getId().toSimpleString();
        auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
        auto cameraData = systemValues["cameras"][cameraId];
        auto streamData = cameraData[streamPrefix];
        ASSERT_EQ(30, streamData["targetFps"].toInt());

        const int channelCount = m_camera->getVideoLayout()->channelCount();
        const QString expectedResolution = lit("%1x720").arg(1280 * channelCount);
        ASSERT_EQ(expectedResolution, streamData["resolution"].toString());

        auto alarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
        if (!isPrimary)
        {
            EXPECT_EQ(alarms["cameras." + cameraId + ".secondaryStream.resolution.0"].level, AlarmLevel::warning);

            liveParams.resolution = QSize(640 / channelCount, 480);
            dataProvider->setPrimaryStreamParams(liveParams);
            alarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
            EXPECT_EQ(alarms["cameras." + cameraId + ".secondaryStream.resolution.0"].level, AlarmLevel::none);
        }

        EXPECT_EQ(alarms[lm("cameras.%1.%2.fpsDeltaAvarage.0").args(cameraId, streamPrefix)].level, AlarmLevel::none);
        EXPECT_EQ(alarms[lm("cameras.%1.%2.actualBitrateBps.0").args(cameraId, streamPrefix)].level, AlarmLevel::none);

        // Add some 'live' data to get actual bitrate.
        QnWritableCompressedVideoDataPtr video(new QnWritableCompressedVideoData());
        video->width = liveParams.resolution.width();
        video->height = liveParams.resolution.height();
        const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
        
        video->m_data.resize(1000 * 50);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            video->timestamp = currentTimeMs;
            video->channelNumber = channel;
            for (int i = 0; i < 5; ++i)
            {
                video->timestamp += 50000; //< 20 fps, 1Mb/sec for each camera channel.
                dataProvider->onData(video);
            }
        }

        nx::utils::ElapsedTimer timer;
        timer.restart();
        do {
            systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
            cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
            streamData = cameraData[streamPrefix];
        } while (streamData["fpsDelta"].isNull() && !timer.hasExpired(10s));

        ASSERT_EQ(1000000 * channelCount, streamData["actualBitrateBps"].toInt());
        ASSERT_FLOAT_EQ(10, streamData["fpsDelta"].toDouble());

        dataProvider->stop();
        m_camera->setLicenseUsed(false);
    }
};

QnSharedResourcePointer<resource::test::CameraMock> MetricsCamerasApi::m_camera;
std::unique_ptr<ServerForTests> MetricsCamerasApi::launcher;

TEST_F(MetricsCamerasApi, infoGroup)
{
    const auto cameraId = m_camera->getId().toSimpleString();
    auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][cameraId];
    EXPECT_GE(cameraData.size(), 4);
    EXPECT_EQ(cameraData["_"]["name"], "CMock 2");

    auto infoData = cameraData["info"];
    EXPECT_EQ(infoData["server"], launcher->id);
    EXPECT_EQ(infoData["type"], "Camera");
    EXPECT_EQ(infoData["ip"], "192.168.0.2");
    EXPECT_EQ(infoData["model"], "Model_2");
    EXPECT_EQ(infoData["vendor"], "Vendor_2");
    EXPECT_EQ(infoData["firmware"], "1.0.2");
    EXPECT_EQ(infoData["recording"], "Off");

    m_camera->setLicenseUsed(true);
    systemValues = launcher->get<SystemValues>("/ec2/metrics/values");

    cameraData = systemValues["cameras"][cameraId];
    infoData = cameraData["info"];
    EXPECT_EQ(infoData["recording"], "Scheduled");
    m_camera->setLicenseUsed(false);
}

TEST_F(MetricsCamerasApi, availabilityGroup)
{
    const auto cameraId = m_camera->getId().toSimpleString();
    auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    EXPECT_GE(cameraData.size(), 4);

    int offlineEvents = cameraData["availability"]["offlineEvents"].toInt();
    EXPECT_EQ(0, offlineEvents);

    const int kOfflineIterations = 4;
    for (int i = 0; i < kOfflineIterations; ++i)
    {
        m_camera->setStatus(Qn::Online);
        m_camera->setStatus(Qn::Offline);
    }

    systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    cameraData = systemValues["cameras"][cameraId];
    offlineEvents = cameraData["availability"]["offlineEvents"].toInt();
    EXPECT_EQ(kOfflineIterations, offlineEvents);

    auto systemAlarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
    EXPECT_EQ(systemAlarms["cameras." + cameraId + ".availability.offlineEvents.0"].level, AlarmLevel::warning);
    EXPECT_EQ(systemAlarms["cameras." + cameraId + ".availability.status.0"].level, AlarmLevel::error);

    m_camera->setStatus(Qn::Online);
    systemAlarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
    EXPECT_EQ(systemAlarms["cameras." + cameraId + ".availability.status.0"].level, AlarmLevel::none);

    auto eventConnector = launcher->serverModule()->eventConnector();
    QStringList macAddrList;
    macAddrList << m_camera->getMAC().toString();
    eventConnector->at_cameraIPConflict(
        m_camera, QHostAddress(m_camera->getHostAddress()), macAddrList, /*time*/ 0);

    nx::utils::ElapsedTimer timer;
    timer.restart();
    do
    {
        systemAlarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
    } while (systemAlarms.count("cameras." + cameraId + ".availability.ipConflicts3min.0") == 0
        && !timer.hasExpired(15s));
    EXPECT_EQ(systemAlarms["cameras." + cameraId + ".availability.ipConflicts3min.0"].level, AlarmLevel::error);
}

TEST_F(MetricsCamerasApi, analyticsGroup)
{
    const auto cameraId = m_camera->getId().toSimpleString();
    m_camera->setLicenseUsed(true);

    auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][cameraId];
    auto analyticsData = cameraData["storage"];
    EXPECT_EQ(analyticsData["recordingBitrateBps"], 1000000);
    EXPECT_EQ(analyticsData["archiveLengthS"], 24 * 3600 * 2);
    EXPECT_EQ(analyticsData["minArchiveLengthS"], 24 * 3600 * kMinDays);

    auto systemAlarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
    EXPECT_EQ(systemAlarms["cameras." + cameraId + ".storage.archiveLengthS.0"].level, AlarmLevel::none);
    ASSERT_FALSE(analyticsData["hasArchiveCleanup"].toBool());

    auto catalog = launcher->serverModule()->normalStorageManager()->getFileCatalog(
        m_camera->getUniqueId(), QnServer::LowQualityCatalog);
    catalog->deleteFirstRecord(); //< This catalog is empty.

    systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    analyticsData = cameraData["storage"];
    ASSERT_FALSE(analyticsData["hasArchiveCleanup"].toBool());

    catalog = launcher->serverModule()->normalStorageManager()->getFileCatalog(
        m_camera->getUniqueId(), QnServer::HiQualityCatalog);
    catalog->deleteFirstRecord(); //< This catalog is non empty.

    systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    analyticsData = cameraData["storage"];
    ASSERT_TRUE(analyticsData["hasArchiveRotated"].toBool());

    systemAlarms = launcher->getFlat<SystemAlarms>("/ec2/metrics/alarms");
    EXPECT_EQ(systemAlarms["cameras." + cameraId + ".storage.archiveLengthS.0"].level, AlarmLevel::error);

    m_camera->setLicenseUsed(false);
}

TEST_F(MetricsCamerasApi, primaryStreamGroup)
{
    checkStreamParams(/*isPrimary*/ true);
}

TEST_F(MetricsCamerasApi, secondaryStreamGroup)
{
    checkStreamParams(/*isPrimary*/ false);
}

} // namespace nx::vms::server::test
