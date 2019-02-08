#pragma once

#include <functional>

#include <nx/utils/thread/mutex.h>

#include "storage_resource.h"

class QnStoragePluginFactory: public QObject
{
    Q_OBJECT
public:
    typedef std::function<QnStorageResource *(QnCommonModule*, const QString&)> StorageFactory;

    QnStoragePluginFactory(QObject* parent = nullptr);
    virtual ~QnStoragePluginFactory();

    bool existsFactoryForProtocol(const QString &protocol);
    void registerStoragePlugin(const QString &protocol, const StorageFactory &factory, bool isDefaultProtocol = false);
    QnStorageResource *createStorage(
        QnCommonModule* commonModule,
        const QString &url,
        bool useDefaultForUnknownPrefix = true);

private:
    QHash<QString, StorageFactory> m_factoryByProtocol;
    StorageFactory m_defaultFactory;
    QnMutex m_mutex; // TODO: #vasilenko this mutex is not used, is it intentional?
};
