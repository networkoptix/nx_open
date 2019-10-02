#include <core/resource_access/providers/direct_parent_server_monitor_access_provider_test_fixture.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

using namespace nx::core::access;

TEST_F(QnDirectParentServerMonitorAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, emptyServerIsntAccessible)
{
    auto server = addServer();
    auto adminUser = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(adminUser, server));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, serverInaccessibleIfCameraInaccessible)
{
    auto server = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = addCamera();
    camera->setParentId(server->getId());
    ASSERT_FALSE(accessProvider()->hasAccess(customUser, server));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, serverAccessibleIfCameraAccessible)
{
    auto server = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = addCamera();
    camera->setParentId(server->getId());
    sharedResourcesManager()->setSharedResources(customUser, {camera->getId()});
    ASSERT_TRUE(accessProvider()->hasAccess(customUser, server));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, accessUpdatedWhenCameraMovedBetweenServers)
{
    auto server1 = addServer();
    auto server2 = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = addCamera();
    camera->setParentId(server1->getId());
    sharedResourcesManager()->setSharedResources(customUser, {camera->getId()});

    ASSERT_TRUE(accessProvider()->hasAccess(customUser, server1));
    ASSERT_FALSE(accessProvider()->hasAccess(customUser, server2));

    camera->setParentId(server2->getId());

    ASSERT_FALSE(accessProvider()->hasAccess(customUser, server1));
    ASSERT_TRUE(accessProvider()->hasAccess(customUser, server2));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, desktopCameraDoesntGiveAccessToMotitor)
{
    auto server = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = createCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setParentId(server->getId());
    resourcePool()->addResource(camera);
    sharedResourcesManager()->setSharedResources(customUser, {camera->getId()});
    ASSERT_FALSE(accessProvider()->hasAccess(customUser, server));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, serverInaccessibleIfAccessibleCameraRemoved)
{
    auto server = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = addCamera();
    camera->setParentId(server->getId());
    sharedResourcesManager()->setSharedResources(customUser, {camera->getId()});
    resourcePool()->removeResource(camera);
    ASSERT_FALSE(accessProvider()->hasAccess(customUser, server));
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, checkSource)
{
    auto server = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = addCamera();
    camera->setParentId(server->getId());
    resourcePool()->addResource(camera);
    sharedResourcesManager()->setSharedResources(customUser, {camera->getId()});
    ASSERT_EQ(accessProvider()->accessibleVia(customUser, server), Source::childDevice);
}

TEST_F(QnDirectParentServerMonitorAccessProviderTest, accessProviders)
{
    auto server = addServer();
    auto customUser = addUser(GlobalPermission::customUser);
    auto camera = addCamera();
    camera->setParentId(server->getId());
    sharedResourcesManager()->setSharedResources(customUser, {camera->getId()});
    QnResourceList providers;
    accessProvider()->accessibleVia(customUser, server, &providers);
    ASSERT_TRUE(providers.contains(camera));
}
