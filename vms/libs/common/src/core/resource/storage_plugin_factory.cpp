#include "storage_plugin_factory.h"

#include <nx/utils/log/log.h>

QnStoragePluginFactory::StorageFactory QnStoragePluginFactory::s_factory = nullptr;

QnStoragePluginFactory::QnStoragePluginFactory(QObject* parent):
    QObject(parent),
    m_defaultFactory()
{
}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

void QnStoragePluginFactory::registerStoragePlugin(
    const QString& protocol,
    const StorageFactory& factory,
    bool isDefaultProtocol)
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

void QnStoragePluginFactory::setDefault(StorageFactory factory)
{
    s_factory = factory;
}

QnStorageResource *QnStoragePluginFactory::createStorage(
    QnCommonModule* commonModule,
    const QString &url,
    bool useDefaultForUnknownPrefix)
{
    if (s_factory)
        return s_factory(commonModule, url);

    int index = url.indexOf(lit("://"));
    if (index == -1)
        return m_defaultFactory ? m_defaultFactory(commonModule, url) : nullptr;

    QString protocol = url.left(index);
    if (m_factoryByProtocol.contains(protocol))
    {
        QnStorageResource *ret = m_factoryByProtocol.value(protocol)(commonModule, url);
        if (ret == nullptr)
        {
            NX_ERROR(this, "Failed to create storage for url %1", url);
            return nullptr;
        }
        ret->setStorageType(protocol);
        return ret;
    } else {
        if (useDefaultForUnknownPrefix)
            return m_defaultFactory ? m_defaultFactory(commonModule, url) : nullptr;
        return nullptr;
    }
}
