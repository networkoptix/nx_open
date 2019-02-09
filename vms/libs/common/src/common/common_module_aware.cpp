#include "common_module_aware.h"
#include "common_module.h"

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

QnLicensePool* QnCommonModuleAware::licensePool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->licensePool() : nullptr;
}

QnRuntimeInfoManager* QnCommonModuleAware::runtimeInfoManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->runtimeInfoManager() : nullptr;
}

QnResourcePool* QnCommonModuleAware::resourcePool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourcePool() : nullptr;
}

QnResourceAccessManager* QnCommonModuleAware::resourceAccessManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourceAccessManager() : nullptr;
}

QnResourceAccessProvider* QnCommonModuleAware::resourceAccessProvider() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourceAccessProvider() : nullptr;
}

QnResourceAccessSubjectsCache* QnCommonModuleAware::resourceAccessSubjectsCache() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourceAccessSubjectsCache() : nullptr;
}

QnGlobalPermissionsManager* QnCommonModuleAware::globalPermissionsManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->globalPermissionsManager() : nullptr;
}

QnSharedResourcesManager* QnCommonModuleAware::sharedResourcesManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->sharedResourcesManager() : nullptr;
}

QnUserRolesManager* QnCommonModuleAware::userRolesManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->userRolesManager() : nullptr;
}

QnCameraHistoryPool* QnCommonModuleAware::cameraHistoryPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->cameraHistoryPool() : nullptr;
}

QnResourcePropertyDictionary* QnCommonModuleAware::resourcePropertyDictionary() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourcePropertyDictionary() : nullptr;
}

QnCameraUserAttributePool* QnCommonModuleAware::cameraUserAttributesPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->cameraUserAttributesPool() : nullptr;
}

QnMediaServerUserAttributesPool* QnCommonModuleAware::mediaServerUserAttributesPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->mediaServerUserAttributesPool() : nullptr;
}

QnResourceStatusDictionary* QnCommonModuleAware::statusDictionary() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourceStatusDictionary() : nullptr;
}

QnGlobalSettings* QnCommonModuleAware::globalSettings() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->globalSettings() : nullptr;
}

QnLayoutTourManager* QnCommonModuleAware::layoutTourManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->layoutTourManager() : nullptr;
}

nx::vms::event::RuleManager* QnCommonModuleAware::eventRuleManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->eventRuleManager() : nullptr;
}

QnAuditManager* QnCommonModuleAware::auditManager() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->auditManager() : nullptr;
}

nx::network::http::ClientPool* QnCommonModuleAware::httpClientPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->httpClientPool() : nullptr;
}

QnResourceDataPool* QnCommonModuleAware::dataPool() const
{
    NX_ASSERT(m_initialized);
    return m_commonModule ? m_commonModule->resourceDataPool() : nullptr;
}
