#pragma once

#include <QtCore/QObject>
#include <QtCore/QDir>

#include <common/common_module.h>
#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>

#include "settings.h"

class QnCommonModule;
class StreamingChunkCache;
class QnStorageManager;
class QnStaticCommonModule;
class QnStorageDbPool;
class MSSettings;
class PluginManager;
class CommonPluginContainer;
class QThread;
class AbstractArchiveIntegrityWatcher;
class QnDataProviderFactory;
class QnResourceCommandProcessor;
class QnResourcePool;
class QnResourcePropertyDictionary;
class QnCameraHistoryPool;
class QnServerUpdateTool;
class QnMotionHelper;
class QnServerDb;
class QnAuditManager;
class QnMServerAuditManager;
class QnAudioStreamerPool;
class QnStorageDbPool;
class QnRecordingManager;
class HostSystemPasswordSynchronizer;
class CameraDriverRestrictionList;
class QnVideoCameraPool;
class QnPtzControllerPool;
class QnFileDeletor;
class QnResourceAccessManager;
class QnResourceDiscoveryManager;
class QnAuditManager;

namespace nx::vms::common::p2p::downloader { class Downloader; }

namespace nx {
namespace mediaserver { class CmdLineArguments; }
namespace mediaserver::event {
class ExtendedRuleProcessor;
class EventConnector;
class EventMessageBus;
} // namespace mediaserver::event

namespace analytics {
namespace storage {
class AbstractEventsStorage;
} // namespace storage
} // namespace analytics

namespace time_sync {
class TimeSyncManager;
} // namespace time_sync

namespace mediaserver {

class UnusedWallpapersWatcher;
class LicenseWatcher;
class RootFileSystem;
class Settings;
class ServerTimeSyncManager;
class ServerUpdateManager;

namespace metadata {

class ManagerPool;
class EventRuleWatcher;

} // namespace metadata

namespace resource {

class SharedContextPool;

} // namespace resource

namespace camera {

class ErrorProcessor;

} // namespace camera

} // namespace mediaserver

class CommonUpdateManager;

} // namespace nx

class QnMediaServerModule : public QObject, public QnInstanceStorage
{
    Q_OBJECT;

public:
    QnMediaServerModule(
        const nx::mediaserver::CmdLineArguments* arguments = nullptr, QObject* parent = nullptr);
    virtual ~QnMediaServerModule() override;


    void stop();
    using QnInstanceStorage::instance;

    StreamingChunkCache* streamingChunkCache() const;
    QnCommonModule* commonModule() const;

    QSettings* roSettings() const;
    QSettings* runTimeSettings() const;

    std::chrono::milliseconds lastRunningTime() const;
    std::chrono::milliseconds lastRunningTimeBeforeRestart() const;
    void setLastRunningTime(std::chrono::milliseconds value) const;

    const nx::mediaserver::Settings& settings() const { return m_settings->settings(); }
    nx::mediaserver::Settings* mutableSettings() { return m_settings->mutableSettings(); }

    void syncRoSettings() const;
    nx::mediaserver::UnusedWallpapersWatcher* unusedWallpapersWatcher() const;
    nx::mediaserver::LicenseWatcher* licenseWatcher() const;
    nx::mediaserver::event::EventMessageBus* eventMessageBus() const;
    PluginManager* pluginManager() const;
    nx::mediaserver::metadata::ManagerPool* metadataManagerPool() const;
    nx::mediaserver::metadata::EventRuleWatcher* metadataRuleWatcher() const;
    nx::mediaserver::resource::SharedContextPool* sharedContextPool() const;
    AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher() const;
    nx::analytics::storage::AbstractEventsStorage* analyticsEventsStorage() const;
    nx::CommonUpdateManager* updateManager() const;
    QnDataProviderFactory* dataProviderFactory() const;
    QnResourceCommandProcessor* resourceCommandProcessor() const;

    QnResourcePool* resourcePool() const;
    QnResourcePropertyDictionary* propertyDictionary() const;
    QnCameraHistoryPool* cameraHistoryPool() const;

    nx::mediaserver::RootFileSystem* rootFileSystem() const;

    QnStorageManager* normalStorageManager() const;
    QnStorageManager* backupStorageManager() const;
    nx::mediaserver::event::EventConnector* eventConnector() const;
    nx::mediaserver::event::ExtendedRuleProcessor* eventRuleProcessor() const;
    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;
    QnGlobalSettings* globalSettings() const;
    QnServerUpdateTool* serverUpdateTool() const;
    QnMotionHelper* motionHelper() const;
    nx::vms::common::p2p::downloader::Downloader* p2pDownloader() const;
    QnServerDb* serverDb() const;
    QnAudioStreamerPool* audioStreamPool() const;
    QnStorageDbPool* storageDbPool() const;
    QnRecordingManager* recordingManager() const;
    HostSystemPasswordSynchronizer* hostSystemPasswordSynchronizer() const;
    QnVideoCameraPool* videoCameraPool() const;
    void stopStorages();
    QnPtzControllerPool* ptzControllerPool() const;
    QnFileDeletor* fileDeletor() const;

    QnResourceAccessManager* resourceAccessManager() const;
    QnAuditManager* auditManager() const;
    QnResourceDiscoveryManager* resourceDiscoveryManager() const;
    nx::mediaserver::camera::ErrorProcessor* cameraErrorProcessor() const;

private:
    void registerResourceDataProviders();
    QDir downloadsDirectory() const;
    void stopLongRunnables();

    QnCommonModule* m_commonModule;
    MSSettings* m_settings;
    QnStorageDbPool* m_storageDbPool = nullptr;
    StreamingChunkCache* m_streamingChunkCache;

    struct UniquePtrContext
    {
        std::shared_ptr<QnStorageManager> normalStorageManager;
        std::shared_ptr<QnStorageManager> backupStorageManager;
    };
    std::unique_ptr<UniquePtrContext> m_context;

    CommonPluginContainer m_pluginContainer;
    PluginManager* m_pluginManager = nullptr;
    nx::mediaserver::UnusedWallpapersWatcher* m_unusedWallpapersWatcher = nullptr;
    nx::mediaserver::LicenseWatcher* m_licenseWatcher = nullptr;
    nx::mediaserver::event::EventMessageBus* m_eventMessageBus = nullptr;
    nx::mediaserver::metadata::ManagerPool* m_metadataManagerPool = nullptr;

    nx::mediaserver::event::ExtendedRuleProcessor* m_eventRuleProcessor = nullptr;
    nx::mediaserver::event::EventConnector* m_eventConnector = nullptr;
    nx::mediaserver::metadata::EventRuleWatcher* m_metadataRuleWatcher = nullptr;
    nx::mediaserver::resource::SharedContextPool* m_sharedContextPool = nullptr;
    AbstractArchiveIntegrityWatcher* m_archiveIntegrityWatcher;
    mutable boost::optional<std::chrono::milliseconds> m_lastRunningTimeBeforeRestart;
    std::unique_ptr<nx::analytics::storage::AbstractEventsStorage> m_analyticsEventsStorage;
    std::unique_ptr<nx::mediaserver::RootFileSystem> m_rootFileSystem;
    nx::CommonUpdateManager* m_updateManager = nullptr;
    QnServerUpdateTool* m_serverUpdateTool = nullptr;
    QnDataProviderFactory* m_resourceDataProviderFactory = nullptr;
    QScopedPointer<QnResourceCommandProcessor> m_resourceCommandProcessor;
    QnMotionHelper* m_motionHelper = nullptr;
    nx::vms::common::p2p::downloader::Downloader* m_p2pDownloader = nullptr;
    QnAudioStreamerPool* m_audioStreamPool = nullptr;
    QnServerDb* m_serverDb = nullptr;
    QnVideoCameraPool* m_videoCameraPool = nullptr;
    QnRecordingManager* m_recordingManager = nullptr;
    HostSystemPasswordSynchronizer* m_hostSystemPasswordSynchronizer = nullptr;
    QnPtzControllerPool* m_ptzControllerPool = nullptr;
    QnFileDeletor* m_fileDeletor = nullptr;
    nx::mediaserver::camera::ErrorProcessor* m_cameraErrorProcessor;
};
