#include "storage_resource.h"
#include "utils/common/util.h"
#include "../dataprovider/media_streamdataprovider.h"


// ------------------------------ QnStorageResource -------------------------------

QnStorageResource::QnStorageResource():
    m_writedSpace(0)
{
}

QnStorageResource::~QnStorageResource()
{
}


void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);
}

qint64 QnStorageResource::getWritedSpace() const
{
    QMutexLocker lock(&m_writedSpaceMtx);
    return m_writedSpace;
}

void QnStorageResource::addWritedSpace(qint64 value)
{
    QMutexLocker lock(&m_writedSpaceMtx);
    m_writedSpace += value;
}

// ---------------------------- QnStoragePluginFactory ------------------------------

Q_GLOBAL_STATIC(QnStoragePluginFactory, inst)

QnStoragePluginFactory::QnStoragePluginFactory(): m_defaultStoragePlugin(0)
{
}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

QnStoragePluginFactory* QnStoragePluginFactory::instance()
{
    return inst();
}

void QnStoragePluginFactory::registerStoragePlugin(const QString& name, StorageTypeInstance pluginInst, bool isDefaultProtocol)
{
    m_storageTypes.insert(name, pluginInst);
    if (isDefaultProtocol)
        m_defaultStoragePlugin = pluginInst;
}

QnStorageResource* QnStoragePluginFactory::createStorage(const QString& storageType, bool useDefaultForUnknownPrefix)
{
    int prefix = storageType.indexOf(QLatin1String("://"));
    if (prefix == -1) 
        return m_defaultStoragePlugin ? m_defaultStoragePlugin() : 0;
    
    QString protocol = storageType.left(prefix);
    if (m_storageTypes.contains(protocol))
        return m_storageTypes.value(protocol)();
    else {
        if (useDefaultForUnknownPrefix)
            return m_defaultStoragePlugin ? m_defaultStoragePlugin() : 0;
        else
            return 0;
    }
}
