// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "direct_access_provider_test_fixture.h"

#include <core/resource_access/providers/abstract_resource_access_provider.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

namespace nx::core::access {
namespace test {

void DirectAccessProviderTestFixture::SetUp()
{
    m_staticCommon = std::make_unique<QnStaticCommonModule>();
    m_module = std::make_unique<QnCommonModule>(
        /*clientMode*/ false,
        nx::core::access::Mode::direct);
    initializeContext(m_module.get());
}

void DirectAccessProviderTestFixture::TearDown()
{
    deinitializeContext();
    m_module.reset();
    m_staticCommon.reset();
}

} // namespace test
} // namespace nx::core::access
