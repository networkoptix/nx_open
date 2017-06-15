#include <core/resource_access/providers/direct_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/videowall_item_access_provider.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <common/common_module.h>

#include <nx_ec/data/api_user_role_data.h>

using namespace nx::core::access;

class QnDirectVideoWallItemAccessProviderTest: public QnDirectBaseAccessProviderTestFixture
{
protected:
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnVideoWallItemAccessProvider(Mode::direct, commonModule());
    }

    QnLayoutResourcePtr addLayoutForVideoWall(const QnVideoWallResourcePtr& videoWall)
    {
        auto layout = createLayout();
        layout->setParentId(videoWall->getId());
        resourcePool()->addResource(layout);
        return layout;
    }
};

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkSource)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAdminPermission);

    ASSERT_EQ(accessProvider()->accessibleVia(user, target),
        QnAbstractResourceAccessProvider::Source::videowall);
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkByAccessRights)
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

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkAccessRightsChange)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkLayoutOnVideoWall)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAdminPermission);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkCameraOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAdminPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkCameraOnLayoutAddedOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    auto layout = createLayout();
    layout->addFlags(Qn::remote);
    layout->setParentId(videoWall->getId());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    item.resource.uniqueId = target->getUniqueId();
    layout->addItem(item);

    resourcePool()->addResource(layout);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkCameraDroppedOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    /* What's going on on drop. */
    auto layout = createLayout();

    QnLayoutItemData item;
    item.resource.id = target->getId();
    item.resource.uniqueId = target->getUniqueId();
    layout->addItem(item);
    resourcePool()->addResource(layout);

    layout->setParentId(videoWall->getId());

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkLayoutRemoved)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);

    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    resourcePool()->removeResource(layout);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkCameraAddedOnVideoWall)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAdminPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkVideoWallAdded)
{
    auto camera = addCamera();
    auto videoWall = createVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAdminPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    resourcePool()->addResource(videoWall);

    ASSERT_TRUE(accessProvider()->hasAccess(user, layout));
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}


TEST_F(QnDirectVideoWallItemAccessProviderTest, checkVideoWallRemoved)
{
    auto camera = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAdminPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    resourcePool()->removeResource(videoWall);

    ASSERT_FALSE(accessProvider()->hasAccess(user, layout));
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}


TEST_F(QnDirectVideoWallItemAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(2, providers.size());
    ASSERT_TRUE(providers.contains(videoWall));
    ASSERT_TRUE(providers.contains(layout));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkByLayoutParentId)
{
    auto videoWall = addVideoWall();
    auto target = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectVideoWallItemAccessProviderTest, checkCameraOnVideoWallByParentId)
{
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = target->getId();
    layout->addItem(layoutItem);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}
