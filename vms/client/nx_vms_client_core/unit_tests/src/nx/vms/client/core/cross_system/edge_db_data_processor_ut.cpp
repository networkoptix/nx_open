// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <map>
#include <memory>
#include <set>

#include <gtest/gtest.h>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/cross_system/edge_db_data_loader.h>
#include <nx/vms/client/core/cross_system/edge_db_data_processor.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/system_context.h>

#include "test_edge_db_connection.h"

namespace nx::vms::client::core {
namespace test {

using namespace nx::vms::api;

namespace {

const nx::Uuid kOrganizationId("6c4b1a5e-0f43-4b8e-9a43-3f7d5e2b9a01");
const QString kSite1Id = "1f6e2b9c-7d4a-4c3e-8b5a-9e0d1c2b3a4f";
const QString kSite2Id = "2a7f3c0d-8e5b-4d4f-9c6b-0f1e2d3c4b5a";
const QString kNoContextSiteId = "4c915e2f-a07d-4f61-9e8d-21304f5e6d7c";
const QString kCustomGroupIdProperty = "customGroupId";

WithSite<DeviceModelV4> device(const QString& siteId, const QString& name)
{
    WithSite<DeviceModelV4> result;
    result.id = nx::Uuid::createUuid();
    result.name = name;
    result.siteId = siteId;
    result.parameters[kCustomGroupIdProperty] = QJsonValue(name + " group");
    result.group = DeviceGroupSettings{.id = name + " recorder", .name = name + " recorder name"};
    return result;
}

WithSite<LayoutData> layout(const QString& siteId, const QString& name)
{
    WithSite<LayoutData> result;
    result.id = nx::Uuid::createUuid();
    result.name = name;
    result.siteId = siteId;
    return result;
}

} // namespace

class EdgeDbDataProcessorTest: public ::testing::Test
{
protected:
    EdgeDbDataProcessorTest()
    {
        for (const auto& siteId: {kSite1Id, kSite2Id})
            addContext(siteId);

        edgeDb.organizationId = kOrganizationId;
        connection.handler = edgeDb.handler();
    }

    QnResourcePool* pool(const QString& siteId) const
    {
        return contexts.at(siteId)->resourcePool();
    }

    void givenDevice(const WithSite<DeviceModelV4>& model) { edgeDb.add("devices", model); }

    void givenLayout(const WithSite<LayoutData>& model) { edgeDb.add("layouts", model); }

    void givenConnectedSite(const QString& siteId) { connectedSites.insert(siteId); }

    void addContext(const QString& siteId)
    {
        auto context = std::unique_ptr<SystemContext>(
            appContext.createSystemContext(SystemContext::Mode::unitTests));
        appContext.addSystemContext(context.get());
        contexts[siteId] = std::move(context);
    }

    void whenDataProcessed()
    {
        // The processor handles the data when the loader becomes ready.
        loader.start(kOrganizationId);
        ASSERT_TRUE(waitFor([this]() { return loader.isReady(); }));
    }

protected:
    ApplicationContext appContext{
        ApplicationContext::Mode::unitTests, Qn::SerializationFormat::json};
    std::map<QString, std::unique_ptr<SystemContext>> contexts;
    std::set<QString> connectedSites;
    FakeEdgeDb edgeDb;
    TestEdgeDbConnection connection;
    EdgeDbDataLoader loader{&connection};
    EdgeDbDataProcessor processor{&loader,
        [this](const QString& siteId) -> EdgeDbDataProcessor::SiteContext
        {
            const auto it = contexts.find(siteId);
            if (it == contexts.end())
                return {};

            return {
                .systemContext = it->second.get(),
                .hasOwnDevices = connectedSites.contains(siteId),
            };
        }};
};

TEST_F(EdgeDbDataProcessorTest, devicesAreCreatedInTheirSiteContexts)
{
    const auto device1 = device(kSite1Id, "Camera 1");
    const auto device2 = device(kSite2Id, "Camera 2");
    givenDevice(device1);
    givenDevice(device2);

    whenDataProcessed();

    const auto camera = pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(device1.id);
    ASSERT_TRUE(camera);
    EXPECT_EQ(camera->systemId(), kSite1Id);
    EXPECT_EQ(camera->getName(), "Camera 1");
    EXPECT_TRUE(camera->hasFlags(Qn::cross_system | Qn::fake));
    EXPECT_EQ(camera->getProperty(kCustomGroupIdProperty), "Camera 1 group");
    EXPECT_EQ(camera->getGroupId(), "Camera 1 recorder");
    EXPECT_EQ(camera->getDefaultGroupName(), "Camera 1 recorder name");

    EXPECT_FALSE(pool(kSite1Id)->getResourceById(device2.id));
    EXPECT_TRUE(pool(kSite2Id)->getResourceById<CrossSystemCameraResource>(device2.id));
}

TEST_F(EdgeDbDataProcessorTest, devicesAreAddedWithTheirCustomGroups)
{
    const auto device1 = device(kSite1Id, "Camera 1");
    givenDevice(device1);

    // The resource tree places a Device into its custom group when the Device is added.
    QString customGroupWhenAdded;
    QObject::connect(pool(kSite1Id),
        &QnResourcePool::resourcesAdded,
        [&customGroupWhenAdded](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
                customGroupWhenAdded = resource->getProperty(kCustomGroupIdProperty);
        });

    whenDataProcessed();

    EXPECT_EQ(customGroupWhenAdded, "Camera 1 group");
}

TEST_F(EdgeDbDataProcessorTest, deviceWithoutGroupIsNotGrouped)
{
    auto device1 = device(kSite1Id, "Camera 1");
    device1.group.reset();
    givenDevice(device1);

    whenDataProcessed();

    const auto camera = pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(device1.id);
    ASSERT_TRUE(camera);
    EXPECT_TRUE(camera->getGroupId().isEmpty());
    EXPECT_TRUE(camera->getDefaultGroupName().isEmpty());
}

TEST_F(EdgeDbDataProcessorTest, deviceGroupIsUpdatedWithSiteData)
{
    const auto device1 = device(kSite1Id, "Camera 1");
    givenDevice(device1);
    whenDataProcessed();

    const auto camera = pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(device1.id);
    ASSERT_TRUE(camera);

    // The Site is connected, and the Device has been moved to another group since the load.
    CameraDataEx data;
    data.id = device1.id;
    data.typeId = nx::Uuid::createUuid();
    data.name = device1.name;
    data.groupId = "Another recorder";
    data.groupName = "Another recorder name";
    camera->update(data);

    EXPECT_EQ(camera->getGroupId(), "Another recorder");
    EXPECT_EQ(camera->getDefaultGroupName(), "Another recorder name");
}

TEST_F(EdgeDbDataProcessorTest, deviceTypeDefinesItsKind)
{
    auto desktopCamera = device(kSite1Id, "Desktop camera");
    desktopCamera.typeId = CameraData::kDesktopCameraTypeId;
    auto virtualCamera = device(kSite1Id, "Virtual camera");
    virtualCamera.typeId = CameraData::kVirtualCameraTypeId;
    const auto camera = device(kSite1Id, "Camera");
    givenDevice(desktopCamera);
    givenDevice(virtualCamera);
    givenDevice(camera);

    whenDataProcessed();

    const auto resource = [this](nx::Uuid id)
    {
        return pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(id);
    };

    ASSERT_TRUE(resource(desktopCamera.id));
    EXPECT_TRUE(resource(desktopCamera.id)->hasFlags(Qn::desktop_camera));
    EXPECT_EQ(resource(desktopCamera.id)->getTypeId(), CameraData::kDesktopCameraTypeId);

    ASSERT_TRUE(resource(virtualCamera.id));
    EXPECT_TRUE(resource(virtualCamera.id)->hasFlags(Qn::virtual_camera));

    ASSERT_TRUE(resource(camera.id));
    EXPECT_FALSE(resource(camera.id)->hasFlags(Qn::desktop_camera));
    EXPECT_FALSE(resource(camera.id)->hasFlags(Qn::virtual_camera));
}

TEST_F(EdgeDbDataProcessorTest, layoutsAreCreatedInTheirSiteContexts)
{
    const auto layout1 = layout(kSite1Id, "Layout 1");
    givenLayout(layout1);

    whenDataProcessed();

    const auto layoutResource = pool(kSite1Id)->getResourceById<LayoutResource>(layout1.id);
    ASSERT_TRUE(layoutResource);
    EXPECT_EQ(layoutResource->getName(), "Layout 1");
    EXPECT_TRUE(layoutResource->hasFlags(Qn::remote | Qn::fake));
    EXPECT_FALSE(layoutResource->hasFlags(Qn::cross_system));
}

TEST_F(EdgeDbDataProcessorTest, connectedSiteGetsLayoutsButNotDevices)
{
    const auto device1 = device(kSite1Id, "Camera 1");
    const auto layout1 = layout(kSite1Id, "Layout 1");
    givenDevice(device1);
    givenLayout(layout1);
    givenConnectedSite(kSite1Id);

    whenDataProcessed();

    EXPECT_FALSE(pool(kSite1Id)->getResourceById(device1.id));
    EXPECT_TRUE(pool(kSite1Id)->getResourceById<LayoutResource>(layout1.id));
}

TEST_F(EdgeDbDataProcessorTest, siteWithoutContextDoesNotAffectOthers)
{
    const auto unknownSiteDevice = device(kNoContextSiteId, "Camera 1");
    const auto unknownSiteLayout = layout(kNoContextSiteId, "Layout 1");
    const auto device2 = device(kSite2Id, "Camera 2");
    givenDevice(unknownSiteDevice);
    givenLayout(unknownSiteLayout);
    givenDevice(device2);

    whenDataProcessed();

    EXPECT_TRUE(pool(kSite2Id)->getResourceById(device2.id));
    for (const auto& [siteId, context]: contexts)
    {
        EXPECT_FALSE(context->resourcePool()->getResourceById(unknownSiteDevice.id));
        EXPECT_FALSE(context->resourcePool()->getResourceById(unknownSiteLayout.id));
    }
}

TEST_F(EdgeDbDataProcessorTest, siteContextAddedLaterGetsItsResources)
{
    const auto device1 = device(kNoContextSiteId, "Camera 1");
    const auto layout1 = layout(kNoContextSiteId, "Layout 1");
    givenDevice(device1);
    givenLayout(layout1);

    whenDataProcessed();
    processor.onSiteContextAdded(kSite1Id); //< Not pending, ignored.

    addContext(kNoContextSiteId);
    processor.onSiteContextAdded(kNoContextSiteId);

    EXPECT_TRUE(pool(kNoContextSiteId)->getResourceById<CrossSystemCameraResource>(device1.id));
    EXPECT_TRUE(pool(kNoContextSiteId)->getResourceById<LayoutResource>(layout1.id));
    EXPECT_FALSE(pool(kSite1Id)->getResourceById(device1.id));
}

TEST_F(EdgeDbDataProcessorTest, recreatedSiteContextGetsResourcesAgain)
{
    const auto device1 = device(kSite1Id, "Camera 1");
    const auto layout1 = layout(kSite1Id, "Layout 1");
    givenDevice(device1);
    givenLayout(layout1);
    whenDataProcessed();

    // A recreated context starts with an empty pool.
    pool(kSite1Id)->removeResources(pool(kSite1Id)->getResources());
    processor.onSiteContextAdded(kSite1Id);

    EXPECT_TRUE(pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(device1.id));
    EXPECT_TRUE(pool(kSite1Id)->getResourceById<LayoutResource>(layout1.id));
}

TEST_F(EdgeDbDataProcessorTest, newDataReplacesPendingSites)
{
    const auto device1 = device(kNoContextSiteId, "Camera 1");
    givenDevice(device1);
    whenDataProcessed();

    // The Site is not available anymore.
    edgeDb.collections.clear();
    whenDataProcessed();

    addContext(kNoContextSiteId);
    processor.onSiteContextAdded(kNoContextSiteId);

    EXPECT_FALSE(pool(kNoContextSiteId)->getResourceById(device1.id));
}

TEST_F(EdgeDbDataProcessorTest, devicesWithSiteDataAreNotChanged)
{
    const auto device1 = device(kSite1Id, "Camera from edgeDb");
    givenDevice(device1);

    // Loaded from the Site when it was connected.
    CameraDataEx data;
    data.id = device1.id;
    data.typeId = nx::Uuid::createUuid();
    data.name = "Existing camera";
    const auto existing =
        CrossSystemCameraResourcePtr(new CrossSystemCameraResource(kSite1Id, data));
    pool(kSite1Id)->addResource(existing);

    whenDataProcessed();

    const auto camera = pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(device1.id);
    EXPECT_EQ(camera, existing);
    EXPECT_EQ(camera->getName(), "Existing camera");
    EXPECT_TRUE(camera->getGroupId().isEmpty());
}

TEST_F(EdgeDbDataProcessorTest, devicesWithoutSiteDataAreUpdated)
{
    auto device1 = device(kSite1Id, "Camera from edgeDb");
    device1.typeId = CameraData::kVirtualCameraTypeId;
    givenDevice(device1);

    // Created for a cross-site Layout item, which knows only the id and the name.
    const auto existing = CrossSystemCameraResourcePtr(
        new CrossSystemCameraResource(kSite1Id, device1.id, "Layout item camera", /*typeId*/ {}));
    pool(kSite1Id)->addResource(existing);

    // The resource tree moves a shown Device to its custom group by this notification.
    std::set<QString> changedProperties;
    QObject::connect(existing.get(),
        &QnResource::propertyChanged,
        [&changedProperties](const QnResourcePtr&, const QString& key)
        { changedProperties.insert(key); });

    whenDataProcessed();

    EXPECT_TRUE(changedProperties.contains(kCustomGroupIdProperty));

    const auto camera = pool(kSite1Id)->getResourceById<CrossSystemCameraResource>(device1.id);
    EXPECT_EQ(camera, existing);
    EXPECT_EQ(camera->getName(), "Camera from edgeDb");
    EXPECT_EQ(camera->getTypeId(), CameraData::kVirtualCameraTypeId);
    EXPECT_TRUE(camera->hasFlags(Qn::cross_system | Qn::fake | Qn::virtual_camera));
    EXPECT_EQ(camera->getGroupId(), "Camera from edgeDb recorder");
    EXPECT_EQ(camera->getProperty(kCustomGroupIdProperty), "Camera from edgeDb group");
}

} // namespace test
} // namespace nx::vms::client::core
