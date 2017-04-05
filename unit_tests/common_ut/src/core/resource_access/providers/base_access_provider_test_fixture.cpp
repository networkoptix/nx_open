#include "base_access_provider_test_fixture.h"

#include <core/resource_access/providers/resource_access_provider.h>

void QnBaseAccessProviderTestFixture::SetUp()
{
    base_type::SetUp();
    m_accessProvider = createAccessProvider();
    for (auto provider: resourceAccessProvider()->providers())
    {
        resourceAccessProvider()->removeBaseProvider(provider);
        delete provider;
    }
    resourceAccessProvider()->addBaseProvider(m_accessProvider);
    setupAwaitAccess();
}

void QnBaseAccessProviderTestFixture::TearDown()
{
    base_type::TearDown();
}

QnAbstractResourceAccessProvider* QnBaseAccessProviderTestFixture::accessProvider() const
{
    return m_accessProvider;
}
