// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

class QnCommonModule;
class QnGlobalPermissionsManager;
class QnLicensePool;
class QnResourcePool;
class QnResourceAccessManager;
namespace nx::core::access { class ResourceAccessProvider; }
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
class QnAuditManager;
class QnResourceDataPool;

namespace nx { namespace vms { namespace event { class RuleManager; }}}
namespace nx::vms::rules { class Engine; }

class NX_VMS_COMMON_API QnCommonModuleAware
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
    nx::core::access::ResourceAccessProvider* resourceAccessProvider() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
    QnSharedResourcesManager* sharedResourcesManager() const;
    QnUserRolesManager* userRolesManager() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;
    QnCameraUserAttributePool* cameraUserAttributesPool() const;
    QnMediaServerUserAttributesPool* mediaServerUserAttributesPool() const;
    QnResourceStatusDictionary* statusDictionary() const;
    QnGlobalSettings* globalSettings() const;
    QnLayoutTourManager* layoutTourManager() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    nx::vms::rules::Engine* vmsRulesEngine() const;
    QnAuditManager* auditManager() const;
    QnResourceDataPool* dataPool() const;
private:
    void init(QObject *parent);

private:
    QPointer<QnCommonModule> m_commonModule;
    bool m_initialized {false};
};
