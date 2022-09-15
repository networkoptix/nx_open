// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module_aware.h"

#include <nx/vms/common/system_context.h>

#include "common_module.h"

using namespace nx::vms::common;

QnCommonModuleAware::QnCommonModuleAware(QnCommonModule* commonModule)
{
    init(commonModule);
}

QnCommonModuleAware::QnCommonModuleAware(QObject* parent, bool lazyInitialization)
{
    if (!lazyInitialization)
        init(parent);
}

void QnCommonModuleAware::init(QObject* parent)
{
    for (;parent; parent = parent->parent())
    {
        QnCommonModuleAware* moduleAware = dynamic_cast<QnCommonModuleAware*>(parent);
        if (moduleAware != nullptr)
        {
            m_commonModule = moduleAware->commonModule();
            NX_ASSERT(m_commonModule, "Invalid context");
            break;
        }

        m_commonModule = dynamic_cast<QnCommonModule*>(parent);
        if (m_commonModule)
            break;
    }

    m_initialized = (m_commonModule != nullptr);
}

void QnCommonModuleAware::initializeContext(QObject *parent)
{
    init(parent);
}

void QnCommonModuleAware::initializeContext(QnCommonModule* commonModule)
{
    init(commonModule);
}

void QnCommonModuleAware::deinitializeContext()
{
    m_commonModule = nullptr;
    m_initialized = false;
}

QnCommonModule* QnCommonModuleAware::commonModule() const
{
    return m_commonModule;
}

nx::vms::common::SystemContext* QnCommonModuleAware::systemContext() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext() : nullptr;
}

QnUuid QnCommonModuleAware::peerId() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->peerId() : QnUuid();
}

QnUuid QnCommonModuleAware::sessionId() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->sessionId() : QnUuid();
}

QnLicensePool* QnCommonModuleAware::licensePool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->licensePool() : nullptr;
}

QnRuntimeInfoManager* QnCommonModuleAware::runtimeInfoManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->runtimeInfoManager() : nullptr;
}

QnResourcePool* QnCommonModuleAware::resourcePool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->resourcePool() : nullptr;
}

QnResourceAccessManager* QnCommonModuleAware::resourceAccessManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->resourceAccessManager() : nullptr;
}

nx::core::access::ResourceAccessProvider* QnCommonModuleAware::resourceAccessProvider() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->resourceAccessProvider() : nullptr;
}

QnResourceAccessSubjectsCache* QnCommonModuleAware::resourceAccessSubjectsCache() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule
        ? m_commonModule->systemContext()->resourceAccessSubjectsCache()
        : nullptr;
}

QnGlobalPermissionsManager* QnCommonModuleAware::globalPermissionsManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->globalPermissionsManager() : nullptr;
}

QnSharedResourcesManager* QnCommonModuleAware::sharedResourcesManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->sharedResourcesManager() : nullptr;
}

QnUserRolesManager* QnCommonModuleAware::userRolesManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->userRolesManager() : nullptr;
}

QnCameraHistoryPool* QnCommonModuleAware::cameraHistoryPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->cameraHistoryPool() : nullptr;
}

QnResourcePropertyDictionary* QnCommonModuleAware::resourcePropertyDictionary() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule
        ? m_commonModule->systemContext()->resourcePropertyDictionary()
        : nullptr;
}

QnResourceStatusDictionary* QnCommonModuleAware::statusDictionary() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->resourceStatusDictionary() : nullptr;
}

SystemSettings* QnCommonModuleAware::globalSettings() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->globalSettings() : nullptr;
}

QnLayoutTourManager* QnCommonModuleAware::layoutTourManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->layoutTourManager() : nullptr;
}

nx::vms::event::RuleManager* QnCommonModuleAware::eventRuleManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->eventRuleManager() : nullptr;
}

QnAuditManager* QnCommonModuleAware::auditManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->auditManager() : nullptr;
}

QnResourceDataPool* QnCommonModuleAware::dataPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->resourceDataPool() : nullptr;
}

std::shared_ptr<ec2::AbstractECConnection> QnCommonModuleAware::ec2Connection() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->ec2Connection() : nullptr;
}

QnCommonMessageProcessor* QnCommonModuleAware::messageProcessor() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->systemContext()->messageProcessor() : nullptr;
}
