#pragma once

class QnCommonModule;
class QnGlobalPermissionsManager;
class QnLicensePool;
class QnResourcePool;
class QnResourceAccessManager;
class QnResourceAccessProvider;
class QnResourceAccessSubjectsCache;
class QnRuntimeInfoManager;
class QnSharedResourcesManager;
class QnUserRolesManager;
class QnCameraHistoryPool;
class QnResourcePropertyDictionary;
class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;
class QnResourceStatusDictionary;

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
    QnResourceAccessProvider* resourceAccessProvider() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
    QnSharedResourcesManager* sharedResourcesManager() const;
    QnUserRolesManager* userRolesManager() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    QnResourcePropertyDictionary* propertyDictionary() const;
    QnCameraUserAttributePool* cameraUserAttributesPool() const;
    QnMediaServerUserAttributesPool* mediaServerUserAttributesPool() const;
    QnResourceStatusDictionary* statusDictionary() const;

private:
    void init(QObject *parent);

private:
    QnCommonModule* m_commonModule;
};
