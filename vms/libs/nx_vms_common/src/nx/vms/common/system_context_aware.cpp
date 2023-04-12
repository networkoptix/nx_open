// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

#include "system_context.h"

namespace nx::vms::common {

struct SystemContextAware::Private
{
    mutable QPointer<SystemContext> systemContext;
    mutable std::unique_ptr<SystemContextInitializer> delayedInitializer;
};

SystemContextAware::SystemContextAware(SystemContext* systemContext):
    m_context(systemContext),
    d(new Private())
{
    NX_CRITICAL(systemContext);
    d->systemContext = systemContext;
}

SystemContextAware::SystemContextAware(std::unique_ptr<SystemContextInitializer> initializer):
    d(new Private())
{
    d->delayedInitializer = std::move(initializer);
}

SystemContextAware::~SystemContextAware()
{
    // Make sure context was not ever initialized or still exists.
    NX_ASSERT(d->delayedInitializer || d->systemContext);
}

SystemContext* SystemContextAware::systemContext() const
{
    if (!d->systemContext)
    {
        if (NX_ASSERT(d->delayedInitializer))
        {
            d->systemContext = d->delayedInitializer->systemContext();
            d->delayedInitializer.reset();
            NX_ASSERT(d->systemContext, "Initialization failed");
        }
    }
    return d->systemContext.data();
}

std::shared_ptr<ec2::AbstractECConnection> SystemContextAware::messageBusConnection() const
{
    return systemContext()->messageBusConnection();
}

QnResourceAccessManager* SystemContextAware::resourceAccessManager() const
{
    return systemContext()->resourceAccessManager();
}

QnGlobalPermissionsManager* SystemContextAware::globalPermissionsManager() const
{
    return systemContext()->globalPermissionsManager();
}

QnResourcePool* SystemContextAware::resourcePool() const
{
    return systemContext()->resourcePool();
}

QnCommonMessageProcessor* SystemContextAware::messageProcessor() const
{
    return systemContext()->messageProcessor();
}

QnCameraHistoryPool* SystemContextAware::cameraHistoryPool() const
{
    return systemContext()->cameraHistoryPool();
}

SystemSettings* SystemContextAware::systemSettings() const
{
    return systemContext()->globalSettings();
}

ShowreelManager* SystemContextAware::showreelManager() const
{
    return systemContext()->showreelManager();
}

LookupListManager* SystemContextAware::lookupListManager() const
{
    return systemContext()->lookupListManager();
}

QnRuntimeInfoManager* SystemContextAware::runtimeInfoManager() const
{
    return systemContext()->runtimeInfoManager();
}

UserGroupManager* SystemContextAware::userGroupManager() const
{
    return systemContext()->userGroupManager();
}

} // namespace nx::vms::common
