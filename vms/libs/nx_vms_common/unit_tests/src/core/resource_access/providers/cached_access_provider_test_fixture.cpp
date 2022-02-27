// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cached_access_provider_test_fixture.h"

#include <core/resource_access/providers/abstract_resource_access_provider.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

namespace nx::core::access {
namespace test {

void CachedAccessProviderTestFixture::SetUp()
{
    m_staticCommon = std::make_unique<QnStaticCommonModule>();
    m_module = std::make_unique<QnCommonModule>(
        /*clientMode*/ false,
        nx::core::access::Mode::cached);
    initializeContext(m_module.get());
}

void CachedAccessProviderTestFixture::TearDown()
{
    deinitializeContext();
    ASSERT_TRUE(m_awaitedAccessQueue.empty());
    m_module.reset();
    m_staticCommon.reset();
}

void CachedAccessProviderTestFixture::awaitAccessValue(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, Source value)
{
    m_awaitedAccessQueue.emplace_back(subject, resource, value);
}

void CachedAccessProviderTestFixture::at_accessChanged(const QnResourceAccessSubject& subject,
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

void CachedAccessProviderTestFixture::setupAwaitAccess()
{
    QObject::connect(accessProvider(),
        &AbstractResourceAccessProvider::accessChanged,
        [this](const QnResourceAccessSubject& subject,
            const QnResourcePtr& resource,
            Source value)
        {
            at_accessChanged(subject, resource, value);
        });
}

} // namespace test
} // namespace nx::core::access
