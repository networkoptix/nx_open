#pragma once

class QnCommonModule;
class QnGlobalPermissionsManager;
class QnLicensePool;
class QnResourcePool;
class QnResourceAccessManager;
class QnResourceAccessSubjectsCache;
class QnRuntimeInfoManager;
class QnSharedResourcesManager;
class QnUserRolesManager;

class QnCommonModuleAware
{
public:
    QnCommonModuleAware(QnCommonModule* commonModule);
    QnCommonModuleAware(QObject* parent);

    QnCommonModule* commonModule() const;

    QnLicensePool* licensePool() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
    QnResourcePool* resourcePool() const;
    QnResourceAccessManager* resourceAccessManager() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
    QnSharedResourcesManager* sharedResourcesManager() const;
    QnUserRolesManager* userRolesManager() const;

private:
    void init(QObject *parent);

private:
    QnCommonModule* m_commonModule;
};
