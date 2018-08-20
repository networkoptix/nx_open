#pragma once

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

namespace nx::vms::event { class RuleManager; }
namespace nx::mediaserver::event { class EventMessageBus; }

namespace nx {
namespace mediaserver {

class Settings;
class RootFileSystem;

class ServerModuleAware
{
public:
    ServerModuleAware(QObject* parent);
    ServerModuleAware(QnMediaServerModule* serverModule);

    QnMediaServerModule* serverModule() const;

    QnCommonModule* commonModule() const;

    QnResourcePool* resourcePool() const;
    QnResourcePropertyDictionary* propertyDictionary() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    const nx::mediaserver::Settings& settings() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    QnServerDb* serverDb() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnUserRolesManager* userRolesManager() const;
    QnAudioStreamerPool* audioStreamPool() const;
    QnStorageDbPool* storageDbPool() const;
    QnRecordingManager* recordingManager() const;
    nx::mediaserver::event::EventMessageBus* eventMessageBus() const;
    QnVideoCameraPool* videoCameraPool() const;
    RootFileSystem* rootFileSystem() const;

private:
    QnMediaServerModule* m_serverModule;
};

} // namespace mediaserver
} // namespace nx
