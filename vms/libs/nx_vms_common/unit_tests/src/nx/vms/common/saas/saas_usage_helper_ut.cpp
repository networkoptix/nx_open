// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::common::saas::test {

struct TestPluginData
{
    QString pluginId;
    QnUuid engineId;
};

static const std::array<TestPluginData, 2> testPluginData = {
    TestPluginData{"testAnalyticsPlugin", QnUuid("bd0c8d99-5092-4db0-8767-1ae571250be9")},
    TestPluginData{"testAnalyticsPlugin2", QnUuid("bd0c8d99-5092-4db0-8767-1ae571250be8")},
};

static const QnUuid kAnalyticsServiceId("94ca45f8-4859-457a-bc17-f2f9394524fe");

static const QnUuid kServiceUnlimitedMegapixels("60a18a70-452b-46a1-9bfd-e66af6fbd0dc");
static const QnUuid kServiceTenMegapixels("60a18a70-452b-46a1-9bfd-e66af6fbd0dd");
static const QnUuid kServiceFiveMegapixels("60a18a70-452b-46a1-9bfd-e66af6fbd0de");


static const std::string kServiceDataJson = R"json(
[
  {
    "id": "46fd4533-16dd-4e07-b285-ef059b4ecb31",
    "type": "local_recording",
    "state": "active",
    "displayName": "recording service",
    "description": "",
    "createdByChannelPartner": "5f124fdd-9fc7-43c1-898a-0dabde021894",
    "parameters": {
      "empty": "empty"
    }
  },
  {
    "id": "60a18a70-452b-46a1-9bfd-e66af6fbd0de",
    "type": "cloud_storage",
    "state": "active",
    "displayName": "storage service",
    "description": "",
    "createdByChannelPartner": "5f124fdd-9fc7-43c1-898a-0dabde021894",
    "parameters": {
      "days": 30,
      "maxResolutionMp": 5,
      "totalChannelNumber": 5
    }
  },
  {
    "id": "60a18a70-452b-46a1-9bfd-e66af6fbd0dd",
    "type": "cloud_storage",
    "state": "active",
    "displayName": "storage service",
    "description": "",
    "createdByChannelPartner": "5f124fdd-9fc7-43c1-898a-0dabde021894",
    "parameters": {
      "days": 30,
      "maxResolutionMp": 10,
      "totalChannelNumber": 5
    }
  },
  {
    "id": "60a18a70-452b-46a1-9bfd-e66af6fbd0dc",
    "type": "cloud_storage",
    "state": "active",
    "displayName": "storage service",
    "description": "",
    "createdByChannelPartner": "5f124fdd-9fc7-43c1-898a-0dabde021894",
    "parameters": {
      "days": 30,
      "totalChannelNumber": 5
    }
  },
  {
    "id": "94ca45f8-4859-457a-bc17-f2f9394524fe",
    "type": "analytics",
    "state": "active",
    "displayName": "analytics service",
    "description": "",
    "createdByChannelPartner": "5f124fdd-9fc7-43c1-898a-0dabde021894",
    "parameters": {
      "integrationId": "testAnalyticsPlugin",
      "totalChannelNumber": 5
    }
  }
]
)json";

static const std::string kSaasDataJson = R"json(
{
  "cloudSystemId": "2df6b2f1-df31-451d-aac9-6bcb663e210b",
  "state" : "active",
  "services" : {
    "60a18a70-452b-46a1-9bfd-e66af6fbd0de": {
      "quantity": 10
    },
    "60a18a70-452b-46a1-9bfd-e66af6fbd0dd": {
      "quantity": 20
    },
    "60a18a70-452b-46a1-9bfd-e66af6fbd0dc": {
      "quantity": 30
    },
    "46fd4533-16dd-4e07-b285-ef059b4ecb31": {
      "quantity": 10
    },
    "94ca45f8-4859-457a-bc17-f2f9394524fe": {
      "quantity": 2
    }
  },
  "security" : {
    "lastCheck": "2023-06-05 18:10:52",
    "tmpExpirationDate" : "2023-06-05 20:40:52",
    "issue" : "No issues"
  },
  "signature" : ""
}
)json";

class SaasServiceUsageHelperTest: public nx::vms::common::test::ContextBasedTest
{
protected:

    virtual void SetUp()
    {
        using namespace nx::vms::common;

        m_server = addServer();
        m_cloudeStorageHelper.reset(new CloudStorageServiceUsageHelper(systemContext()));
        m_integrationsHelper.reset(new IntegrationServiceUsageHelper(systemContext()));

        auto manager = systemContext()->saasServiceManager();
        manager->loadServiceData(kServiceDataJson);
        manager->loadSaasData(kSaasDataJson);

        for (const auto& data: testPluginData)
        {
            auto pluginResource = AnalyticsPluginResourcePtr(new AnalyticsPluginResource());
            pluginResource->setIdUnsafe(AnalyticsPluginResource::uuidFromManifest(data.pluginId));
            systemContext()->resourcePool()->addResource(pluginResource);

            auto engineResource = AnalyticsEngineResourcePtr(new AnalyticsEngineResource());
            engineResource->setIdUnsafe(data.engineId);
            engineResource->setParentId(pluginResource->getId());
            systemContext()->resourcePool()->addResource(engineResource);

            vms::api::analytics::PluginManifest manifest;
            manifest.id = data.pluginId;
            manifest.isLicenseRequired = true;
            pluginResource->setManifest(manifest);
        }
    }

    virtual void TearDown()
    {
        m_cloudeStorageHelper.reset();
        m_integrationsHelper.reset();
    }

    QnVirtualCameraResourceList addCameras(int size, int megapixels = 0, bool useBackup = false)
    {
        QnVirtualCameraResourceList result;
        for (int i = 0; i < size; ++i)
            result.push_back(addCamera(megapixels, useBackup));
        return result;
    }

    QnVirtualCameraResourcePtr addCamera(int megapixels, bool useBackup)
    {
        using namespace nx::vms::api;
        auto camera = ContextBasedTest::addCamera();
        if (useBackup)
        {
            camera->setBackupQuality(CameraBackupQuality::CameraBackupBoth);
            camera->setBackupPolicy(BackupPolicy::on);
        }
        auto capabilities = camera->cameraMediaCapability();
        capabilities.maxResolution = QSize(1000 * megapixels, 1000);
        camera->setCameraMediaCapability(capabilities);
        return camera;
    }

    QnMediaServerResourcePtr m_server;
    QScopedPointer<CloudStorageServiceUsageHelper> m_cloudeStorageHelper;
    QScopedPointer<IntegrationServiceUsageHelper> m_integrationsHelper;
};

TEST_F(SaasServiceUsageHelperTest, CloudRecordingServicesLoaded)
{
    using namespace nx::vms::api;

    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());

    auto info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(3, info.size());

    // 5 Megapixels licenses.
    ASSERT_EQ(50, info[5].total);

    //< 10 Megapixels licenses.
    ASSERT_EQ(100, info[10].total);

    // Unlimited Megapixels licenses.
    ASSERT_EQ(150, info[SaasCloudStorageParameters::kUnlimitedResolution].total);
}

TEST_F(SaasServiceUsageHelperTest, CloudRecordingServiceUsage)
{
    using namespace nx::vms::api;

    auto cameras1 = addCameras(/*size*/ 50, /* megapixels*/ 5, /*useBackup*/ true);

    auto info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.

    m_cloudeStorageHelper->invalidateCache();
    auto cameras2 = addCameras(/*size*/ 200, /* megapixels*/ 6, /*useBackup*/ true);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(100, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(100, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    m_cloudeStorageHelper->invalidateCache();
    addCameras(/*size*/ 200, /* megapixels*/ 6, /*useBackup*/ false);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(100, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(100, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    m_cloudeStorageHelper->invalidateCache();
    auto cameras3 = addCameras(/*size*/ 50, /* megapixels*/ 100, /*useBackup*/ true);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());

    m_cloudeStorageHelper->invalidateCache();
    auto cameras4 = addCameras(/*size*/ 1, /* megapixels*/ 100, /*useBackup*/ true);
    ASSERT_TRUE(m_cloudeStorageHelper->isOverflow());

    std::set<QnUuid> devicesToAdd;
    std::set<QnUuid> devicesToRemove;
    devicesToRemove.insert(cameras4.front()->getId());
    m_cloudeStorageHelper->proposeChange(devicesToAdd, devicesToRemove);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());

    QSet<QnUuid> devices;
    for (const auto& camera: cameras1)
        devices.insert(camera->getId());
    m_cloudeStorageHelper->setUsedDevices(devices);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(0, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    auto serviceIdInfo = m_cloudeStorageHelper->allInfoByService();
    ASSERT_EQ(3, serviceIdInfo.size());

    // Unlimited Megapixels licenses.
    ASSERT_EQ(0, serviceIdInfo[kServiceUnlimitedMegapixels].inUse);

    // 10 Megapixels licenses.
    ASSERT_EQ(0, serviceIdInfo[kServiceTenMegapixels].inUse);

    // 5 Megapixels licenses.
    ASSERT_EQ(cameras1.size(), serviceIdInfo[kServiceFiveMegapixels].inUse);

    devices.clear();
    for (const auto& camera: cameras2)
        devices.insert(camera->getId());
    m_cloudeStorageHelper->setUsedDevices(devices);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());

    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(0, info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(100, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(100, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    serviceIdInfo = m_cloudeStorageHelper->allInfoByService();
    ASSERT_EQ(3, serviceIdInfo.size());

    // Unlimited Megapixels licenses.
    ASSERT_EQ(100, serviceIdInfo[kServiceUnlimitedMegapixels].inUse);

    // 10 Megapixels licenses.
    ASSERT_EQ(100, serviceIdInfo[kServiceTenMegapixels].inUse);

    // 5 Megapixels licenses.
    ASSERT_EQ(0, serviceIdInfo[kServiceFiveMegapixels].inUse);

}

TEST_F(SaasServiceUsageHelperTest, CloudRecordingSaasMapping)
{
    using namespace nx::vms::api;

    // Available services:
    // 1. "60a18a70-452b-46a1-9bfd-e66af6fbd0de": 50 channels, 5 megapixels
    // 2. "60a18a70-452b-46a1-9bfd-e66af6fbd0dd": 100 channels, 10 megapixels
    // 3. "60a18a70-452b-46a1-9bfd-e66af6fbd0dc": 150 channels, unlimited megapixels

    auto cameras2 = addCameras(/*size*/ 150, /* megapixels*/ 6, /*useBackup*/ true);
    auto mapping = m_cloudeStorageHelper->servicesByCameras();
    std::map<QnUuid, int> serviceUsages;
    for (const auto& [cameraId, serviceId]: mapping)
        ++serviceUsages[serviceId];

    ASSERT_EQ(0, serviceUsages[QnUuid("60a18a70-452b-46a1-9bfd-e66af6fbd0de")]);
    ASSERT_EQ(100, serviceUsages[QnUuid("60a18a70-452b-46a1-9bfd-e66af6fbd0dd")]);
    ASSERT_EQ(50, serviceUsages[QnUuid("60a18a70-452b-46a1-9bfd-e66af6fbd0dc")]);

    auto cameras1 = addCameras(/*size*/ 200, /* megapixels*/ 5, /*useBackup*/ true);
    m_cloudeStorageHelper->invalidateCache();
    mapping = m_cloudeStorageHelper->servicesByCameras();
    serviceUsages.clear();
    for (const auto& [cameraId, serviceId]: mapping)
        ++serviceUsages[serviceId];

    ASSERT_EQ(100, serviceUsages[QnUuid("60a18a70-452b-46a1-9bfd-e66af6fbd0de")]);
    ASSERT_EQ(100, serviceUsages[QnUuid("60a18a70-452b-46a1-9bfd-e66af6fbd0dd")]);
    ASSERT_EQ(150, serviceUsages[QnUuid("60a18a70-452b-46a1-9bfd-e66af6fbd0dc")]);
}

TEST_F(SaasServiceUsageHelperTest, CloudRecordingSaasProposeChanges)
{
    using namespace nx::vms::api;

    // Available services:
    // 1. "60a18a70-452b-46a1-9bfd-e66af6fbd0de": 50 channels, 5 megapixels
    // 2. "60a18a70-452b-46a1-9bfd-e66af6fbd0dd": 100 channels, 10 megapixels
    // 3. "60a18a70-452b-46a1-9bfd-e66af6fbd0dc": 150 channels, unlimited megapixels

    auto enabledBackupCameras = addCameras(/*size*/ 50, /* megapixels*/ 5, /*useBackup*/ true);
    auto disabledBackupCameras = addCameras(/*size*/ 50, /* megapixels*/ 5, /*useBackup*/ false);

    const auto getCamerasIds =
        [](QnVirtualCameraResourceList::iterator begin, QnVirtualCameraResourceList::iterator end)
        {
            std::set<QnUuid> idSet;
            std::transform(begin, end, std::inserter(idSet, idSet.end()),
                [](const QnVirtualCameraResourcePtr& camera){ return camera->getId(); });
            return idSet;
        };

    auto someEnabledBackupCamerasIds =
        getCamerasIds(enabledBackupCameras.begin(), std::next(enabledBackupCameras.begin(), 25));

    auto allEnabledBackupCamerasIds =
        getCamerasIds(enabledBackupCameras.begin(), enabledBackupCameras.end());

    auto someDisabledBackupCamerasIds =
        getCamerasIds(disabledBackupCameras.begin(), std::next(disabledBackupCameras.begin(), 25));

    auto allDisabledBackupCamerasIds =
        getCamerasIds(disabledBackupCameras.begin(), disabledBackupCameras.end());

    std::set<QnUuid> allCamerasIds;
    allCamerasIds.merge(allEnabledBackupCamerasIds);
    allCamerasIds.merge(allDisabledBackupCamerasIds);

    // Propose enabling already enabled does nothing.
    m_cloudeStorageHelper->proposeChange(someEnabledBackupCamerasIds, {});
    auto info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Propose disable already disabled does nothing.
    m_cloudeStorageHelper->proposeChange({}, someDisabledBackupCamerasIds);
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Propose enabling cameras with disabled backup changes usage.
    m_cloudeStorageHelper->proposeChange(someDisabledBackupCamerasIds, {});
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(25, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Propose disabling cameras with enabled backup (not disabled earlier) changes usage.
    m_cloudeStorageHelper->proposeChange({}, someEnabledBackupCamerasIds);
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(25, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Propose disabling cameras with backup proposed to enable earlier shows that state of
    // previous propose is not stored.
    m_cloudeStorageHelper->proposeChange({}, someDisabledBackupCamerasIds);
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Propose enabling some disabled cameras and disabling the same amount of enabled cameras
    // doesn't change usage.
    m_cloudeStorageHelper->proposeChange(someDisabledBackupCamerasIds, someEnabledBackupCamerasIds);
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Propose enabling all known cameras and disabling all known cameras at the same time is no-op.
    m_cloudeStorageHelper->proposeChange(allCamerasIds, allCamerasIds);
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    // Invalidating cache returns usage information to the initial state.
    m_cloudeStorageHelper->invalidateCache();
    info = m_cloudeStorageHelper->allInfoByMegapixels();
    ASSERT_EQ(50, info[5].inUse);
    ASSERT_EQ(0, info[10].inUse);
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);
}

TEST_F(SaasServiceUsageHelperTest, CloudRecordingSaasAccumulateLicenseUsage)
{
    using namespace nx::vms::api;

    // Available services:
    // 1. "60a18a70-452b-46a1-9bfd-e66af6fbd0de": 50 channels, 5 megapixels
    // 2. "60a18a70-452b-46a1-9bfd-e66af6fbd0dd": 100 channels, 10 megapixels
    // 3. "60a18a70-452b-46a1-9bfd-e66af6fbd0dc": 150 channels, unlimited megapixels

    auto enabledBackupCameras5MP = addCameras(/*size*/ 15, /* megapixels*/ 5, /*useBackup*/ true);
    auto enabledBackupCameras15MP = addCameras(/*size*/ 25, /* megapixels*/ 15, /*useBackup*/ true);
    auto disabledBackupCameras15MP = addCameras(/*size*/ 50, /* megapixels*/ 15, /*useBackup*/ false);

    auto summary = m_cloudeStorageHelper->allInfoForResolution(5);
    ASSERT_EQ(300, summary.available);
    ASSERT_EQ(40, summary.inUse);
    ASSERT_EQ(300, summary.total);

    summary = m_cloudeStorageHelper->allInfoForResolution(10);
    ASSERT_EQ(250, summary.available);
    ASSERT_EQ(25, summary.inUse);
    ASSERT_EQ(250, summary.total);

    summary = m_cloudeStorageHelper->allInfoForResolution(15);
    ASSERT_EQ(150, summary.available);
    ASSERT_EQ(25, summary.inUse);
    ASSERT_EQ(150, summary.total);

    summary = m_cloudeStorageHelper->allInfoForResolution(100);
    ASSERT_EQ(150, summary.available);
    ASSERT_EQ(25, summary.inUse);
    ASSERT_EQ(150, summary.total);
}

TEST_F(SaasServiceUsageHelperTest, LocalRecordingServiceLoaded)
{
    using namespace nx::vms::api;

    auto licenses = systemContext()->licensePool()->getLicenses();
    ASSERT_EQ(1, licenses.size());
    ASSERT_EQ(10, licenses[0]->cameraCount());
    ASSERT_EQ("saas", licenses[0]->xclass());
    const auto dateStr = licenses[0]->tmpExpirationDate().toString(Qt::ISODate);
    ASSERT_EQ("2023-06-05T20:40:52Z", dateStr);
}

TEST_F(SaasServiceUsageHelperTest, IntegrationServiceLoaded)
{
    using namespace nx::vms::api;

    auto info = m_integrationsHelper->allInfo();
    ASSERT_EQ(1, info.size());
    ASSERT_EQ(10, info.begin()->second.available);
}

TEST_F(SaasServiceUsageHelperTest, IntegrationServiceUsage)
{
    auto cameras = addCameras(/*size*/ 50);
    QSet<QnUuid> engines;
    engines.insert(testPluginData[0].engineId);

    cameras[0]->setUserEnabledAnalyticsEngines(engines);
    m_integrationsHelper->invalidateCache();
    auto info = m_integrationsHelper->allInfo();
    ASSERT_FALSE(m_integrationsHelper->isOverflow());
    ASSERT_EQ(1, info.begin()->second.inUse);

    for (const auto& camera: cameras)
        camera->setUserEnabledAnalyticsEngines(engines);
    m_integrationsHelper->invalidateCache();
    info = m_integrationsHelper->allInfo();
    ASSERT_TRUE(m_integrationsHelper->isOverflow());
    ASSERT_EQ(50, info.begin()->second.inUse);

    std::vector<IntegrationServiceUsageHelper::Propose> propose;
    propose.push_back(IntegrationServiceUsageHelper::Propose{cameras[0]->getId(), QSet<QnUuid>()});

    m_integrationsHelper->proposeChange(propose);
    info = m_integrationsHelper->allInfo();
    ASSERT_EQ(49, info.begin()->second.inUse);

    propose.push_back(IntegrationServiceUsageHelper::Propose{cameras[1]->getId(), QSet<QnUuid>()});
    m_integrationsHelper->proposeChange(propose);
    info = m_integrationsHelper->allInfo();
    ASSERT_EQ(48, info.begin()->second.inUse);
}

TEST_F(SaasServiceUsageHelperTest, camerasByService)
{
    auto cameras = addCameras(/*size*/ 50);
    QSet<QnUuid> engines;
    for (const auto& data: testPluginData)
        engines.insert(data.engineId);

    for (const auto& camera: cameras)
        camera->setUserEnabledAnalyticsEngines(engines);
    m_integrationsHelper->invalidateCache();
    auto info = m_integrationsHelper->camerasByService();
    ASSERT_EQ(testPluginData.size(), info.size());
    ASSERT_EQ(QnUuid(), info.begin()->first);
    ASSERT_EQ(kAnalyticsServiceId, info.rbegin()->first);
    ASSERT_EQ(50, info[kAnalyticsServiceId].size());
    ASSERT_EQ(50, info[QnUuid()].size());
}

} // nx::vms::common::saas::test
