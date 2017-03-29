#pragma once

class QnCommonModule;
class QnLicensePool;
class QnRuntimeInfoManager;
class QnResourcePool;
class QnResourceAccessSubjectsCache;
class QnGlobalPermissionsManager;

class QnCommonModuleAware
{
public:
    QnCommonModuleAware(QnCommonModule* commonModule);
    QnCommonModuleAware(QObject* parent);

    QnCommonModule* commonModule() const;

    QnLicensePool* licensePool() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
    QnResourcePool* resourcePool() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
private:
    void init(QObject *parent);

private:
    QnCommonModule* m_commonModule;
};
