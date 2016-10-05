#include "access_provider_test_fixture.h"

#include <common/common_module.h>

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/providers/abstract_resource_access_provider.h>

void QnAccessProviderTestFixture::SetUp()
{
    m_module.reset(new QnCommonModule());

    for (auto provider : qnResourceAccessProvider->providers())
    {
        qnResourceAccessProvider->removeBaseProvider(provider);
        delete provider;
    }

    m_accessProvider = createAccessProvider();
    qnResourceAccessProvider->addBaseProvider(m_accessProvider);

    QObject::connect(m_accessProvider,
        &QnAbstractResourceAccessProvider::accessChanged,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            QnAbstractResourceAccessProvider::Source value)
        {
            at_accessChanged(subject, resource, value);
        });
}

void QnAccessProviderTestFixture::TearDown()
{
    ASSERT_TRUE(m_awaitedAccessQueue.empty());
    m_module.clear();
}

QnAbstractResourceAccessProvider* QnAccessProviderTestFixture::accessProvider() const
{
    return m_accessProvider;
}

void QnAccessProviderTestFixture::awaitAccessValue(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, QnAbstractResourceAccessProvider::Source value)
{
    m_awaitedAccessQueue.emplace_back(subject, resource, value);
}

void QnAccessProviderTestFixture::at_accessChanged(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, QnAbstractResourceAccessProvider::Source value)
{
    ASSERT_EQ(value, accessProvider()->accessibleVia(subject, resource));
    ASSERT_EQ(value != QnAbstractResourceAccessProvider::Source::none,
        accessProvider()->hasAccess(subject, resource));

    if (m_awaitedAccessQueue.empty())
        return;

    auto awaited = m_awaitedAccessQueue.front();
    if (awaited.subject == subject && awaited.resource == resource)
    {
        m_awaitedAccessQueue.pop_front();
        ASSERT_EQ(value, awaited.value);
    }
}

