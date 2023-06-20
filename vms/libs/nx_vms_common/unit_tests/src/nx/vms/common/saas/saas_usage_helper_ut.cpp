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

static const QnUuid kEngineId("bd0c8d99-5092-4db0-8767-1ae571250be9");

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
      "maxResolution": 5,
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
      "maxResolution": 10,
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
      "integrationId": "bd0c8d99-5092-4db0-8767-1ae571250be9",
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
        m_server = addServer();
        m_cloudeStorageHelper.reset(new CloudStorageServiceUsageHelper(systemContext()));
        m_integrationsHelper.reset(new IntegrationServiceUsageHelper(systemContext()));

        auto manager = systemContext()->saasServiceManager();
        manager->loadServiceData(kServiceDataJson);
        manager->loadSaasData(kSaasDataJson);

        auto pluginResource = AnalyticsPluginResourcePtr(new AnalyticsPluginResource());
        pluginResource->setIdUnsafe(QnUuid::createUuid());

        auto engineResource = AnalyticsEngineResourcePtr(new AnalyticsEngineResource());
        engineResource->setIdUnsafe(kEngineId);
        engineResource->setParentId(pluginResource->getId());
        systemContext()->resourcePool()->addResource(pluginResource);
        systemContext()->resourcePool()->addResource(engineResource);

        vms::api::analytics::PluginManifest manifest;
        manifest.id = "Test plugin resource";
        manifest.isLicenseRequired = true;
        pluginResource->setManifest(manifest);
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
            camera->setBackupQuality(CameraBackupQuality::CameraBackup_Both);
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

    auto info = m_cloudeStorageHelper->allInfo();
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
    
    auto info = m_cloudeStorageHelper->allInfo();
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.

    m_cloudeStorageHelper->invalidateCache();
    auto cameras2 = addCameras(/*size*/ 200, /* megapixels*/ 6, /*useBackup*/ true);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    info = m_cloudeStorageHelper->allInfo();
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(100, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(100, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    m_cloudeStorageHelper->invalidateCache();
    addCameras(/*size*/ 200, /* megapixels*/ 6, /*useBackup*/ false);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    info = m_cloudeStorageHelper->allInfo();
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(100, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(100, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);

    m_cloudeStorageHelper->invalidateCache();
    auto cameras3 = addCameras(/*size*/ 50, /* megapixels*/ 100, /*useBackup*/ true);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());

    m_cloudeStorageHelper->invalidateCache();
    auto cameras4 = addCameras(/*size*/ 1, /* megapixels*/ 100, /*useBackup*/ true);
    ASSERT_TRUE(m_cloudeStorageHelper->isOverflow());

    QSet<QnUuid> devices;
    for (const auto& camera: cameras1)
        devices.insert(camera->getId());
    m_cloudeStorageHelper->setUsedDevices(devices);
    ASSERT_FALSE(m_cloudeStorageHelper->isOverflow());
    info = m_cloudeStorageHelper->allInfo();
    ASSERT_EQ(cameras1.size(), info[5].inUse); //< 5 Megapixels licenses.
    ASSERT_EQ(0, info[10].inUse); //< 10 Megapixels licenses.
    ASSERT_EQ(0, info[SaasCloudStorageParameters::kUnlimitedResolution].inUse);
}

TEST_F(SaasServiceUsageHelperTest, LocalRecordingServiceLoaded)
{
    using namespace nx::vms::api;

    auto licenses = systemContext()->licensePool()->getLicenses();
    ASSERT_EQ(1, licenses.size());
    ASSERT_EQ(10, licenses[0]->cameraCount());
    ASSERT_EQ("cloud", licenses[0]->xclass());
}

TEST_F(SaasServiceUsageHelperTest, IntegrationServiceLoaded)
{
    using namespace nx::vms::api;

    auto info = m_integrationsHelper->allInfo();
    ASSERT_EQ(1, info.size());
    ASSERT_EQ(10, info.begin()->available);
}

TEST_F(SaasServiceUsageHelperTest, IntegrationServiceUsage)
{
    auto cameras = addCameras(/*size*/ 50);
    QSet<QnUuid> engines;
    engines.insert(kEngineId);

    cameras[0]->setUserEnabledAnalyticsEngines(engines);
    m_integrationsHelper->invalidateCache();
    auto info = m_integrationsHelper->allInfo();
    ASSERT_FALSE(m_integrationsHelper->isOverflow());
    ASSERT_EQ(1, info.begin()->inUse);

    for (const auto& camera: cameras)
        camera->setUserEnabledAnalyticsEngines(engines);
    m_integrationsHelper->invalidateCache();
    info = m_integrationsHelper->allInfo();
    ASSERT_TRUE(m_integrationsHelper->isOverflow());
    ASSERT_EQ(50, info.begin()->inUse);
    
    std::vector<IntegrationServiceUsageHelper::Propose> propose;
    propose.push_back(IntegrationServiceUsageHelper::Propose{cameras[0]->getId(), QSet<QnUuid>()});

    m_integrationsHelper->proposeChange(propose);
    info = m_integrationsHelper->allInfo();
    ASSERT_EQ(49, info.begin()->inUse);

    propose.push_back(IntegrationServiceUsageHelper::Propose{cameras[1]->getId(), QSet<QnUuid>()});
    m_integrationsHelper->proposeChange(propose);
    info = m_integrationsHelper->allInfo();
    ASSERT_EQ(48, info.begin()->inUse);
}

} // nx::vms::common::saas::test
