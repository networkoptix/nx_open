// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "direct_base_access_provider_test_fixture.h"

#include <core/resource_access/providers/resource_access_provider.h>

namespace nx::core::access {
namespace test {

void DirectBaseAccessProviderTestFixture::SetUp()
{
    m_accessProvider = createAccessProvider();
    for (auto provider: resourceAccessProvider()->providers())
    {
        resourceAccessProvider()->removeBaseProvider(provider);
        delete provider;
    }
    resourceAccessProvider()->addBaseProvider(m_accessProvider);
}

AbstractResourceAccessProvider* DirectBaseAccessProviderTestFixture::accessProvider() const
{
    return m_accessProvider;
}

} // namespace test
} // namespace nx::core::access
