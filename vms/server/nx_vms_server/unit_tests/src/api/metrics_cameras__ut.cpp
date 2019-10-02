#include <gtest/gtest.h>

#include <nx/vms/api/metrics.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"
#include <nx/mediaserver/camera_mock.h>
#include <core/resource_management/resource_pool.h>

namespace nx::test {

using namespace nx::vms::api::metrics;
using namespace nx::vms::server;

static const QString kCameraName("Camera1");
static const QString kCameraHostAddress("192.168.0.2");
static const QString kCameraFirmware("1.2.3.4");
static const QString kCameraModel("model1");
static const QString kCameraVendor("vendor1");

class DataProviderStub: public QnAbstractStreamDataProvider
{
public:
    using QnAbstractStreamDataProvider::QnAbstractStreamDataProvider;
};

class MetricsCameraApi: public ::testing::Test
{
public:
    static std::unique_ptr<DataProviderStub> m_dataProviderStub;
    static QnSharedResourcePointer<resource::test::CameraMock> m_camera;
    static std::unique_ptr<MediaServerLauncher> launcher;

    static void SetUpTestCase()
    {
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

        launcher->commonModule()->resourcePool()->addResource(m_camera);
        m_dataProviderStub.reset(new DataProviderStub(m_camera));

        DeviceFileCatalog catalog(
            launcher->serverModule(),
            m_camera->getUniqueId(), QnServer::HiQualityCatalog, QnServer::StoragePool::Normal);

        nx::vms::server::Chunk chunk1;
        chunk1.startTimeMs = 1400000000LL * 1000;
        chunk1.durationMs = 45 * 1000;
        chunk1.setFileSize(100);
        catalog.addRecord(chunk1);

        nx::vms::server::Chunk chunk2;
        chunk2.startTimeMs = 1400000000LL * 1000;
        chunk2.durationMs = 45 * 1000;
        chunk2.setFileSize(200);
        catalog.addRecord(chunk2);
    }

    static void TearDownTestCase()
    {
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

std::unique_ptr<DataProviderStub> MetricsCameraApi::m_dataProviderStub;
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
}

TEST_F(MetricsCameraApi, offlineEvents)
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
}

} // nx::test
