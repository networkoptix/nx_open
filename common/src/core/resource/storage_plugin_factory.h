#ifndef __STORAGE_PLUGIN_FACTORY_RESOURCE_H__
#define __STORAGE_PLUGIN_FACTORY_RESOURCE_H__

class QnStoragePluginFactory
{
public:
    typedef QnStorageResource *(*StorageResourceFactory)();

    QnStoragePluginFactory();
    virtual ~QnStoragePluginFactory();
    static QnStoragePluginFactory *instance();

    void registerStoragePlugin(const QString &protocol, const StorageResourceFactory &factory, bool isDefaultProtocol = false);
    QnStorageResource *createStorage(const QString &url, bool useDefaultForUnknownPrefix = true);

private:
    QHash<QString, StorageResourceFactory> m_factoryByProtocol;
    StorageResourceFactory m_defaultFactory;
    QMutex m_mutex; // TODO: #vasilenko this mutex is not used, is it intentional?
};

#endif // __STORAGE_PLUGIN_FACTORY_RESOURCE_H__