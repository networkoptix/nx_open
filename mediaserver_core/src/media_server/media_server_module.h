#pragma once

#include <QtCore/QObject>

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

namespace nx {

namespace analytics {
namespace storage {
class AbstractEventsStorage;
}
} // namespace analytics

namespace time_sync { 
class TimeSyncManager; 
} // namespace time_sync

namespace mediaserver {

class UnusedWallpapersWatcher;
class LicenseWatcher;
class RootTool;
class Settings;
class ServerTimeSyncManager;

namespace updates2 {
class ServerUpdates2Manager;
}

namespace metadata {

class ManagerPool;
class EventRuleWatcher;

} // namespace metadata

namespace resource {

class SharedContextPool;

} // namespace resource

} // namespace mediaserver
} // namespace nx

class QnMediaServerModule : public QObject,
                            public QnInstanceStorage,
                            public Singleton<QnMediaServerModule>
{
    Q_OBJECT;

  public:
    QnMediaServerModule(const QString& enforcedMediatorEndpoint = QString(),
        const QString& roSettingsPath = QString(),
        const QString& rwSettingsPath = QString(),
        QObject* parent = nullptr);
    virtual ~QnMediaServerModule();

    using Singleton<QnMediaServerModule>::instance;
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
    PluginManager* pluginManager() const;
    nx::mediaserver::metadata::ManagerPool* metadataManagerPool() const;
    nx::mediaserver::metadata::EventRuleWatcher* metadataRuleWatcher() const;
    nx::mediaserver::resource::SharedContextPool* sharedContextPool() const;
    AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher() const;
    nx::analytics::storage::AbstractEventsStorage* analyticsEventsStorage() const;
    nx::mediaserver::updates2::ServerUpdates2Manager* updates2Manager() const;
    QnDataProviderFactory* dataProviderFactory() const;
    QnResourceCommandProcessor* resourceCommandProcessor() const;

    QnResourcePool* resourcePool() const;
    QnResourcePropertyDictionary* propertyDictionary() const;
    QnCameraHistoryPool* cameraHistoryPool() const;

    nx::mediaserver::RootTool* rootTool() const;

  private:
    void registerResourceDataProviders();
    QDir downloadsDirectory() const;

    QnCommonModule* m_commonModule;
    MSSettings* m_settings;
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
    nx::mediaserver::metadata::ManagerPool* m_metadataManagerPool = nullptr;
    nx::mediaserver::metadata::EventRuleWatcher* m_metadataRuleWatcher = nullptr;
    nx::mediaserver::resource::SharedContextPool* m_sharedContextPool = nullptr;
    AbstractArchiveIntegrityWatcher* m_archiveIntegrityWatcher;
    mutable boost::optional<std::chrono::milliseconds> m_lastRunningTimeBeforeRestart;
    std::unique_ptr<nx::analytics::storage::AbstractEventsStorage> m_analyticsEventsStorage;
    std::unique_ptr<nx::mediaserver::RootTool> m_rootTool;
    nx::mediaserver::updates2::ServerUpdates2Manager* m_updates2Manager;
    QScopedPointer<QnDataProviderFactory> m_resourceDataProviderFactory;
    QScopedPointer<QnResourceCommandProcessor> m_resourceCommandProcessor;
};

#define qnServerModule QnMediaServerModule::instance()
