#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>
#include <common/common_module.h>

class QnCommonModule;
class StreamingChunkCache;
class QnStorageManager;

class QnMediaServerModule:
    public QObject,
    public QnInstanceStorage,
    public Singleton<QnMediaServerModule>,
    public QnCommonModuleAware
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
private:
    StreamingChunkCache* m_streamingChunkCache = nullptr;

    QnStorageManager* m_normalStorageManager = nullptr;
    QnStorageManager* m_backupStorageManager = nullptr;
};
