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

class QnMediaServerModule:
    public QObject,
    public QnInstanceStorage,
    public Singleton<QnMediaServerModule>
{
    Q_OBJECT;
public:
    QnMediaServerModule(
        const QString& enforcedMediatorEndpoint = QString(),
        QObject *parent = nullptr);
    virtual ~QnMediaServerModule();

    using Singleton<QnMediaServerModule>::instance;
    using QnInstanceStorage::instance;

    StreamingChunkCache* streamingChunkCache() const;
    QnCommonModule* commonModule() const;
private:
    QnCommonModule* m_commonModule;

    StreamingChunkCache* m_streamingChunkCache;

    std::shared_ptr<QnStorageManager> m_normalStorageManager;
    std::shared_ptr<QnStorageManager> m_backupStorageManager;
};

#define qnServerModule QnMediaServerModule::instance()