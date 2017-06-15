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

namespace nx { namespace mediaserver { class UnusedWallpapersWatcher; } }

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
    nx::mediaserver::UnusedWallpapersWatcher* m_unusedWallpapersWatcher = nullptr;
};

#define qnServerModule QnMediaServerModule::instance()