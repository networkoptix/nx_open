#include "system_internet_access_watcher.h"

#include <algorithm>

#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_changes_listener.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::client::desktop {

SystemInternetAccessWatcher::SystemInternetAccessWatcher(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
    const auto messageProcessor = commonModule()->messageProcessor();
    NX_ASSERT(messageProcessor);

    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            m_processRuntimeUpdates = true;
            updateState();
        });

    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed, this,
        [this]()
        {
            m_processRuntimeUpdates = false;
            setHasInternetAccess(false);
        });

    auto serverChangesListener = new QnResourceChangesListener(this);
    serverChangesListener->connectToResources<QnMediaServerResource>(
        &QnMediaServerResource::serverFlagsChanged,
        this, &SystemInternetAccessWatcher::updateState);

    serverChangesListener->connectToResources<QnMediaServerResource>(
        &QnMediaServerResource::statusChanged,
        this, &SystemInternetAccessWatcher::updateState);

    const auto handleServerAddedOrRemoved =
        [this](const QnResourcePtr& resource)
        {
            if (m_processRuntimeUpdates && resource->hasFlags(Qn::server))
                updateState();
        };

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, handleServerAddedOrRemoved);

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, handleServerAddedOrRemoved);

    updateState();

    m_processRuntimeUpdates = !commonModule()->remoteGUID().isNull();
}

SystemInternetAccessWatcher::~SystemInternetAccessWatcher()
{
}

bool SystemInternetAccessWatcher::systemHasInternetAccess() const
{
    return m_hasInternetAccess;
}

void SystemInternetAccessWatcher::updateState()
{
    const auto servers = resourcePool()->getAllServers(Qn::Online);
    setHasInternetAccess(std::any_of(servers.cbegin(), servers.cend(),
        &SystemInternetAccessWatcher::serverHasInternetAccess));
}

void SystemInternetAccessWatcher::setHasInternetAccess(bool value)
{
    if (m_hasInternetAccess == value)
        return;

    m_hasInternetAccess = value;
    emit internetAccessChanged(m_hasInternetAccess, {});
}

bool SystemInternetAccessWatcher::serverHasInternetAccess(const QnMediaServerResourcePtr& server)
{
    return server->getServerFlags().testFlag(vms::api::ServerFlag::SF_HasPublicIP);
}

} // namespace nx::vms::client::desktop
