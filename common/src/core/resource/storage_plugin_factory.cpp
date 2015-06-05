#include "storage_plugin_factory.h"

Q_GLOBAL_STATIC(QnStoragePluginFactory, QnStoragePluginFactory_instance)

QnStoragePluginFactory::QnStoragePluginFactory(): m_defaultFactory(0)
{
}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

QnStoragePluginFactory *QnStoragePluginFactory::instance()
{
    return QnStoragePluginFactory_instance();
}

void QnStoragePluginFactory::registerStoragePlugin(const QString &protocol, const StorageResourceFactory &factory, bool isDefaultProtocol)
{
    m_factoryByProtocol.insert(protocol, factory);
    if (isDefaultProtocol)
        m_defaultFactory = factory;
}

QnStorageResource *QnStoragePluginFactory::createStorage(const QString &url, bool useDefaultForUnknownPrefix)
{
    int index = url.indexOf(lit("://"));
    if (index == -1) 
        return m_defaultFactory ? m_defaultFactory() : NULL;
    
    QString protocol = url.left(index);
    if (m_factoryByProtocol.contains(protocol)) {
        return m_factoryByProtocol.value(protocol)();
    } else {
        if (useDefaultForUnknownPrefix)
            return m_defaultFactory ? m_defaultFactory() : NULL;
        else
            return NULL;
    }
}
