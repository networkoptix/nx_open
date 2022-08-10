// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module.h"

#include <QtCore/QPointer>

#include <nx/metrics/metrics_storage.h>
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

    m_metrics = std::make_shared<nx::metrics::Storage>(); //< Depends on nothing.
}

SystemContext* QnCommonModule::systemContext() const
{
    return d->systemContext.data();
}

QnCommonModule::~QnCommonModule()
{
    /* Here all singletons will be destroyed, so we guarantee all socket work will stop. */
    clear();
}

void QnCommonModule::setSystemIdentityTime(qint64 value, const QnUuid& sender)
{
    NX_INFO(this, "System identity time has changed from %1 to %2", m_systemIdentityTime, value);
    m_systemIdentityTime = value;
    emit systemIdentityTimeChanged(value, sender);
}

qint64 QnCommonModule::systemIdentityTime() const
{
    return m_systemIdentityTime;
}

nx::metrics::Storage* QnCommonModule::metrics() const
{
    return m_metrics.get();
}

std::weak_ptr<nx::metrics::Storage> QnCommonModule::metricsWeakRef() const
{
    return std::weak_ptr<nx::metrics::Storage>(m_metrics);
}

void QnCommonModule::setAuditManager(QnAuditManager* auditManager)
{
    m_auditManager = auditManager;
}

QnAuditManager* QnCommonModule::auditManager() const
{
    return m_auditManager;
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

nx::core::access::ResourceAccessProvider* QnCommonModule::resourceAccessProvider() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceAccessProvider()
        : nullptr;
}

QnResourceAccessSubjectsCache* QnCommonModule::resourceAccessSubjectsCache() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceAccessSubjectsCache()
        : nullptr;
}

QnGlobalPermissionsManager* QnCommonModule::globalPermissionsManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->globalPermissionsManager()
        : nullptr;
}

QnSharedResourcesManager* QnCommonModule::sharedResourcesManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->sharedResourcesManager()
        : nullptr;
}

QnUserRolesManager* QnCommonModule::userRolesManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->userRolesManager()
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

QnLayoutTourManager* QnCommonModule::layoutTourManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->layoutTourManager()
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

std::shared_ptr<ec2::AbstractECConnection> QnCommonModule::ec2Connection() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->ec2Connection()
        : nullptr;
}

QnCommonMessageProcessor* QnCommonModule::messageProcessor() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->messageProcessor()
        : nullptr;
}

//-------------------------------------------------------------------------------------------------
