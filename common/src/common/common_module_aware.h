#pragma once

#include <QtCore/QPointer>

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
class QnGlobalSettings;
class QnLayoutTourManager;

namespace nx { namespace vms { namespace event { class RuleManager; }}}

class QnCommonModuleAware
{
public:
    QnCommonModuleAware(QnCommonModule* commonModule);
    QnCommonModuleAware(QObject* parent, bool lazyInitialization = false);

    /** Delayed initialization call. */
    void initializeContext(QObject *parent);

    /** Delayed initialization call. */
    void initializeContext(QnCommonModule* commonModule);

    void deinitializeContext();

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
    QnGlobalSettings* globalSettings() const;
    QnLayoutTourManager* layoutTourManager() const;
    nx::vms::event::RuleManager* eventRuleManager() const;

private:
    void init(QObject *parent);

private:
    QPointer<QnCommonModule> m_commonModule;
    bool m_initialized {false};
};
