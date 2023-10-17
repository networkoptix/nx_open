// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_internet_access_watcher.h"

#include <algorithm>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

bool haveInternetAccess(const QnMediaServerResourceList& servers)
{
    return std::any_of(servers.cbegin(), servers.cend(),
        [](const QnMediaServerResourcePtr& server) { return server->hasInternetAccess(); });
}

} // namespace

SystemInternetAccessWatcher::SystemInternetAccessWatcher(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
}

void SystemInternetAccessWatcher::start()
{
    auto serverChangesListener =
        new core::SessionResourcesSignalListener<QnMediaServerResource>(systemContext(), this);

    auto recalculateAll =
        [this]() { setHasInternetAccess(haveInternetAccess(resourcePool()->servers())); };

    auto updateStateIfNeeded =
        [this, recalculateAll](const QnMediaServerResourcePtr& server)
        {
            if (!m_hasInternetAccess && server->hasInternetAccess())
                setHasInternetAccess(true);
            // Recalculate everything if this was the only server which provided the Internet.
            else if (m_hasInternetAccess)
                recalculateAll();
        };

    serverChangesListener->addOnSignalHandler(
        &QnMediaServerResource::serverFlagsChanged,
        updateStateIfNeeded);

    serverChangesListener->addOnSignalHandler(
        &QnMediaServerResource::statusChanged,
        updateStateIfNeeded);

    serverChangesListener->setOnAddedHandler(
        [this](const QnMediaServerResourceList& servers)
        {
            if (!m_hasInternetAccess && haveInternetAccess(servers))
                setHasInternetAccess(true);
        });
    serverChangesListener->setOnRemovedHandler(
        [this, recalculateAll](const QnMediaServerResourceList& servers)
        {
            if (m_hasInternetAccess)
                recalculateAll();
        });
    serverChangesListener->start();
}

SystemInternetAccessWatcher::~SystemInternetAccessWatcher()
{
}

bool SystemInternetAccessWatcher::systemHasInternetAccess() const
{
    return m_hasInternetAccess;
}

void SystemInternetAccessWatcher::setHasInternetAccess(bool value)
{
    if (m_hasInternetAccess == value)
        return;

    m_hasInternetAccess = value;
    emit internetAccessChanged(m_hasInternetAccess, QPrivateSignal());
}

} // namespace nx::vms::client::desktop
