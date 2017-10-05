#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>
#include <common/common_module.h>

class QnCommonModule;
class StreamingChunkCache;
class QnStorageManager;
class QnStaticCommonModule;
class QnStorageDbPool;
class MSSettings;
class PluginManager;
class CommonPluginContainer;
class QThread;

namespace nx { 
namespace mediaserver {

class UnusedWallpapersWatcher;
class LicenseWatcher;
class RootTool;

namespace metadata {

class ManagerPool;
class EventRuleWatcher;

} // namespace metadata

} // namespace mediaserver
} // namespace nx

class QnMediaServerModule:
    public QObject,
    public QnInstanceStorage,
    public Singleton<QnMediaServerModule>
{
    Q_OBJECT;
public:
    QnMediaServerModule(
        const QString& enforcedMediatorEndpoint = QString(),
        const QString& roSettingsPath = QString(),
        const QString& rwSettingsPath = QString(),
        QObject *parent = nullptr);
    virtual ~QnMediaServerModule();

    using Singleton<QnMediaServerModule>::instance;
    using QnInstanceStorage::instance;

    StreamingChunkCache* streamingChunkCache() const;
    QnCommonModule* commonModule() const;

    QSettings* roSettings() const;
    QSettings* runTimeSettings() const;
    MSSettings* settings() const;
    nx::mediaserver::UnusedWallpapersWatcher* unusedWallpapersWatcher() const;
    nx::mediaserver::LicenseWatcher* licenseWatcher() const;
    PluginManager* pluginManager() const;
    nx::mediaserver::metadata::ManagerPool* metadataManagerPool() const;
    nx::mediaserver::metadata::EventRuleWatcher* metadataRuleWatcher() const;

    void initializeRootTool();
    nx::mediaserver::RootTool* rootTool() const;

private:
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
    QThread* m_metadataManagerPoolThread = nullptr;
    std::unique_ptr<nx::mediaserver::RootTool> m_rootTool;
};

#define qnServerModule QnMediaServerModule::instance()
