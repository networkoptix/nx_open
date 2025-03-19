// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_interface_watcher.h"

#include <core/resource_management/resource_discovery_manager.h>
#include <mobile_client/mobile_client_module.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/window_context.h>

namespace nx::vms::client::mobile {

NetworkInterfaceWatcher::NetworkInterfaceWatcher(QObject* parent):
    QObject(parent),
    WindowContextAware(WindowContext::fromQmlContext(this))
{
}

void NetworkInterfaceWatcher::onContextReady()
{
    const auto updateManager =
        [this]()
        {
            const auto newContext = mainSystemContext();
            const auto manager = newContext
                ? newContext->resourceDiscoveryManager()
                : nullptr;

            if (m_discoveryManager == manager)
                return;

            if (m_discoveryManager)
                m_discoveryManager->disconnect(this);

            m_discoveryManager = manager;
            if (m_discoveryManager)
                return;

            connect(m_discoveryManager, &QnResourceDiscoveryManager::localInterfacesChanged,
                this, &NetworkInterfaceWatcher::interfacesChanged);
        };

    updateManager();
    connect(windowContext(), &WindowContext::mainSystemContextChanged, this, updateManager);
}

} // namespace nx::vms::client::mobile
