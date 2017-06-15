#include "cached_access_provider_test_fixture.h"

#include <common/common_module.h>

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/providers/abstract_resource_access_provider.h>

using namespace nx::core::access;

void QnCachedAccessProviderTestFixture::SetUp()
{
    m_module.reset(new QnCommonModule(false, nx::core::access::Mode::cached));
    initializeContext(m_module.data());
}

void QnCachedAccessProviderTestFixture::TearDown()
{
    deinitializeContext();
    ASSERT_TRUE(m_awaitedAccessQueue.empty());
    m_module.clear();
}

void QnCachedAccessProviderTestFixture::awaitAccessValue(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, Source value)
{
    m_awaitedAccessQueue.emplace_back(subject, resource, value);
}

void QnCachedAccessProviderTestFixture::at_accessChanged(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, Source value)
{
    ASSERT_EQ(value, accessProvider()->accessibleVia(subject, resource));
    ASSERT_EQ(value != Source::none,
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

void QnCachedAccessProviderTestFixture::setupAwaitAccess()
{
    QObject::connect(accessProvider(),
        &QnAbstractResourceAccessProvider::accessChanged,
        [this](const QnResourceAccessSubject& subject,
            const QnResourcePtr& resource,
            Source value)
        {
            at_accessChanged(subject, resource, value);
        });
}

