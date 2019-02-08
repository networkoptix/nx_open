#include "direct_access_provider_test_fixture.h"

#include <common/common_module.h>

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/providers/abstract_resource_access_provider.h>

void QnDirectAccessProviderTestFixture::SetUp()
{
    m_module.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
    initializeContext(m_module.data());
}

void QnDirectAccessProviderTestFixture::TearDown()
{
    deinitializeContext();
    m_module.clear();
}
