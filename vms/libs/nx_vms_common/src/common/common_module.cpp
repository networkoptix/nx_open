// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module.h"

#include <QtCore/QPointer>

#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>

using namespace nx::vms::common;

//-------------------------------------------------------------------------------------------------
// QnCommonModule::Private

struct QnCommonModule::Private
{
    QPointer<SystemContext> systemContext;
};

//-------------------------------------------------------------------------------------------------
// QnCommonModule

QnCommonModule::QnCommonModule(SystemContext* systemContext):
    d(new Private)
{
    d->systemContext = systemContext;
}

SystemContext* QnCommonModule::systemContext() const
{
    return d->systemContext.data();
}

QnCommonModule::~QnCommonModule()
{
}

//-------------------------------------------------------------------------------------------------
// Temporary methods for the migration simplification.
AbstractCertificateVerifier* QnCommonModule::certificateVerifier() const
{
    return d->systemContext->certificateVerifier();
}

QnUuid QnCommonModule::peerId() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->peerId()
        : QnUuid();
}

QnUuid QnCommonModule::sessionId() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->sessionId()
        : QnUuid();
}

QnLicensePool* QnCommonModule::licensePool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->licensePool()
        : nullptr;
}

QnRuntimeInfoManager* QnCommonModule::runtimeInfoManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->runtimeInfoManager()
        : nullptr;
}

QnResourcePool* QnCommonModule::resourcePool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourcePool()
        : nullptr;
}

QnResourceAccessManager* QnCommonModule::resourceAccessManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceAccessManager()
        : nullptr;
}

QnCameraHistoryPool* QnCommonModule::cameraHistoryPool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->cameraHistoryPool()
        : nullptr;
}

QnResourcePropertyDictionary* QnCommonModule::resourcePropertyDictionary() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourcePropertyDictionary()
        : nullptr;
}

QnResourceStatusDictionary* QnCommonModule::resourceStatusDictionary() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceStatusDictionary()
        : nullptr;
}

SystemSettings* QnCommonModule::globalSettings() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->globalSettings()
        : nullptr;
}

nx::vms::event::RuleManager* QnCommonModule::eventRuleManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->eventRuleManager()
        : nullptr;
}

QnResourceDataPool* QnCommonModule::resourceDataPool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceDataPool()
        : nullptr;
}

std::shared_ptr<ec2::AbstractECConnection> QnCommonModule::messageBusConnection() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->messageBusConnection()
        : nullptr;
}

QnCommonMessageProcessor* QnCommonModule::messageProcessor() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->messageProcessor()
        : nullptr;
}

nx::metrics::Storage* QnCommonModule::metrics() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->metrics().get()
        : nullptr;
}

std::weak_ptr<nx::metrics::Storage> QnCommonModule::metricsWeakRef() const
{
    return NX_ASSERT(d->systemContext)
        ? std::weak_ptr<nx::metrics::Storage>(d->systemContext->metrics())
        : std::weak_ptr<nx::metrics::Storage>{};
}

//-------------------------------------------------------------------------------------------------
