#include "access_provider_test_fixture.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/providers/abstract_resource_access_provider.h>

void QnAccessProviderTestFixture::SetUp()
{
    m_module.reset(new QnCommonModule());
    m_accessProvider.reset(createAccessProvider());
    QObject::connect(accessProvider(),
        &QnAbstractResourceAccessProvider::accessChanged,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            QnAbstractResourceAccessProvider::Source value)
    {
        at_accessChanged(subject, resource,
            value != QnAbstractResourceAccessProvider::Source::none);
    });
}

void QnAccessProviderTestFixture::TearDown()
{
    ASSERT_TRUE(m_awaitedAccessQueue.empty());
    m_accessProvider.clear();
    m_module.clear();
}

QnAbstractResourceAccessProvider* QnAccessProviderTestFixture::accessProvider() const
{
    return m_accessProvider.data();
}

ec2::ApiUserGroupData QnAccessProviderTestFixture::createRole(Qn::GlobalPermissions permissions) const
{
    return ec2::ApiUserGroupData(QnUuid::createUuid(), QStringLiteral("test_role"),
        permissions);
}

void QnAccessProviderTestFixture::awaitAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, bool value)
{
    m_awaitedAccessQueue.emplace_back(subject, resource, value);
}

void QnAccessProviderTestFixture::at_accessChanged(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, bool value)
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
