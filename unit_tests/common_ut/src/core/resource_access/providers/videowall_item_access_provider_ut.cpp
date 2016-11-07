#include <core/resource_access/providers/base_access_provider_test_fixture.h>
#include <core/resource_access/providers/videowall_item_access_provider.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx_ec/data/api_user_group_data.h>

class QnVideoWallItemAccessProviderTest: public QnBaseAccessProviderTestFixture
{
protected:
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnVideoWallItemAccessProvider();
    }
};

TEST_F(QnVideoWallItemAccessProviderTest, checkSource)
{
    auto target = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnVideoWallItem item;
    item.layout = target->getId();
    videoWall->items()->addItem(item);

    ASSERT_EQ(accessProvider()->accessibleVia(user, target),
        QnAbstractResourceAccessProvider::Source::videowall);
}

TEST_F(QnVideoWallItemAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkByAccessRights)
{
    auto target = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);

    QnVideoWallItem item;
    item.layout = target->getId();
    videoWall->items()->addItem(item);

    /* User has no access to videowalls */
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkAccessRightsChange)
{
    auto target = addLayout();
    auto videoWall = addVideoWall();

    QnVideoWallItem item;
    item.layout = target->getId();
    videoWall->items()->addItem(item);

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkLayoutOnVideoWall)
{
    auto target = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnVideoWallItem item;
    item.layout = target->getId();
    videoWall->items()->addItem(item);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkCameraOnVideoWall)
{
    auto target = addCamera();
    auto layout = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkLayoutRemoved)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    qnResPool->removeResource(layout);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkCameraAddedOnVideoWall)
{
    auto target = addCamera();
    auto layout = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkVideoWallAdded)
{
    auto camera = addCamera();
    auto layout = addLayout();
    auto videoWall = createVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    qnResPool->addResource(videoWall);

    ASSERT_TRUE(accessProvider()->hasAccess(user, layout));
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}


TEST_F(QnVideoWallItemAccessProviderTest, checkVideoWallRemoved)
{
    auto camera = addCamera();
    auto layout = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    qnResPool->removeResource(videoWall);

    ASSERT_FALSE(accessProvider()->hasAccess(user, layout));
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}


TEST_F(QnVideoWallItemAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto layout = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(1, providers.size());
    ASSERT_TRUE(providers.contains(videoWall));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkByLayoutParentId)
{
    auto videoWall = addVideoWall();
    auto target = addLayout();
    target->setParentId(videoWall->getId());
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallItemAccessProviderTest, checkCameraOnVideoWallByParentId)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}
