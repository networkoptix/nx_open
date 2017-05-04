#include <common/common_module.h>

#include <core/resource_access/providers/base_access_provider_test_fixture.h>
#include <core/resource_access/providers/layout_tour_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_role_data.h>

namespace {

static const auto kSource = QnAbstractResourceAccessProvider::Source::tour;

} // namespace

class QnLayoutTourAccessProviderTest: public QnBaseAccessProviderTestFixture
{
protected:
    void awaitAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value = true)
    {
        if (value)
            awaitAccessValue(subject, resource, kSource);
        else
            awaitAccessValue(subject, resource, QnAbstractResourceAccessProvider::Source::none);
    }

    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnLayoutTourAccessProvider(commonModule());
    }
};

TEST_F(QnLayoutTourAccessProviderTest, checkDefaultAccess)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();
    target->setParentId(tour.id);

    resourcePool()->addResource(target);
    layoutTourManager()->addOrUpdateTour(tour);
    resourcePool()->addResource(user);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
    ASSERT_EQ(accessProvider()->accessibleVia(user, target), kSource);
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessOnLayoutAdded)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();
    target->setParentId(tour.id);

    layoutTourManager()->addOrUpdateTour(tour);
    resourcePool()->addResource(user);
    resourcePool()->addResource(target);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessOnTourAdded)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();
    target->setParentId(tour.id);

    resourcePool()->addResource(target);
    resourcePool()->addResource(user);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    layoutTourManager()->addOrUpdateTour(tour);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessOnOtherUserTour)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = QnUuid::createUuid();

    auto target = createLayout();
    target->setParentId(tour.id);

    resourcePool()->addResource(target);
    layoutTourManager()->addOrUpdateTour(tour);
    resourcePool()->addResource(user);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessOnOtherUserLayout)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();
    target->setParentId(user->getId());

    resourcePool()->addResource(target);
    layoutTourManager()->addOrUpdateTour(tour);
    resourcePool()->addResource(user);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessOnSharedLayout)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();

    resourcePool()->addResource(target);
    layoutTourManager()->addOrUpdateTour(tour);
    resourcePool()->addResource(user);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessAdded)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();
    target->setParentId(tour.id);

    resourcePool()->addResource(user);
    resourcePool()->addResource(target);

    awaitAccess(user, target);
    layoutTourManager()->addOrUpdateTour(tour);
}

TEST_F(QnLayoutTourAccessProviderTest, checkAccessRemoved)
{
    auto user = createUser(Qn::NoGlobalPermissions);

    ec2::ApiLayoutTourData tour;
    tour.id = QnUuid::createUuid();
    tour.parentId = user->getId();

    auto target = createLayout();
    target->setParentId(tour.id);

    resourcePool()->addResource(target);
    resourcePool()->addResource(user);
    layoutTourManager()->addOrUpdateTour(tour);

    awaitAccess(user, target, false);
    layoutTourManager()->removeTour(tour.id);
}
