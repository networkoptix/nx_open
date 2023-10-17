// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

namespace nx::vms::client::desktop {

namespace test {

class MimeDataTest: public ContextBasedTest
{
protected:
    QnMediaServerResourcePtr createServerResource() const
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource());
        server->setIdUnsafe(QnUuid::createUuid());
        server->setName("server");
        systemContext()->resourcePool()->addResource(server);
        return server;
    }

    QnVirtualCameraResourcePtr createCameraResource(const QnUuid& parentId) const
    {
        QnVirtualCameraResourcePtr camera(new CameraResourceStub());
        camera->setIdUnsafe(QnUuid::createUuid());
        camera->setName("camera");
        camera->setParentId(parentId);
        systemContext()->resourcePool()->addResource(camera);
        return camera;
    }
};

TEST_F(MimeDataTest, mimeDataConversionsAreCorrect)
{
    static constexpr auto kTestMimeType = "application/x-test-mime-type";
    static constexpr auto kTestMimeData = "Test MIME Data";

    const auto serverResource = createServerResource();
    const auto cameraResource = createCameraResource(serverResource->getId());

    QnResourceList resources({
        serverResource,
        cameraResource});

    QnUuidList entities({
        QnUuid::createUuid(),
        QnUuid::createUuid(),
        QnUuid::createUuid()});

    menu::Parameters::ArgumentHash arguments({
        {0, 9000},
        {1, QString("9000")},
        {2, QUuid::createUuid()}});

    MimeData mimeData;
    mimeData.setResources(resources);
    mimeData.setEntities(entities);
    mimeData.setArguments(arguments);
    mimeData.setData(kTestMimeType, kTestMimeData);

    // Convert MimeData instance to the QMimeData.
    std::unique_ptr<QMimeData> qtMimeData(mimeData.createMimeData());

    // Convert QMimeData back to the MimeData.
    MimeData restoredMimeData(qtMimeData.get());

    // Ensure that resources, entities, arguments and custom type MIME data remain unchanged after
    // consversions.
    ASSERT_EQ(restoredMimeData.resources(), resources);
    ASSERT_EQ(restoredMimeData.entities(), entities);
    ASSERT_EQ(restoredMimeData.arguments(), arguments);
    ASSERT_EQ(restoredMimeData.data(kTestMimeType), kTestMimeData);
}

} // namespace test

} // namespace nx::vms::client::desktop
