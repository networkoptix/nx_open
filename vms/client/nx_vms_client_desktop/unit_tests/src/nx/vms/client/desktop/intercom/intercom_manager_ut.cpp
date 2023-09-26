// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <client/desktop_client_message_processor.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/intercom/intercom_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/test_support/client_camera_resource_stub.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api;

static constexpr AccessRights kRequiredAccessRights = AccessRight::view | AccessRight::userInput;

class IntercomManagerTest: public ContextBasedTest
{
protected:
    virtual void SetUp() override
    {
        messageProcessor = systemContext()->createMessageProcessor<
            QnDesktopClientMessageProcessor>();
        intercomManager.reset(new IntercomManager(systemContext()));
    }

    virtual void TearDown() override
    {
        intercomManager.reset();
        logout();
        ContextBasedTest::TearDown();
        systemContext()->deleteMessageProcessor();
    }

    nx::vms::client::core::AccessController* accessController() const
    {
        return systemContext()->accessController();
    }

    void logout()
    {
        if (!accessController()->user())
            return;

        emulateConnectionTerminated();
        accessController()->setUser({});
    }

    void loginAs(Ids groupIds, const QString& name = "user")
    {
        logout();
        auto user = createUser(groupIds, name);
        resourcePool()->addResource(user);
        emulateConnectionEstablished();
        accessController()->setUser(user);
    }

    QnVirtualCameraResourcePtr createCamera()
    {
        return ClientCameraResourceStubPtr(new ClientCameraResourceStub());
    }

    QnVirtualCameraResourcePtr addIntercomCamera()
    {
        const auto camera = createCamera();
        initializeIntercom(camera);
        resourcePool()->addResource(camera);
        return camera;
    }

    void initializeIntercom(const QnVirtualCameraResourcePtr& camera) const
    {
        static const QString kOpenDoorPortName = QString::fromStdString(
            nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::powerRelay));

        QnIOPortData intercomFeaturePort;
        intercomFeaturePort.outputName = kOpenDoorPortName;

        camera->setIoPortDescriptions({intercomFeaturePort}, false);
    }

    QnLayoutResourceList layouts() const
    {
        return resourcePool()->getResources<QnLayoutResource>();
    }

    void emulateConnectionEstablished()
    {
        emit messageProcessor->connectionOpened();
        emit messageProcessor->initialResourcesReceived();
    }

    void emulateConnectionTerminated()
    {
        emit messageProcessor->connectionClosed();
    }

protected:
    QPointer<QnClientMessageProcessor> messageProcessor;
    std::unique_ptr<IntercomManager> intercomManager;
};

TEST_F(IntercomManagerTest, createIntercomLayoutWhenIntercomIsCreated)
{
    loginAs(kPowerUsersGroupId);
    EXPECT_TRUE(layouts().empty());

    const auto intercomCamera = addIntercomCamera();
    ASSERT_EQ(layouts().size(), 1);
    EXPECT_TRUE(nx::vms::common::isIntercomOnIntercomLayout(intercomCamera, layouts()[0]));
    EXPECT_TRUE(layouts()[0]->hasFlags(Qn::local));
}

TEST_F(IntercomManagerTest, dontCreateIntercomLayoutWithoutRequiredPermissions)
{
    loginAs(kViewersGroupId);
    EXPECT_TRUE(layouts().empty());

    const auto intercomCamera = addIntercomCamera();
    ASSERT_TRUE(layouts().empty());
}

TEST_F(IntercomManagerTest, createIntercomLayoutWhenIntercomIsInitialized)
{
    loginAs(kPowerUsersGroupId);
    EXPECT_TRUE(layouts().empty());

    const auto camera = createCamera();
    resourcePool()->addResource(camera);
    EXPECT_TRUE(layouts().empty());

    initializeIntercom(camera);

    ASSERT_EQ(layouts().size(), 1);
    EXPECT_TRUE(nx::vms::common::isIntercomOnIntercomLayout(camera, layouts()[0]));
    EXPECT_TRUE(layouts()[0]->hasFlags(Qn::local));
}

TEST_F(IntercomManagerTest, createIntercomLayoutWhenIntercomAccessIsGranted)
{
    loginAs(NoGroup);

    const auto intercomCamera = addIntercomCamera();
    EXPECT_TRUE(layouts().empty());

    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        accessController()->user()->getId(),
        {{intercomCamera->getId(), kRequiredAccessRights}});

    ASSERT_EQ(layouts().size(), 1);
    EXPECT_TRUE(nx::vms::common::isIntercomOnIntercomLayout(intercomCamera, layouts()[0]));
    EXPECT_TRUE(layouts()[0]->hasFlags(Qn::local));
}

TEST_F(IntercomManagerTest, removeLocalIntercomLayoutWhenIntercomIsDeleted)
{
    const auto intercomCamera = addIntercomCamera();

    loginAs(kPowerUsersGroupId);
    EXPECT_EQ(layouts().size(), 1);

    resourcePool()->removeResource(intercomCamera);
    EXPECT_TRUE(layouts().empty());
}

TEST_F(IntercomManagerTest, removeLocalIntercomLayoutWhenIntercomAccessIsRevoked)
{
    loginAs(NoGroup);

    const auto intercomCamera = addIntercomCamera();

    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        accessController()->user()->getId(),
        {{intercomCamera->getId(), kRequiredAccessRights}});

    EXPECT_EQ(layouts().size(), 1);

    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        accessController()->user()->getId(), {});

    EXPECT_TRUE(layouts().empty());
}

TEST_F(IntercomManagerTest, dontCreateIntercomLayoutIfOneExists)
{
    const auto intercomCamera = addIntercomCamera();
    addIntercomLayout(intercomCamera);
    EXPECT_EQ(layouts().size(), 1);

    loginAs(kPowerUsersGroupId);
    EXPECT_EQ(layouts().size(), 1);
}

//TEST_F(IntercomManagerTest, removeRemoteIntercomLayoutWhenIntercomIsDeleted)
//{
//  This test is currently not implemented. It requires a full client-server interaction test.
//}

TEST_F(IntercomManagerTest, dontRemoveRemoteIntercomLayoutWhenIntercomAccessIsRevoked)
{
    loginAs(NoGroup);

    const auto intercomCamera = addIntercomCamera();
    const auto layout = addIntercomLayout(intercomCamera);
    EXPECT_FALSE(layout->hasFlags(Qn::local));

    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        accessController()->user()->getId(),
        {{intercomCamera->getId(), kRequiredAccessRights}});

    EXPECT_EQ(layouts().size(), 1);

    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        accessController()->user()->getId(), {});

    EXPECT_EQ(layouts().size(), 1);
}

} // namespace test
} // namespace nx::vms::client::desktop
