#include "storage_plugin_factory.h"

Q_GLOBAL_STATIC(QnStoragePluginFactory, QnStoragePluginFactory_instance)

QnStoragePluginFactory::QnStoragePluginFactory(): m_defaultFactory()
{
}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

QnStoragePluginFactory *QnStoragePluginFactory::instance()
{
    return QnStoragePluginFactory_instance();
}

void QnStoragePluginFactory::registerStoragePlugin(const QString &protocol, const StorageFactory &factory, bool isDefaultProtocol)
{
    QnMutexLocker lock(&m_mutex);
    m_factoryByProtocol.insert(protocol, factory);
    if (isDefaultProtocol)
        m_defaultFactory = factory;
}

bool QnStoragePluginFactory::existsFactoryForProtocol(const QString &protocol)
{
    QnMutexLocker lock(&m_mutex);
    return m_factoryByProtocol.find(protocol) == m_factoryByProtocol.end() ? false : true;
}

QnStorageResource *QnStoragePluginFactory::createStorage(
    QnCommonModule* commonModule,
    const QString &url,
    bool useDefaultForUnknownPrefix)
{
    int index = url.indexOf(lit("://"));
    if (index == -1)
        return m_defaultFactory ? m_defaultFactory(commonModule, url) : NULL;

    QString protocol = url.left(index);
    if (m_factoryByProtocol.contains(protocol)) {
        QnStorageResource *ret = m_factoryByProtocol.value(protocol)(commonModule, url);
        ret->setStorageType(protocol);
        return ret;
    } else {
        if (useDefaultForUnknownPrefix)
            return m_defaultFactory ? m_defaultFactory(commonModule, url) : NULL;
        else
            return NULL;
    }
}
