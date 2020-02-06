#pragma once

#include <QtCore/QObject>
#include <QtCore/QDir>

#include <common/common_module.h>
#include <utils/common/instance_storage.h>

#include "settings.h"
#include <plugins/resource/mdns/mdns_listener.h>
#include <nx/network/upnp/upnp_device_searcher.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/analytics/manager.h>

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
class QnMediaServerResourceSearchers;
class QnPlatformAbstraction;
class QnServerConnector;
class QnResourceStatusWatcher;
class StreamingChunkTranscoder;
class IFrameSearchHelper;

namespace nx::vms::common::p2p::downloader { class Downloader; }
namespace nx::vms::server::hls { class SessionPool; }
namespace nx::vms::server { class CmdLineArguments; }
namespace nx::vms::server::analytics {
    class SdkObjectFactory;
    class AbstractIFrameSearchHelper;
    class IFrameSearchHelper;
} // namespace nx::vms::server::analytics

namespace nx::vms::server::network { class MulticastAddressRegistry;  }

namespace nx::vms::server::event {
    class ExtendedRuleProcessor;
    class EventConnector;
    class EventMessageBus;
    class ServerRuntimeEventManager;
} // namespace nx::vms::server::event

namespace nx::analytics::db { class AbstractEventsStorage; }
namespace nx::vms::server::time_sync { class TimeSyncManager; }

namespace nx::vms::server {
    class UnusedWallpapersWatcher;
    class LicenseWatcher;
    class VideoWallLicenseWatcher;
    class RootFileSystem;
    class Settings;
    class ServerTimeSyncManager;
    class UpdateManager;
} // namespace nx::vms::server

namespace nx::vms::server::nvr { class IService; }

namespace nx::vms::server::resource { class SharedContextPool; }
namespace nx::vms::server::camera { class ErrorProcessor; }

class QnMediaServerModule : public QObject, public /*mixin*/ QnInstanceStorage
{
    Q_OBJECT;

public:
    QnMediaServerModule(
        const nx::vms::server::CmdLineArguments* arguments = nullptr,
        std::unique_ptr<MSSettings> serverSettings = nullptr,
        QObject* parent = nullptr);
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

    const nx::vms::server::Settings& settings() const { return m_settings->settings(); }
    nx::vms::server::Settings* mutableSettings() { return m_settings->mutableSettings(); }
    nx::analytics::db::Settings analyticEventsStorageSettings() { return m_settings->analyticEventsStorage(); }

    QnStoragePluginFactory* storagePluginFactory() const;

    void syncRoSettings() const;
    nx::vms::server::UnusedWallpapersWatcher* unusedWallpapersWatcher() const;
    nx::vms::server::LicenseWatcher* licenseWatcher() const;
    nx::vms::server::VideoWallLicenseWatcher* videoWallLicenseWatcher() const;
    nx::vms::server::event::EventMessageBus* eventMessageBus() const;
    PluginManager* pluginManager() const;
    nx::vms::server::analytics::Manager* analyticsManager() const;
    nx::vms::server::analytics::EventRuleWatcher* analyticsEventRuleWatcher() const;
    nx::vms::server::analytics::SdkObjectFactory* sdkObjectFactory() const;

    nx::vms::server::resource::SharedContextPool* sharedContextPool() const;
    AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher() const;
    nx::analytics::db::AbstractEventsStorage* analyticsEventsStorage() const;
    nx::vms::server::UpdateManager* updateManager() const;
    QnDataProviderFactory* dataProviderFactory() const;
    QnResourceCommandProcessor* resourceCommandProcessor() const;

    QnResourcePool* resourcePool() const;
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;
    QnCameraHistoryPool* cameraHistoryPool() const;

    nx::vms::server::RootFileSystem* rootFileSystem() const;

    QnStorageManager* normalStorageManager() const;
    QnStorageManager* backupStorageManager() const;
    nx::vms::server::event::EventConnector* eventConnector() const;
    nx::vms::server::event::ExtendedRuleProcessor* eventRuleProcessor() const;
    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;
    QnGlobalSettings* globalSettings() const;
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
    nx::vms::server::camera::ErrorProcessor* cameraErrorProcessor() const;
    QnMediaServerResourceSearchers* resourceSearchers() const;
    QnPlatformAbstraction* platform() const;
    QnServerConnector* serverConnector () const;
    QnResourceStatusWatcher* statusWatcher() const;
    QnMdnsListener* mdnsListener() const;
    nx::utils::TimerManager* timerManager() const;
    nx::network::upnp::DeviceSearcher* upnpDeviceSearcher() const;
    nx::vms::server::hls::SessionPool* hlsSessionPool() const;
    nx::vms::server::network::MulticastAddressRegistry* multicastAddressRegistry() const;
    nx::vms::server::nvr::IService* nvrService() const;
    nx::vms::server::event::ServerRuntimeEventManager* serverRuntimeEventManager() const;

    void initializeP2PDownloader();

    QString metadataDatabaseDir() const;
    bool isStopping() const { return m_isStopping.load(); }
    void initOutgoingSocketCounter();

    nx::vms::server::analytics::AbstractIFrameSearchHelper* iframeSearchHelper();
private:
    void registerResourceDataProviders();
    /**
     * Returns absolute path to downloads directory.
     * It will create this directory if does not exist.
     */
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

    PluginManager* m_pluginManager = nullptr;
    nx::vms::server::UnusedWallpapersWatcher* m_unusedWallpapersWatcher = nullptr;
    nx::vms::server::LicenseWatcher* m_licenseWatcher = nullptr;
    nx::vms::server::VideoWallLicenseWatcher* m_videoWallLicenseWatcher = nullptr;
    nx::vms::server::event::EventMessageBus* m_eventMessageBus = nullptr;
    nx::vms::server::analytics::Manager* m_analyticsManager = nullptr;

    nx::vms::server::event::ExtendedRuleProcessor* m_eventRuleProcessor = nullptr;
    nx::vms::server::event::EventConnector* m_eventConnector = nullptr;
    nx::vms::server::analytics::EventRuleWatcher* m_analyticsEventRuleWatcher = nullptr;
    nx::vms::server::resource::SharedContextPool* m_sharedContextPool = nullptr;
    AbstractArchiveIntegrityWatcher* m_archiveIntegrityWatcher = nullptr;
    mutable boost::optional<std::chrono::milliseconds> m_lastRunningTimeBeforeRestart;
    nx::analytics::db::AbstractEventsStorage* m_analyticsEventsStorage = nullptr;
    std::unique_ptr<nx::vms::server::RootFileSystem> m_rootFileSystem;
    nx::vms::server::UpdateManager* m_updateManager = nullptr;
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
    nx::vms::server::camera::ErrorProcessor* m_cameraErrorProcessor = nullptr;
    QnServerConnector* m_serverConnector = nullptr;
    QnResourceStatusWatcher* m_statusWatcher = nullptr;

    QnPlatformAbstraction* m_platform = nullptr;
    std::unique_ptr<QnMdnsListener> m_mdnsListener;
    std::unique_ptr<nx::network::upnp::DeviceSearcher> m_upnpDeviceSearcher;
    std::unique_ptr<QnMediaServerResourceSearchers> m_resourceSearchers;
    nx::vms::server::analytics::SdkObjectFactory* m_sdkObjectFactory;
    nx::vms::server::hls::SessionPool* m_hlsSessionPool = nullptr;
    nx::vms::server::network::MulticastAddressRegistry* m_multicastAddressRegistry = nullptr;
    std::unique_ptr<nx::vms::server::nvr::IService> m_nvrService;
    StreamingChunkTranscoder* m_streamingChunkTranscoder = nullptr;
    nx::vms::server::analytics::IFrameSearchHelper* m_analyticsIFrameSearchHelper = nullptr;
    std::unique_ptr<nx::vms::server::event::ServerRuntimeEventManager> m_serverRuntimeEventManager;

    // When server stops, QnResourcePropertyDictionary is destroyed before QnResource objects
    // (at least some of them) because of unknown reasons. So QnResource can not make soap requests
    // in its destructor. To prevent them the special flag is added.
    // This behavior should be fixed in 4.2.
    std::atomic<bool> m_isStopping{ false };
};
