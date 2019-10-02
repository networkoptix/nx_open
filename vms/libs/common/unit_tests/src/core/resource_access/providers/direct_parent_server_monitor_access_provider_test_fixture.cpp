#include "direct_parent_server_monitor_access_provider_test_fixture.h"

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/parent_server_monitor_access_provider.h>

QnAbstractResourceAccessProvider* QnDirectParentServerMonitorAccessProviderTest::accessProvider() const
{
    for (auto provider: resourceAccessProvider()->providers())
    {
        if (dynamic_cast<QnParentServerMonitorAccessProvider*>(provider))
            return provider;
    }
    NX_ASSERT(false);
    return nullptr;
}
