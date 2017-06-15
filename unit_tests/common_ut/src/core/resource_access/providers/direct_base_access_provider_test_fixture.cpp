#include "direct_base_access_provider_test_fixture.h"

#include <core/resource_access/providers/resource_access_provider.h>

void QnDirectBaseAccessProviderTestFixture::SetUp()
{
    base_type::SetUp();
    m_accessProvider = createAccessProvider();
    for (auto provider: resourceAccessProvider()->providers())
    {
        resourceAccessProvider()->removeBaseProvider(provider);
        delete provider;
    }
    resourceAccessProvider()->addBaseProvider(m_accessProvider);
}

void QnDirectBaseAccessProviderTestFixture::TearDown()
{
    base_type::TearDown();
}

QnAbstractResourceAccessProvider* QnDirectBaseAccessProviderTestFixture::accessProvider() const
{
    return m_accessProvider;
}
