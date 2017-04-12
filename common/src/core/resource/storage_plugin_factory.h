#ifndef __STORAGE_PLUGIN_FACTORY_RESOURCE_H__
#define __STORAGE_PLUGIN_FACTORY_RESOURCE_H__

#include <functional>

#include <nx/utils/thread/mutex.h>

#include "storage_resource.h"

class QnStoragePluginFactory
{
public:
    typedef std::function<QnStorageResource *(QnCommonModule*, const QString &)> StorageFactory;

    QnStoragePluginFactory();
    virtual ~QnStoragePluginFactory();
    static QnStoragePluginFactory *instance();

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

#endif // __STORAGE_PLUGIN_FACTORY_RESOURCE_H__