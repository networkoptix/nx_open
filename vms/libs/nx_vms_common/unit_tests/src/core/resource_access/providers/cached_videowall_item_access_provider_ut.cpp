// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <common/common_module.h>
#include <core/resource_access/providers/cached_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/videowall_item_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx/vms/api/data/user_role_data.h>

namespace nx::core::access {
namespace test {

class CachedVideoWallItemAccessProviderTest: public CachedBaseAccessProviderTestFixture
{
protected:
    virtual AbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new VideoWallItemAccessProvider(Mode::cached, commonModule());
    }
};

TEST_F(CachedVideoWallItemAccessProviderTest, checkSource)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::admin);

    ASSERT_EQ(accessProvider()->accessibleVia(user, target), Source::videowall);
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkByAccessRights)
{
    auto target = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(GlobalPermission::accessAllMedia);

    QnVideoWallItem item;
    item.layout = target->getId();
    videoWall->items()->addItem(item);

    // User has no access to videowalls.
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkAccessRightsChange)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::controlVideowall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkLayoutOnVideoWall)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::admin);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkLayoutAddedBeforeVideowall)
{
    auto user = addUser(GlobalPermission::controlVideowall);

    auto videoWall = createVideoWall();

    // What's going on on drop.
    auto layout = addLayoutForVideoWall(videoWall);

    awaitAccessValue(user, layout, Source::videowall);
    resourcePool()->addResource(videoWall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, layout));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkLayoutAddedBeforeVideowallItem)
{
    auto user = addUser(GlobalPermission::controlVideowall);

    auto videoWall = addVideoWall();

    auto layout = createLayout();
    layout->setParentId(videoWall->getId());
    resourcePool()->addResource(layout);

    awaitAccessValue(user, layout, Source::videowall);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, layout));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkLayoutAddedAfterVideowallItem)
{
    auto user = addUser(GlobalPermission::controlVideowall);

    auto videoWall = addVideoWall();

    auto layout = createLayout();
    layout->setParentId(videoWall->getId());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    awaitAccessValue(user, layout, Source::videowall);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(accessProvider()->hasAccess(user, layout));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkCameraOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::admin);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkPushMyScreen)
{
    auto target = addCamera();
    target->addFlags(Qn::desktop_camera);
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::controlVideowall);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkCameraOnLayoutAddedOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto user = addUser(GlobalPermission::controlVideowall);
    auto layout = createLayout();
    layout->addFlags(Qn::remote);
    layout->setParentId(videoWall->getId());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    awaitAccessValue(user, target, Source::videowall);
    resourcePool()->addResource(layout);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkCameraDroppedOnVideoWall_3_0)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto user = addUser(GlobalPermission::controlVideowall);

    /* What's going on on drop. */
    auto layout = createLayout();

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    resourcePool()->addResource(layout);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    awaitAccessValue(user, target, Source::videowall);
    layout->setParentId(videoWall->getId());

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkCameraDroppedOnVideoWall_3_1)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    QnVideoWallItem vwitem;
    vwitem.uuid = QnUuid::createUuid();
    videoWall->items()->addItem(vwitem);

    auto user = addUser(GlobalPermission::controlVideowall);

    // What's going on on drop.
    auto layout = createLayout();

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    layout->setParentId(videoWall->getId());
    resourcePool()->addResource(layout);

    awaitAccessValue(user, target, Source::videowall);

    vwitem.layout = layout->getId();
    videoWall->items()->updateItem(vwitem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkLayoutRemoved)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);

    auto user = addUser(GlobalPermission::controlVideowall);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    resourcePool()->removeResource(layout);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkCameraAddedOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::admin);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkVideoWallAdded)
{
    auto camera = addCamera();
    auto videoWall = createVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::admin);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    resourcePool()->addResource(videoWall);

    ASSERT_TRUE(accessProvider()->hasAccess(user, layout));
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}


TEST_F(CachedVideoWallItemAccessProviderTest, checkVideoWallRemoved)
{
    auto camera = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::admin);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    resourcePool()->removeResource(videoWall);

    ASSERT_FALSE(accessProvider()->hasAccess(user, layout));
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}


TEST_F(CachedVideoWallItemAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::controlVideowall);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(2, providers.size());
    ASSERT_TRUE(providers.contains(videoWall));
    ASSERT_TRUE(providers.contains(layout));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkByLayoutParentId)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::accessAllMedia);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    user->setRawPermissions(GlobalPermission::controlVideowall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedVideoWallItemAccessProviderTest, checkCameraOnVideoWallByParentId)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(GlobalPermission::controlVideowall);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

} // namespace test
} // namespace nx::core::access
