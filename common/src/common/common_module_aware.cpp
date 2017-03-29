#include "common_module_aware.h"
#include "common_module.h"

//TODO: #GDM #3.1 think how to get rid of these includes
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/resource_pool.h>

#include <licensing/license.h>

QnCommonModuleAware::QnCommonModuleAware(QnCommonModule* commonModule)
{
    init(commonModule);
}

QnCommonModuleAware::QnCommonModuleAware(QObject* parent)
{
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
            NX_ASSERT(m_commonModule, Q_FUNC_INFO, "Invalid context");
            break;
        }

        m_commonModule = dynamic_cast<QnCommonModule*>(parent);
        if (m_commonModule)
            break;
    }
}

QnCommonModule* QnCommonModuleAware::commonModule() const
{
    return m_commonModule;
}

QnLicensePool* QnCommonModuleAware::licensePool() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->instance<QnLicensePool>() : nullptr;
}

QnRuntimeInfoManager* QnCommonModuleAware::runtimeInfoManager() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->runtimeInfoManager() : nullptr;
}

QnResourcePool* QnCommonModuleAware::resourcePool() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->resourcePool() : nullptr;
}

QnResourceAccessManager* QnCommonModuleAware::resourceAccessManager() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->resourceAccessManager() : nullptr;
}

QnResourceAccessSubjectsCache* QnCommonModuleAware::resourceAccessSubjectsCache() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->instance<QnResourceAccessSubjectsCache>() : nullptr;
}

QnGlobalPermissionsManager* QnCommonModuleAware::globalPermissionsManager() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->instance<QnGlobalPermissionsManager>() : nullptr;
}

QnSharedResourcesManager* QnCommonModuleAware::sharedResourcesManager() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->instance<QnSharedResourcesManager>() : nullptr;
}

QnUserRolesManager* QnCommonModuleAware::userRolesManager() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->instance<QnUserRolesManager>() : nullptr;
}

QnCameraHistoryPool* QnCommonModuleAware::cameraHistoryPool() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->cameraHistoryPool() : nullptr;
}
