#include <gtest/gtest.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource_stub.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/direct_resource_access_provider.h>

namespace {

}

class QnDirectResourceAccessProviderTest: public testing::Test,
    protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule());
        m_accessProvider.reset(new QnDirectResourceAccessProvider());
        QObject::connect(accessProvider(),
            &QnAbstractResourceAccessProvider::accessChanged,
            [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
                bool value)
            {
                at_accessChanged(subject, resource, value);
            });
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        ASSERT_TRUE(m_awaitedAccessQueue.empty());
        m_accessProvider.clear();
        m_module.clear();
    }

    QnAbstractResourceAccessProvider* accessProvider() const
    {
        return m_accessProvider.data();
    }

    ec2::ApiUserGroupData createRole(Qn::GlobalPermissions permissions) const
    {
        return ec2::ApiUserGroupData(QnUuid::createUuid(), QStringLiteral("test_role"),
            permissions);
    }

    void awaitAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value = true)
    {
        m_awaitedAccessQueue.emplace_back(subject, resource, value);
    }

    void at_accessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value)
    {
        ASSERT_EQ(value, accessProvider()->hasAccess(subject, resource));

        if (m_awaitedAccessQueue.empty())
            return;

        auto awaited = m_awaitedAccessQueue.front();
        if (awaited.subject == subject && awaited.resource == resource)
        {
            m_awaitedAccessQueue.pop_front();
            ASSERT_EQ(value, awaited.value);
        }
    }

private:
    QSharedPointer<QnCommonModule> m_module;
    QSharedPointer<QnAbstractResourceAccessProvider> m_accessProvider;

    struct AwaitedAccess
    {
        AwaitedAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            bool value)
            :
            subject(subject),
            resource(resource),
            value(value)
        {
        }

        QnResourceAccessSubject subject;
        QnResourcePtr resource;
        bool value;
    };
    std::deque<AwaitedAccess> m_awaitedAccessQueue;
};

TEST_F(QnDirectResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccess(user, target, true);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
    awaitAccess(user, target, false);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>());
}
