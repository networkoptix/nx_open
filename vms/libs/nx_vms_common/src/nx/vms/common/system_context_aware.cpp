// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <QtCore/QObject>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>

#include "system_context.h"

namespace nx::vms::common {

struct SystemContextAware::Private
{
    mutable QPointer<SystemContext> systemContext;
};

SystemContextAware::SystemContextAware(SystemContext* systemContext):
    d(new Private{.systemContext = systemContext})
{
    NX_CRITICAL(systemContext);
    d->systemContext = systemContext;

    if (auto qobject = dynamic_cast<const QObject*>(this))
    {
        QObject::connect(systemContext, &QObject::destroyed, qobject,
            []()
            {
                NX_ASSERT(false,
                    "Context-aware object must be destroyed before the corresponding context is.");
            });
    }
}

SystemContextAware::~SystemContextAware()
{
    // Make sure context was not ever initialized or still exists.
    NX_ASSERT(d->systemContext,
        "Context-aware object must be destroyed before the corresponding context is.");
}

SystemContext* SystemContextAware::systemContext() const
{
    return d->systemContext.data();
}

nx::Uuid SystemContextAware::peerId() const
{
    return systemContext()->peerId();
}

std::shared_ptr<ec2::AbstractECConnection> SystemContextAware::messageBusConnection() const
{
    return systemContext()->messageBusConnection();
}

QnResourceAccessManager* SystemContextAware::resourceAccessManager() const
{
    return systemContext()->resourceAccessManager();
}

QnResourcePool* SystemContextAware::resourcePool() const
{
    return systemContext()->resourcePool();
}

QnResourcePropertyDictionary* SystemContextAware::resourcePropertyDictionary() const
{
    return systemContext()->resourcePropertyDictionary();
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

nx::core::access::AccessRightsManager* SystemContextAware::accessRightsManager() const
{
    return systemContext()->accessRightsManager();
}

QnLicensePool* SystemContextAware::licensePool() const
{
    return systemContext()->licensePool();
}

nx::vms::event::RuleManager* SystemContextAware::eventRuleManager() const
{
    return systemContext()->eventRuleManager();
}

QnResourceStatusDictionary* SystemContextAware::resourceStatusDictionary() const
{
    return systemContext()->resourceStatusDictionary();
}

QnServerAdditionalAddressesDictionary*
    SystemContextAware::serverAdditionalAddressesDictionary() const
{
    return systemContext()->serverAdditionalAddressesDictionary();
}

saas::ServiceManager* SystemContextAware::saasServiceManager() const
{
    return systemContext()->saasServiceManager();
}

std::shared_ptr<nx::analytics::taxonomy::AbstractState>
    SystemContextAware::analyticsTaxonomyState() const
{
    return systemContext()->analyticsTaxonomyState();
}

AbstractCertificateVerifier* SystemContextAware::verifier() const
{
    return systemContext()->certificateVerifier();
}

} // namespace nx::vms::common
