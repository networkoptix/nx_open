#include <core/resource_access/providers/access_provider_test_fixture.h>
#include <core/resource_access/providers/videowall_resource_access_provider.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx_ec/data/api_user_group_data.h>

class QnVideoWallResourceAccessProviderTest: public QnAccessProviderTestFixture
{
protected:
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnVideoWallResourceAccessProvider();
    }
};

TEST_F(QnVideoWallResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnVideoWallResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnVideoWallResourceAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallResourceAccessProviderTest, checkByAccessRights)
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

TEST_F(QnVideoWallResourceAccessProviderTest, checkLayoutOnVideoWall)
{
    auto target = addLayout();
    auto videoWall = addVideoWall();
    auto user = addUser(Qn::GlobalAdminPermission);

    QnVideoWallItem item;
    item.layout = target->getId();
    videoWall->items()->addItem(item);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnVideoWallResourceAccessProviderTest, checkCameraOnVideoWall)
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

TEST_F(QnVideoWallResourceAccessProviderTest, checkCameraAddedOnVideoWall)
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
