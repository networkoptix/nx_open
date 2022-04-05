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
    m_module.reset();
    m_staticCommon.reset();
}

void CachedAccessProviderTestFixture::expectAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, Source value)
{
    ASSERT_EQ(value, accessProvider()->accessibleVia(subject, resource));
    ASSERT_EQ(value != Source::none, accessProvider()->hasAccess(subject, resource));

    NX_MUTEX_LOCKER lock(&m_mutex);
    const AccessKey key{subject.id(), resource->getId()};
    while (m_notifiedAccess[key] != value)
    {
        if (!m_condition.wait(&m_mutex, std::chrono::seconds(5)))
        {
            FAIL() << NX_FMT("Expected access %1 -> %2 is %3, actual %4",
                subject, resource, value, m_notifiedAccess[key])
                .toStdString();
            return;
        }
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
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_notifiedAccess[AccessKey{subject.id(), resource->getId()}] = value;
            m_condition.wakeAll();
        });
}

bool CachedAccessProviderTestFixture::AccessKey::operator<(const AccessKey& rhs) const
{
    if (subject != rhs.subject)
        return subject < rhs.subject;
    return resource < rhs.resource;
}

bool CachedAccessProviderTestFixture::AccessKey::operator==(const AccessKey& rhs) const
{
    return subject == rhs.subject && resource == rhs.resource;
}

} // namespace test
} // namespace nx::core::access
