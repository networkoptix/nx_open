// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_data_p.h"

#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connection_delegate_helper.h>

namespace nx::vms::client::desktop {

void SystemContext::Private::initializeNetworkModules()
{
    if (appContext()->commonFeatures().flags.testFlag(
            common::ApplicationContext::FeatureFlag::networking))
    {
        q->networkModule()->connectionFactory()->setUserInteractionDelegate(
            createConnectionUserInteractionDelegate(q,
                []()
                {
                    return appContext()->mainWindowContext()->mainWindowWidget();
                }));
    }
}

} // namespace nx::vms::client::desktop
