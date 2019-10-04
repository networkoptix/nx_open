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

    static void TearDownTestCase()
    {
        m_camera.reset();
        launcher.reset();
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
    ASSERT_EQ(2, systemAlarms.size());
    ASSERT_EQ("availability.offlineEvents", systemAlarms[0].parameter);
    ASSERT_EQ("availability.status", systemAlarms[1].parameter);

    m_camera->setStatus(Qn::Online);
    systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_EQ(1, systemAlarms.size());

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
    } while (systemAlarms.size() != 2 && !timer.hasExpired(15s));

    ASSERT_EQ(2, systemAlarms.size());
    ASSERT_EQ("availability.ipConflicts3min", systemAlarms[0].parameter);
}

class DataProviderStub : public QnAbstractStreamDataProvider
{
public:
    using QnAbstractStreamDataProvider::QnAbstractStreamDataProvider;
};

TEST_F(MetricsCameraApi, analyticsGroup)
{
    m_camera->setLicenseUsed(true);

    auto systemValues = get<SystemValues>("/ec2/metrics/values");
    auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
    auto analyticsData = cameraData["analytics"];
    ASSERT_EQ(1000000, analyticsData["recordingBitrateBps"].toInt());
    ASSERT_EQ(24 * 3600 * 2, analyticsData["archiveLengthS"].toInt());
    ASSERT_EQ(24 * 3600 * kMinDays, analyticsData["minArchiveLengthS"].toInt());

    auto systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_EQ("analytics.minArchiveLengthS", systemAlarms[0].parameter);

    m_camera->setLicenseUsed(false);
}

} // nx::test
