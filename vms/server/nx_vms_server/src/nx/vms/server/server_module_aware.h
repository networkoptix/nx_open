#pragma once

#include <nx/utils/uuid.h>

class QnMediaServerModule;
class QnCommonModule;
class QnResourcePool;
class QnResourcePropertyDictionary;
class QnCameraHistoryPool;
class QnServerDb;
class QnResourceAccessSubjectsCache;
class QnUserRolesManager;
class QnAudioStreamerPool;
class QnStorageDbPool;
class QnRecordingManager;
class QnVideoCameraPool;
class QnGlobalSettings;
class QnRuntimeInfoManager;
class QnCameraUserAttributePool;
class QnAuditManager;

namespace ec2 { class AbstractECConnection; }
namespace nx::vms::event { class RuleManager; }
namespace nx::vms::server::event { class EventMessageBus; }

namespace nx {
namespace vms::server {

class Settings;
class RootFileSystem;

class ServerModuleAware
{
public:
    ServerModuleAware(QObject* parent);
    ServerModuleAware(QnMediaServerModule* serverModule);

    QnMediaServerModule* serverModule() const;

    QnResourcePool* resourcePool() const;
    QnResourcePropertyDictionary* propertyDictionary() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    const nx::vms::server::Settings& settings() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    QnServerDb* serverDb() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnUserRolesManager* userRolesManager() const;
    QnAudioStreamerPool* audioStreamPool() const;
    QnStorageDbPool* storageDbPool() const;
    QnRecordingManager* recordingManager() const;
    nx::vms::server::event::EventMessageBus* eventMessageBus() const;
    QnVideoCameraPool* videoCameraPool() const;
    RootFileSystem* rootFileSystem() const;
    QnUuid moduleGUID() const;
    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;
    QnGlobalSettings* globalSettings() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
    QnCameraUserAttributePool* cameraUserAttributesPool() const;
    QnAuditManager* auditManager() const;
private:
    QnMediaServerModule* m_serverModule;
};

} // namespace vms::server
} // namespace nx
