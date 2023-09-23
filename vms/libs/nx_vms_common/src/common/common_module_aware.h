// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/utils/uuid.h>

class QnCommonModule;
class QnGlobalPermissionsManager;
class QnLicensePool;
class QnResourcePool;
class QnResourceAccessManager;
class QnRuntimeInfoManager;
class QnCameraHistoryPool;
class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnResourceDataPool;
class QnCommonMessageProcessor;

namespace nx::vms::common {

class SystemSettings;
class SystemContext;
class UserGroupManager;

} // namespace nx::vms::common

namespace nx::core::access { class ResourceAccessSubjectHierarchy; }
namespace nx::vms::event { class RuleManager; }
namespace nx::vms::rules { class Engine; }
namespace ec2 { class AbstractECConnection; }

/**
 * Deprecated class to get access to the System Context. Use SystemContextAware instead.
 */
class NX_VMS_COMMON_API QnCommonModuleAware
{
public:
    QnCommonModuleAware(QnCommonModule* commonModule);

    QnCommonModule* commonModule() const;

    nx::vms::common::SystemContext* systemContext() const;

    QnUuid peerId() const;
    QnUuid sessionId() const;

    QnLicensePool* licensePool() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
    QnResourcePool* resourcePool() const;
    QnResourceAccessManager* resourceAccessManager() const;
    nx::core::access::ResourceAccessSubjectHierarchy* accessSubjectHierarchy() const;
    nx::vms::common::UserGroupManager* userGroupManager() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;
    QnResourceStatusDictionary* statusDictionary() const;
    nx::vms::common::SystemSettings* globalSettings() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    QnResourceDataPool* dataPool() const;

    std::shared_ptr<ec2::AbstractECConnection> messageBusConnection() const;
    QnCommonMessageProcessor* messageProcessor() const;

private:
    QPointer<QnCommonModule> m_commonModule;
};
